#include "Buffer.h"
#include "CommandBuffer.h"
#include "DebugSubpass.h"
#include "FirstPersonCamera.h"
#include "FlingVulkan.h"
#include "FrameBuffer.h"
#include "GraphicsHelpers.h"
#include "GraphicsPipeline.h"
#include "Logger.h"
#include "LogicalDevice.h"
#include "Material.h"
#include "Model.h"
#include "PhyscialDevice.h"
#include "SwapChain.h"
#include "UniformBufferObject.h"

#define FRAME_BUF_DIM 2048

namespace Fling
{
    DebugSubpass::DebugSubpass(const LogicalDevice* t_Dev,
                               const Swapchain* t_Swap,
                               VkRenderPass t_GlobalRenderPass,
                               FirstPersonCamera* t_Cam,
                               std::shared_ptr<Fling::Shader> t_Vert,
                               std::shared_ptr<Fling::Shader> t_Frag)
        : Subpass(t_Dev, t_Swap, t_Vert, t_Frag), m_GlobalRenderPass(t_GlobalRenderPass), m_Camera(t_Cam)
    {
        PrepareAttachments();
    }

    DebugSubpass::~DebugSubpass()
    {
    }

    void DebugSubpass::Draw(CommandBuffer& t_CmdBuf,
                            uint32 t_ActiveFrameInFlight,
                            const RenderSnapshot& t_Snapshot,
                            float DeltaTime)
    {
        ReleaseRetiredMeshResources();
        SyncProxies(t_Snapshot);

        // Invert the project value to match the proper coordinate space compared to OpenGL
        m_Ubo.Projection = m_Camera->GetProjectionMatrix();
        m_Ubo.Projection[1][1] *= -1.0f;
        VkDeviceSize offsets[1] = {0};

        for (size_t i = 0; i < t_Snapshot.Meshes.size() && i < m_ItemSlots.size(); ++i)
        {
            const uint32 ItemSlot = m_ItemSlots[i];
            const GpuMesh& Mesh = m_GpuMeshes[ItemSlot];
            if (!Mesh.Drawn || Mesh.MeshModel == nullptr)
            {
                continue;
            }
            const FrameResources& Frame = m_FrameResources[ItemSlot];
            const uint32 FrameSlot = FrameSync::Slot();
            Fling::Model* Model = Mesh.MeshModel;

            m_Ubo.Model = t_Snapshot.Meshes[i].ResolveWorld(t_Snapshot.BlendAlpha);
            memcpy(Frame.Ubos[FrameSlot]->m_MappedMem, &m_Ubo, sizeof(m_Ubo));

            vkCmdBindDescriptorSets(t_CmdBuf.GetHandle(),
                                    VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    m_GraphicsPipeline->GetPipelineLayout(),
                                    0,
                                    1,
                                    &Frame.Sets[FrameSlot],
                                    0,
                                    nullptr);

            vkCmdBindPipeline(t_CmdBuf.GetHandle(), VK_PIPELINE_BIND_POINT_GRAPHICS, m_GraphicsPipeline->GetPipeline());

            VkBuffer vertexBuffers[1] = {Model->GetVertexBuffer()->GetVkBuffer()};
            // Render the mesh
            vkCmdBindVertexBuffers(t_CmdBuf.GetHandle(), 0, 1, vertexBuffers, offsets);
            vkCmdBindIndexBuffer(
                t_CmdBuf.GetHandle(), Model->GetIndexBuffer()->GetVkBuffer(), 0, Model->GetIndexType());
            vkCmdDrawIndexed(t_CmdBuf.GetHandle(), Model->GetIndexCount(), 1, 0, 0, 0);
        }
    }

    void DebugSubpass::SyncProxies(const RenderSnapshot& t_Snapshot)
    {
        MeshProxyTable::SyncResult Result = m_Proxies.Sync(t_Snapshot.Meshes);
        m_ItemSlots = std::move(Result.ItemSlots);
        m_GpuMeshes.resize(m_Proxies.Capacity());
        m_FrameResources.resize(m_Proxies.Capacity());

        for (const uint32 Slot : Result.Removed)
        {
            RetireGpuMesh(Slot);
        }

        auto Prepare = [&](const uint32 Slot)
        {
            RetireGpuMesh(Slot);
            GpuMesh& Mesh = m_GpuMeshes[Slot];
            const bool bHasAssets = ResolveMeshAssets(m_Proxies.At(Slot), Mesh);
            Mesh.Drawn = bHasAssets && Mesh.MeshMaterial->GetType() == Material::Type::Debug;
            if (!Mesh.Drawn)
            {
                Mesh = GpuMesh{};
                return;
            }

            const VkDeviceSize BufferSize = sizeof(DebugUBO);
            FrameResources& Frame = m_FrameResources[Slot];
            for (uint32 f = 0; f < FrameSync::FramesInFlight; ++f)
            {
                Frame.Ubos[f] = new Buffer(BufferSize,
                                           VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                           VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
                Frame.Ubos[f]->MapMemory(BufferSize);
                CreateMeshDescriptorSet(Frame.Sets[f], Frame.Ubos[f]);
            }
            // The first frame's resources stand for the mesh in the checks of the other code
            Mesh.Ubo = Frame.Ubos[0];
            Mesh.Set = Frame.Sets[0];
        };

        for (const uint32 Slot : Result.Added)
        {
            Prepare(Slot);
        }
        for (const uint32 Slot : Result.Changed)
        {
            Prepare(Slot);
        }
    }

    void DebugSubpass::RetireGpuMesh(const uint32 t_Slot)
    {
        FrameResources& Frame = m_FrameResources[t_Slot];
        for (uint32 f = 0; f < FrameSync::FramesInFlight; ++f)
        {
            if (Frame.Sets[f] != VK_NULL_HANDLE)
            {
                m_RetiredDescriptorSets.Add(Frame.Sets[f]);
            }
            if (Frame.Ubos[f] != nullptr)
            {
                m_RetiredUniformBuffers.Add(Frame.Ubos[f]);
            }
        }
        Frame = FrameResources{};
        m_GpuMeshes[t_Slot] = GpuMesh{};
    }

    void DebugSubpass::ReleaseRetiredMeshResources(const bool t_All)
    {
        auto FreeSet = [this](const VkDescriptorSet t_Set)
        {
            if (m_DescriptorPool != VK_NULL_HANDLE)
            {
                VK_CHECK_RESULT(vkFreeDescriptorSets(m_Device->GetVkDevice(), m_DescriptorPool, 1, &t_Set));
            }
        };
        auto FreeBuffer = [](Buffer* t_Buffer)
        {
            delete t_Buffer;
        };
        if (t_All)
        {
            m_RetiredDescriptorSets.ReleaseAll(FreeSet);
            m_RetiredUniformBuffers.ReleaseAll(FreeBuffer);
        }
        else
        {
            m_RetiredDescriptorSets.Release(FreeSet);
            m_RetiredUniformBuffers.Release(FreeBuffer);
        }
    }

    void DebugSubpass::CreateDescriptorSets(VkDescriptorPool t_Pool)
    {
    }

    void DebugSubpass::CreateMeshDescriptorSet(VkDescriptorSet& t_Set, Buffer* t_Ubo)
    {
        if (t_Set == VK_NULL_HANDLE)
        {
            VkDescriptorSetLayout layout = m_GraphicsPipeline->GetDescriptorSetLayout();
            VkDescriptorSetAllocateInfo allocInfo = {};
            allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
            // If we have specified a specific pool then use that, otherwise use the one on the mesh
            allocInfo.descriptorPool = m_DescriptorPool;
            allocInfo.descriptorSetCount = 1;
            allocInfo.pSetLayouts = &layout;

            VK_CHECK_RESULT(vkAllocateDescriptorSets(m_Device->GetVkDevice(), &allocInfo, &t_Set));
        }

        std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
            // 0: UBO
            Initializers::WriteDescriptorSetUniform(t_Ubo, t_Set, 0),
        };

        vkUpdateDescriptorSets(m_Device->GetVkDevice(),
                               static_cast<uint32>(writeDescriptorSets.size()),
                               writeDescriptorSets.data(),
                               0,
                               nullptr);
    }

    void DebugSubpass::PrepareAttachments()
    {
        // Create the descriptor pool for off screen things
        const uint32 DescriptorCount = 100 * m_SwapChain->GetImageViewCount() * FrameSync::FramesInFlight;

        std::vector<VkDescriptorPoolSize> poolSizes = {
            Initializers::DescriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, DescriptorCount),
        };

        VkDescriptorPoolCreateInfo poolInfo = {};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        // A mesh frees its set when it leaves the snapshot
        poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        poolInfo.poolSizeCount = static_cast<uint32>(poolSizes.size());
        poolInfo.pPoolSizes = poolSizes.data();
        poolInfo.maxSets = DescriptorCount;

        if (vkCreateDescriptorPool(m_Device->GetVkDevice(), &poolInfo, nullptr, &m_DescriptorPool) != VK_SUCCESS)
        {
            F_LOG_FATAL("Failed to create descriptor pool");
        }
    }

    void DebugSubpass::CreateGraphicsPipeline()
    {
        // Draw in wireframe mode
        m_GraphicsPipeline->m_RasterizationState = Initializers::PipelineRasterizationStateCreateInfo(
            VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE);

        // Create it otherwise with defaults
        m_GraphicsPipeline->CreateGraphicsPipeline(m_GlobalRenderPass, nullptr);
    }

    void DebugSubpass::CleanUp()
    {
        // Called after the device is idle
        for (uint32 Slot = 0; Slot < m_GpuMeshes.size(); ++Slot)
        {
            RetireGpuMesh(Slot);
        }
        m_GpuMeshes.clear();
        m_FrameResources.clear();
        m_Proxies.Clear();
        m_ItemSlots.clear();
        ReleaseRetiredMeshResources(true);

        if (m_DescriptorPool != VK_NULL_HANDLE)
        {
            vkDestroyDescriptorPool(m_Device->GetVkDevice(), m_DescriptorPool, nullptr);
            m_DescriptorPool = VK_NULL_HANDLE;
        }
    }
} // namespace Fling
