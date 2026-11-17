#include "Buffer.h"
#include "CommandBuffer.h"
#include "FirstPersonCamera.h"
#include "FlingIntegers.h"
#include "FlingVulkan.h"
#include "FrameBuffer.h"
#include "GraphicsHelpers.h"
#include "GraphicsPipeline.h"
#include "Logger.h"
#include "LogicalDevice.h"
#include "Material.h"
#include "Model.h"
#include "OffscreenSubpass.h"
#include "PhyscialDevice.h"
#include "PreCullView.h"
#include "Profiler.h"
#include "SwapChain.h"
#include "UniformBufferObject.h"

#include <algorithm>
#include <cstring>

namespace Fling
{
    namespace
    {
        /** The binding of the world matrix buffer in mrt.vert */
        constexpr uint32 WorldBufferBinding = 6;
    } // namespace

    OffscreenSubpass::OffscreenSubpass(const LogicalDevice* t_Dev,
                                       const Swapchain* t_Swap,
                                       FirstPersonCamera* t_Cam,
                                       std::shared_ptr<Fling::Shader> t_Vert,
                                       std::shared_ptr<Fling::Shader> t_Frag,
                                       std::shared_ptr<Fling::Shader> t_HizDownsample,
                                       std::shared_ptr<Fling::Shader> t_HizCull)
        : Subpass(t_Dev, t_Swap, t_Vert, t_Frag), m_Camera(t_Cam)
    {
        // Set the clear values for the G Buffer
        m_ClearValues.resize(6);
        m_ClearValues[0].color = m_ClearValues[1].color = {{0.0f, 0.0f, 0.0f, 0.0f}};
        m_ClearValues[2].color = m_ClearValues[3].color = m_ClearValues[4].color = {{0.0f, 0.0f, 0.0f, 0.0f}};
        m_ClearValues[5].depthStencil = {1.0f, 0};

        // Build offscreen semaphores -------
        m_OffscreenSemaphores.resize(VkConfig::MAX_FRAMES_IN_FLIGHT);
        for (int32 i = 0; i < VkConfig::MAX_FRAMES_IN_FLIGHT; i++)
        {
            m_OffscreenSemaphores[i] = GraphicsHelpers::CreateSemaphore(m_Device->GetVkDevice());
        }

        GraphicsHelpers::CreateCommandPool(&m_CommandPool, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

        // Build offscreen command buffers
        m_OffscreenCmdBufs.resize(m_SwapChain->GetImageCount());
        for (size_t i = 0; i < m_OffscreenCmdBufs.size(); ++i)
        {
            m_OffscreenCmdBufs[i] = new Fling::CommandBuffer(m_Device, m_CommandPool);
            assert(m_OffscreenCmdBufs[i] != nullptr);
        }
        m_RecordedDrawListVersions.resize(m_OffscreenCmdBufs.size(), 0);

        m_Uploader = std::make_unique<FrameUploader>(m_Device);

        // One camera UBO is enough: it is in GPU memory, and each frame's upload writes it after the
        // frame before has finished on the GPU
        VkDeviceSize CameraBufferSize = sizeof(OffscreenCameraUBO);
        m_CameraUboBuffer = new Buffer(CameraBufferSize,
                                       VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                       VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        // Tell the Vulkan app that the draw command buffers need to WAIT on this offscreen semaphore
        PrepareAttachments();

        if (t_HizDownsample && t_HizCull)
        {
            m_Occlusion = std::make_unique<OcclusionCuller>(m_Device, m_Uploader.get(), t_HizDownsample, t_HizCull);
            SetOcclusionDepth();
        }
    }

    OffscreenSubpass::~OffscreenSubpass()
    {
        m_Occlusion.reset();
        // Destroy any allocated semaphores
        for (size_t i = 0; i < m_OffscreenSemaphores.size(); ++i)
        {
            vkDestroySemaphore(m_Device->GetVkDevice(), m_OffscreenSemaphores[i], nullptr);
        }

        // Clean up command buffers we used
        for (CommandBuffer* CmdBuf : m_OffscreenCmdBufs)
        {
            delete CmdBuf;
            CmdBuf = nullptr;
        }
        m_OffscreenCmdBufs.clear();

        vkDestroyCommandPool(m_Device->GetVkDevice(), m_CommandPool, nullptr);

        delete m_OffscreenFrameBuf;
        m_OffscreenFrameBuf = nullptr;

        delete m_CameraUboBuffer;
        m_CameraUboBuffer = nullptr;

        delete m_DrawCommands;
        m_DrawCommands = nullptr;

        delete m_WorldBuffer;
        m_WorldBuffer = nullptr;
    }

    void OffscreenSubpass::Draw(CommandBuffer& t_CmdBuf,
                                uint32 t_ActiveSwapImage,
                                const RenderSnapshot& t_Snapshot,
                                float DeltaTime)
    {
        assert(m_GraphicsPipeline);
        // Don't use the given command buffer, instead build the OFFSCREEN command buffer
        CommandBuffer* OffscreenCmdBuf = m_OffscreenCmdBufs[t_ActiveSwapImage];
        assert(OffscreenCmdBuf);

        FLING_PROFILE_SCOPE("Offscreen.Draw");
        m_Uploader->Begin();
        // The resources of destroyed meshes go when the frames that used them are done
        ReleaseRetiredMeshResources();

        const bool bOcclusion = OcclusionActive();
        if (m_Occlusion)
        {
            m_Occlusion->ReleaseRetired();
        }
        // The GPU counted the hidden meshes of a frame that finished (a few frames ago)
        m_OccludedCount = bOcclusion ? m_Occlusion->ReadOccludedCount() : 0;
        m_GpuFrustumCulled = bOcclusion ? m_Occlusion->ReadFrustumCulledCount() : 0;
        if (m_Occlusion)
        {
            m_Occlusion->QueueStatsReadback();
        }
        const bool bGpuBlend = GpuBlendActive();
        // The culler leaves the meshes that the GPU blends to the GPU (it has no matrix for them)
        m_Culler.SetDeferBlended(bGpuBlend);
        if (bOcclusion != m_RecordedOcclusion)
        {
            // The recordings hold either one pass or the two phase passes
            InvalidateCommandBuffers();
            m_RecordedOcclusion = bOcclusion;
        }

        // The meshes in this frame's snapshot: new ones get a proxy, gone ones release theirs
        {
            FLING_PROFILE_SCOPE("Offscreen.SyncProxies");
            SyncProxies(t_Snapshot);
        }

        // The camera moves on most frames, but it is only one small write
        OffscreenCameraUBO CameraUBO = {};
        // Invert the project value to match the proper coordinate space compared to OpenGL
        CameraUBO.Projection = m_Camera->GetProjectionMatrix();
        CameraUBO.Projection[1][1] *= -1.0f;
        CameraUBO.View = m_Camera->GetViewMatrix();
        memcpy(m_Uploader->Stage(*m_CameraUboBuffer, 0, sizeof(CameraUBO)), &CameraUBO, sizeof(CameraUBO));

        // The view volume of exactly the matrices the shader uses this frame
        m_CameraViewProjection = CameraUBO.Projection * CameraUBO.View;
        {
            FLING_PROFILE_SCOPE("Offscreen.ResolveWorlds");
            ResolveWorlds(t_Snapshot);
        }
        {
            FLING_PROFILE_SCOPE("Offscreen.Cull");
            CullMeshes(t_Snapshot);
        }

        {
            FLING_PROFILE_SCOPE("Offscreen.UpdateUbos");
            UpdateMeshUniformBuffers(t_Snapshot);
        }
        if (bOcclusion)
        {
            FLING_PROFILE_SCOPE("Offscreen.OcclusionSlots");
            // The GPU writes the draw commands, from the slot data and this frame's camera
            m_Occlusion->SetWorlds(m_WorldBuffer);
            if (m_Occlusion->EnsureCapacity(m_DrawCommandCapacity, m_DrawCommands))
            {
                InvalidateCommandBuffers();
            }
            WriteOcclusionSlots(t_Snapshot);
            m_Occlusion->WriteParams(
                m_CameraViewProjection, m_View, static_cast<uint32>(m_GpuMeshes.size()), t_Snapshot.BlendAlpha);
        }
        else
        {
            FLING_PROFILE_SCOPE("Offscreen.WriteDrawCommands");
            WriteDrawCommands();
        }

        // The command buffer holds only draw calls and descriptor bindings, not
        // UBO data. Keep the last recording if the draw list did not change.
        if (m_RecordedDrawListVersions[t_ActiveSwapImage] != m_DrawListVersion)
        {
            FLING_PROFILE_SCOPE("Offscreen.RecordCommands");
            BuildOffscreenCommandBuffer(*OffscreenCmdBuf);
            m_RecordedDrawListVersions[t_ActiveSwapImage] = m_DrawListVersion;
        }

        m_UploadCmd = m_Uploader->Record();
    }

    void OffscreenSubpass::SyncProxies(const RenderSnapshot& t_Snapshot)
    {
        MeshProxyTable::SyncResult Result = m_Proxies.Sync(t_Snapshot.Meshes);
        m_ItemSlots = std::move(Result.ItemSlots);
        m_GpuMeshes.resize(m_Proxies.Capacity());
        EnsureWorldCapacity(m_Proxies.Capacity());

        // Removed first: a removed slot can be used again by an added mesh
        for (const uint32 Slot : Result.Removed)
        {
            RetireGpuMesh(m_GpuMeshes[Slot]);
            m_Culler.ClearSlot(Slot);
        }

        auto Prepare = [&](const uint32 Slot)
        {
            GpuMesh& Mesh = m_GpuMeshes[Slot];
            const bool bHasAssets = ResolveMeshAssets(m_Proxies.At(Slot), Mesh);
            Mesh.Drawn = bHasAssets && Mesh.MeshMaterial->GetType() == Material::Type::Default;
            if (!Mesh.Drawn)
            {
                // Another pass draws it (a debug material), or there is no model
                RetireGpuMesh(Mesh);
                m_Culler.SetSlot(Slot, false, Aabb{});
                return;
            }

            if (Mesh.Ubo == nullptr)
            {
                const VkDeviceSize BufferSize = sizeof(OffscreenUBO);
                Mesh.Ubo = new Buffer(BufferSize,
                                      VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
                Mesh.Ubo->MapMemory(BufferSize);
            }
            // The slot never changes while the mesh lives: the vertex shader finds its matrix with it
            OffscreenUBO MeshUBO = {};
            MeshUBO.Slot = Slot;
            std::memcpy(Mesh.Ubo->m_MappedMem, &MeshUBO, sizeof(MeshUBO));
            CreateMeshDescriptorSet(Mesh);
            Mesh.UploadedVersion = 0;
            // The same test the recording uses, so the culler counts only what gets a draw call
            m_Culler.SetSlot(Slot, IsDrawable(Mesh), Mesh.MeshModel->GetLocalBounds());
        };

        for (const uint32 Slot : Result.Added)
        {
            Prepare(Slot);
        }
        for (const uint32 Slot : Result.Changed)
        {
            // The descriptor set may be in a recorded command buffer, so take a new one
            RetireGpuMesh(m_GpuMeshes[Slot]);
            Prepare(Slot);
        }

        if (Result.DrawListChanged())
        {
            InvalidateCommandBuffers();
        }
        EnsureDrawCommandCapacity();
    }

    void OffscreenSubpass::RetireGpuMesh(GpuMesh& t_Mesh)
    {
        if (t_Mesh.Set != VK_NULL_HANDLE)
        {
            m_RetiredDescriptorSets.Add(t_Mesh.Set);
        }
        if (t_Mesh.Ubo != nullptr)
        {
            m_RetiredUniformBuffers.Add(t_Mesh.Ubo);
        }
        t_Mesh = GpuMesh{};
    }

    void OffscreenSubpass::ResolveWorlds(const RenderSnapshot& t_Snapshot)
    {
        // A mesh between two ticks has no matrix yet: make it from its two ends and this frame's factor
        // (With the GPU blend, the cull shader makes it: this keeps no matrix for that mesh.)
        const bool bGpuBlend = GpuBlendActive();
        m_Worlds.resize(t_Snapshot.Meshes.size());
        for (size_t i = 0; i < t_Snapshot.Meshes.size(); ++i)
        {
            const MeshDrawItem& Item = t_Snapshot.Meshes[i];
            if (bGpuBlend && Item.HasBlend)
            {
                continue;
            }
            m_Worlds[i] = Item.ResolveWorld(t_Snapshot.BlendAlpha);
        }
    }

    void OffscreenSubpass::CullMeshes(const RenderSnapshot& t_Snapshot)
    {
        // A change of the visible set needs no new recording: WriteDrawCommands gives each
        // recorded indirect draw its instance count (0 or 1) for this frame
        const Frustum& View = m_View = Frustum::FromViewProjection(m_CameraViewProjection);
        m_Culler.Cull(t_Snapshot.Meshes, m_ItemSlots, m_Worlds, m_FrustumCulling ? &View : nullptr);
        // The next frame's Graphics.PublishSnapshot skips the blend of what is far outside this view
        PreCullView::Set(m_FrustumCulling ? &View : nullptr);
    }

    void OffscreenSubpass::WriteOcclusionSlots(const RenderSnapshot& t_Snapshot)
    {
        OcclusionSlot* Slots = m_Occlusion->StageSlots(m_GpuMeshes.size());
        const bool bGpuBlend = GpuBlendActive();
        for (uint32 Slot = 0; Slot < m_GpuMeshes.size(); ++Slot)
        {
            const GpuMesh& Mesh = m_GpuMeshes[Slot];
            // The staging memory holds an old frame's data: every field is written
            OcclusionSlot Empty;
            Empty.Info = glm::uvec4(IsDrawable(Mesh) ? Mesh.MeshModel->GetIndexCount() : 0u, 0u, 0u, 0u);
            Slots[Slot] = Empty;
        }
        for (size_t i = 0; i < t_Snapshot.Meshes.size() && i < m_ItemSlots.size(); ++i)
        {
            const uint32 Slot = m_ItemSlots[i];
            const GpuMesh& Mesh = m_GpuMeshes[Slot];
            if (!IsDrawable(Mesh))
            {
                continue;
            }
            const Aabb& Bounds = Mesh.MeshModel->GetLocalBounds();
            const MeshDrawItem& Item = t_Snapshot.Meshes[i];
            OcclusionSlot& Out = Slots[Slot];
            Out.BoundsMin = glm::vec4(Bounds.Min, 0.0f);
            Out.BoundsMax = glm::vec4(Bounds.Max, 0.0f);
            uint32 Flags = Bounds.IsValid() ? OcclusionSlot::HasBounds : 0u;
            if (bGpuBlend && Item.HasBlend)
            {
                // The shader makes the matrix from the two ends and the factor, and with frustum culling on it
                // also finds out if the mesh is in the view (the CPU has no matrix for it)
                Flags |= OcclusionSlot::Blend;
                Flags |= m_FrustumCulling ? OcclusionSlot::TestFrustum : OcclusionSlot::InView;
                Out.FromPos = glm::vec4(Item.From.Position, 0.0f);
                Out.FromRot =
                    glm::vec4(Item.From.Rotation.x, Item.From.Rotation.y, Item.From.Rotation.z, Item.From.Rotation.w);
                Out.FromScale = glm::vec4(Item.From.Scale, 0.0f);
                Out.ToPos = glm::vec4(Item.To.Position, 0.0f);
                Out.ToRot = glm::vec4(Item.To.Rotation.x, Item.To.Rotation.y, Item.To.Rotation.z, Item.To.Rotation.w);
                Out.ToScale = glm::vec4(Item.To.Scale, 0.0f);
            }
            else if (m_Culler.IsVisible(Slot))
            {
                Flags |= OcclusionSlot::InView;
            }
            Out.Info.y = Flags;
        }
    }

    void OffscreenSubpass::SetOcclusionDepth()
    {
        // The depth is the last attachment of the offscreen frame buffer
        FrameBufferAttachment* Depth = m_OffscreenFrameBuf->GetAttachmentAtIndex(5);
        assert(Depth);
        m_Occlusion->SetDepth(Depth->GetViewHandle(),
                              m_OffscreenFrameBuf->GetWidth(),
                              m_OffscreenFrameBuf->GetHeight(),
                              Depth->GetFormat());
    }

    void OffscreenSubpass::EnsureDrawCommandCapacity()
    {
        const size_t Needed = std::max<size_t>(m_GpuMeshes.size(), 1);
        if (m_DrawCommands != nullptr && m_DrawCommandCapacity >= Needed)
        {
            return;
        }
        // Grow by doubling. The recordings point at the old buffer, so they record again; it
        // is deleted with the retired UBOs, once the GPU is done with the frame that used it
        size_t Capacity = std::max<size_t>(m_DrawCommandCapacity, 64);
        while (Capacity < Needed)
        {
            Capacity *= 2;
        }
        if (m_DrawCommands != nullptr)
        {
            m_RetiredUniformBuffers.Add(m_DrawCommands);
        }
        const VkDeviceSize Size = sizeof(VkDrawIndexedIndirectCommand) * Capacity;
        m_DrawCommands = new Buffer(Size,
                                    VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                        VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        m_Uploader->Fill(*m_DrawCommands, 0, Size, 0);
        m_DrawCommandCapacity = Capacity;
        InvalidateCommandBuffers();
    }

    void OffscreenSubpass::WriteDrawCommands()
    {
        // One buffer is enough: it is in GPU memory, and the frame's upload writes it after the frame
        // before has finished on the GPU (as with the camera UBO)
        auto* Commands = static_cast<VkDrawIndexedIndirectCommand*>(
            m_Uploader->Stage(*m_DrawCommands, 0, sizeof(VkDrawIndexedIndirectCommand) * m_GpuMeshes.size()));
        for (uint32 Slot = 0; Slot < m_GpuMeshes.size(); ++Slot)
        {
            const GpuMesh& Mesh = m_GpuMeshes[Slot];
            VkDrawIndexedIndirectCommand& Command = Commands[Slot];
            Command = {};
            if (IsDrawable(Mesh))
            {
                Command.indexCount = Mesh.MeshModel->GetIndexCount();
                Command.instanceCount = m_Culler.IsVisible(Slot) ? 1 : 0;
            }
        }
    }

    void OffscreenSubpass::UpdateMeshUniformBuffers(const RenderSnapshot& t_Snapshot)
    {
        const bool bGpuBlend = GpuBlendActive();
        for (size_t i = 0; i < t_Snapshot.Meshes.size() && i < m_ItemSlots.size(); ++i)
        {
            const MeshDrawItem& Item = t_Snapshot.Meshes[i];
            const uint32 Slot = m_ItemSlots[i];
            GpuMesh& Mesh = m_GpuMeshes[Slot];
            // A culled mesh keeps its last upload; its version shows that it is old when it is back in view
            if (!Mesh.Drawn || Mesh.Ubo == nullptr || !m_Culler.IsVisible(Slot))
            {
                continue;
            }
            // The cull shader makes the matrix of a mesh that moves between two ticks
            if (bGpuBlend && Item.HasBlend)
            {
                continue;
            }

            // Version 0 has no LocalToWorld yet, so its matrix can change on any frame
            if (Item.WorldVersion != 0 && Item.WorldVersion == Mesh.UploadedVersion)
            {
                continue;
            }

            std::memcpy(m_Uploader->Stage(*m_WorldBuffer, sizeof(glm::mat4) * Slot, sizeof(glm::mat4)),
                        &m_Worlds[i],
                        sizeof(glm::mat4));
            Mesh.UploadedVersion = Item.WorldVersion;
        }
    }

    void OffscreenSubpass::EnsureWorldCapacity(const size_t t_Slots)
    {
        if (m_WorldBuffer != nullptr && m_WorldCapacity >= t_Slots)
        {
            return;
        }
        size_t Capacity = std::max<size_t>(m_WorldCapacity, 64);
        while (Capacity < t_Slots)
        {
            Capacity *= 2;
        }
        const VkDeviceSize Size = sizeof(glm::mat4) * Capacity;
        // The descriptor sets of the meshes are written again below, and the frames in flight use them
        if (m_WorldBuffer != nullptr)
        {
            m_Device->WaitForIdle();
        }
        Buffer* Bigger = new Buffer(Size,
                                    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        m_Uploader->Fill(*Bigger, 0, Size, 0);
        if (m_WorldBuffer != nullptr)
        {
            // The GPU copies the matrices over: it is freed with the other retired buffers
            m_Uploader->Copy(*m_WorldBuffer, *Bigger, sizeof(glm::mat4) * m_WorldCapacity);
            m_RetiredUniformBuffers.Add(m_WorldBuffer);
        }
        m_WorldBuffer = Bigger;
        m_WorldCapacity = Capacity;

        // Every set that is made points at the buffer, so the ones that exist point at the new one
        const VkDescriptorBufferInfo Info = {m_WorldBuffer->GetVkBuffer(), 0, VK_WHOLE_SIZE};
        for (const GpuMesh& Mesh : m_GpuMeshes)
        {
            if (Mesh.Set == VK_NULL_HANDLE)
            {
                continue;
            }
            VkWriteDescriptorSet Write = {};
            Write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            Write.dstSet = Mesh.Set;
            Write.dstBinding = WorldBufferBinding;
            Write.descriptorCount = 1;
            Write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            Write.pBufferInfo = &Info;
            vkUpdateDescriptorSets(m_Device->GetVkDevice(), 1, &Write, 0, nullptr);
        }
        if (m_Occlusion)
        {
            m_Occlusion->SetWorlds(m_WorldBuffer);
        }
    }

    void OffscreenSubpass::RecordDraws(CommandBuffer& t_CmdBuf, const Buffer& t_Commands)
    {
        VkDeviceSize offsets[1] = {0};

        for (uint32 Slot = 0; Slot < m_GpuMeshes.size(); ++Slot)
        {
            const GpuMesh& Mesh = m_GpuMeshes[Slot];
            // Every drawable slot is recorded, in view or not: its indirect command's instance
            // count, written each frame, decides whether it draws
            if (!IsDrawable(Mesh))
            {
                continue;
            }
            Fling::Model* Model = Mesh.MeshModel;

            vkCmdBindDescriptorSets(t_CmdBuf.GetHandle(),
                                    VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    m_GraphicsPipeline->GetPipelineLayout(),
                                    0,
                                    1,
                                    &Mesh.Set,
                                    0,
                                    nullptr);

            VkBuffer vertexBuffers[1] = {Model->GetVertexBuffer()->GetVkBuffer()};
            // Render the mesh
            vkCmdBindVertexBuffers(t_CmdBuf.GetHandle(), 0, 1, vertexBuffers, offsets);
            vkCmdBindIndexBuffer(
                t_CmdBuf.GetHandle(), Model->GetIndexBuffer()->GetVkBuffer(), 0, Model->GetIndexType());
            vkCmdDrawIndexedIndirect(t_CmdBuf.GetHandle(),
                                     t_Commands.GetVkBuffer(),
                                     sizeof(VkDrawIndexedIndirectCommand) * Slot,
                                     1,
                                     sizeof(VkDrawIndexedIndirectCommand));
        }
    }

    void OffscreenSubpass::BuildOffscreenCommandBuffer(CommandBuffer& t_CmdBuf)
    {
        // Set viewport and scissors to the offscreen frame buffer
        VkViewport viewport = Initializers::Viewport(static_cast<float>(m_OffscreenFrameBuf->GetWidth()),
                                                     static_cast<float>(m_OffscreenFrameBuf->GetHeight()),
                                                     0.0f,
                                                     1.0f);

        VkRect2D scissor = Initializers::Rect2D(m_OffscreenFrameBuf->GetWidth(),
                                                m_OffscreenFrameBuf->GetHeight(),
                                                /** offsetX */ 0,
                                                /** offsetY */ 0);

        auto BeginDraws = [&]()
        {
            t_CmdBuf.SetViewport(0, {viewport});
            t_CmdBuf.SetScissor(0, {scissor});
            vkCmdBindPipeline(t_CmdBuf.GetHandle(), VK_PIPELINE_BIND_POINT_GRAPHICS, m_GraphicsPipeline->GetPipeline());
        };

        t_CmdBuf.Begin();
        t_CmdBuf.BeginLabel(DebugName());
        const bool bOcclusion = m_RecordedOcclusion && OcclusionActive();
        if (bOcclusion)
        {
            // Phase 0: what was visible last frame draws first
            m_Occlusion->RecordFirstPhase(t_CmdBuf.GetHandle());
        }

        t_CmdBuf.BeginRenderPass(*m_OffscreenFrameBuf, m_ClearValues);
        BeginDraws();
        RecordDraws(t_CmdBuf, *m_DrawCommands);
        t_CmdBuf.EndRenderPass();

        if (bOcclusion)
        {
            // Phase 1: test everything in view against the depth of the first draw, then draw
            // what it did not hide and was not drawn yet, over the same images
            m_Occlusion->RecordSecondPhase(t_CmdBuf.GetHandle());
            t_CmdBuf.BeginLoadRenderPass(*m_OffscreenFrameBuf);
            BeginDraws();
            RecordDraws(t_CmdBuf, *m_Occlusion->Draw2());
            t_CmdBuf.EndRenderPass();
        }

        t_CmdBuf.EndLabel();
        t_CmdBuf.End();
    }

    void OffscreenSubpass::CreateMeshDescriptorSet(GpuMesh& t_Mesh)
    {
        if (t_Mesh.Set == VK_NULL_HANDLE)
        {
            VkDescriptorSetLayout layout = m_GraphicsPipeline->GetDescriptorSetLayout();
            VkDescriptorSetAllocateInfo allocInfo = {};
            allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
            allocInfo.descriptorPool = m_DescriptorPool;
            allocInfo.descriptorSetCount = 1;
            allocInfo.pSetLayouts = &layout;

            VK_CHECK_RESULT(vkAllocateDescriptorSets(m_Device->GetVkDevice(), &allocInfo, &t_Mesh.Set));
        }

        const PBRTextures& Textures = t_Mesh.MeshMaterial->GetPBRTextures();
        const VkDescriptorBufferInfo WorldInfo = {m_WorldBuffer->GetVkBuffer(), 0, VK_WHOLE_SIZE};
        VkWriteDescriptorSet WorldWrite = {};
        WorldWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        WorldWrite.dstSet = t_Mesh.Set;
        WorldWrite.dstBinding = WorldBufferBinding;
        WorldWrite.descriptorCount = 1;
        WorldWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        WorldWrite.pBufferInfo = &WorldInfo;
        std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
            // 0: UBO
            Initializers::WriteDescriptorSetUniform(t_Mesh.Ubo, t_Mesh.Set, 0),
            // 1: Color map
            Initializers::WriteDescriptorSetImage(Textures.m_AlbedoTexture, t_Mesh.Set, 1),
            // 2: Normal map
            Initializers::WriteDescriptorSetImage(Textures.m_NormalTexture, t_Mesh.Set, 2),
            // 3: Metal map
            Initializers::WriteDescriptorSetImage(Textures.m_MetalTexture, t_Mesh.Set, 3),
            // 4: Roughness map
            Initializers::WriteDescriptorSetImage(Textures.m_RoughnessTexture, t_Mesh.Set, 4),
            // Any other PBR textures or other samplers go HERE and you add to the MRT shader

            // 5: Camera UBO
            Initializers::WriteDescriptorSetUniform(m_CameraUboBuffer, t_Mesh.Set, 5),
            // 6: The world matrix of every mesh, by slot
            WorldWrite};

        vkUpdateDescriptorSets(m_Device->GetVkDevice(),
                               static_cast<uint32>(writeDescriptorSets.size()),
                               writeDescriptorSets.data(),
                               0,
                               nullptr);
    }

    void OffscreenSubpass::PrepareAttachments()
    {
        assert(m_OffscreenFrameBuf == nullptr);

        VkExtent2D Extents = m_SwapChain->GetExtents();

        m_OffscreenFrameBuf = new FrameBuffer(m_Device, Extents.width, Extents.height);

        AttachmentCreateInfo attachmentInfo = {};

        // Four attachments (3 color, 1 depth)
        attachmentInfo.Width = Extents.width;
        attachmentInfo.Height = Extents.height;
        attachmentInfo.LayerCount = 1;
        attachmentInfo.Usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

        // Color attachments

        // Attachment 0: (World space) Positions
        attachmentInfo.Format = VK_FORMAT_R16G16B16A16_SFLOAT;
        m_OffscreenFrameBuf->AddAttachment(attachmentInfo);

        // Attachment 1: (World space) Normals
        attachmentInfo.Format = VK_FORMAT_R16G16B16A16_SFLOAT;
        m_OffscreenFrameBuf->AddAttachment(attachmentInfo);

        // Attachment 2: Albedo (color)
        attachmentInfo.Format = VK_FORMAT_R8G8B8A8_UNORM;
        m_OffscreenFrameBuf->AddAttachment(attachmentInfo);

        // Attachment 3: Metal
        attachmentInfo.Format = VK_FORMAT_R8G8B8A8_UNORM;
        m_OffscreenFrameBuf->AddAttachment(attachmentInfo);

        // Attachment 4: Roughness
        attachmentInfo.Format = VK_FORMAT_R8G8B8A8_UNORM;
        m_OffscreenFrameBuf->AddAttachment(attachmentInfo);

        // Depth attachment
        // Find a suitable depth format
        VkFormat attDepthFormat;
        const PhysicalDevice* PhysDevice = m_Device->GetPhysicalDevice();
        assert(PhysDevice);

        PhysDevice->GetSupportedDepthFormat(&attDepthFormat);

        // Attachment 3: Depth
        attachmentInfo.Format = attDepthFormat;
        // Sampled, so that the occlusion culling pass can build a depth pyramid from it
        attachmentInfo.Usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        m_OffscreenFrameBuf->AddAttachment(attachmentInfo);

        // Create sampler to sample from the color attachments
        VK_CHECK_RESULT(m_OffscreenFrameBuf->CreateSampler(
            VK_FILTER_NEAREST, VK_FILTER_NEAREST, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE));
        VK_CHECK_RESULT(m_OffscreenFrameBuf->CreateRenderPass());
        VK_CHECK_RESULT(m_OffscreenFrameBuf->CreateLoadRenderPass());
        F_LOG_TRACE("Offscreen render pass created...");

        // Create the descriptor pool for off screen things. Each mesh holds one set, so
        // MaxMeshSetsPerImage is also about how many meshes a scene can have
        static constexpr uint32 MaxMeshSetsPerImage = 5000;
        uint32 DescriptorCount = 2 * MaxMeshSetsPerImage * m_SwapChain->GetImageViewCount();

        static std::vector<VkDescriptorPoolSize> poolSizes = {
            Initializers::DescriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, DescriptorCount),
            Initializers::DescriptorPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, DescriptorCount),
            Initializers::DescriptorPoolSize(VK_DESCRIPTOR_TYPE_SAMPLER, DescriptorCount),
            Initializers::DescriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, DescriptorCount),
            Initializers::DescriptorPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, DescriptorCount)};

        VkDescriptorPoolCreateInfo poolInfo = {};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        // Each mesh frees its set when it is destroyed (see ReleaseRetiredMeshResources)
        poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        poolInfo.poolSizeCount = static_cast<uint32>(poolSizes.size());
        poolInfo.pPoolSizes = poolSizes.data();
        poolInfo.maxSets = to_u32(MaxMeshSetsPerImage * m_SwapChain->GetImageViewCount());

        if (vkCreateDescriptorPool(m_Device->GetVkDevice(), &poolInfo, nullptr, &m_DescriptorPool) != VK_SUCCESS)
        {
            F_LOG_FATAL("Failed to create descriptor pool");
        }
    }

    void OffscreenSubpass::CreateGraphicsPipeline()
    {
        assert(m_OffscreenFrameBuf);
        VkRenderPass RenderPass = m_OffscreenFrameBuf->GetRenderPassHandle();
        assert(RenderPass != VK_NULL_HANDLE);

        m_GraphicsPipeline->m_RasterizationState = Initializers::PipelineRasterizationStateCreateInfo(
            VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE);

        // Color Attachment --------
        // Blend attachment states required for all color attachments
        // This is important, as color write mask will otherwise be 0x0 and you
        // won't see anything rendered to the attachment
        m_GraphicsPipeline->m_ColorBlendAttachmentStates = {
            Initializers::PipelineColorBlendAttachmentState(0xf, VK_FALSE),
            Initializers::PipelineColorBlendAttachmentState(0xf, VK_FALSE),
            Initializers::PipelineColorBlendAttachmentState(0xf, VK_FALSE),
            Initializers::PipelineColorBlendAttachmentState(0xf, VK_FALSE),
            Initializers::PipelineColorBlendAttachmentState(0xf, VK_FALSE)};

        m_GraphicsPipeline->m_ColorBlendState.attachmentCount =
            static_cast<uint32>(m_GraphicsPipeline->m_ColorBlendAttachmentStates.size());

        m_GraphicsPipeline->m_ColorBlendState.pAttachments = m_GraphicsPipeline->m_ColorBlendAttachmentStates.data();

        m_GraphicsPipeline->m_MultisampleState =
            Initializers::PipelineMultiSampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT, 0);

        std::vector<VkDynamicState> dynamicStateEnables = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};

        m_GraphicsPipeline->m_DynamicState =
            Initializers::PipelineDynamicStateCreateInfo(dynamicStateEnables.data(), dynamicStateEnables.size(), 0);

        m_GraphicsPipeline->CreateGraphicsPipeline(RenderPass, nullptr);
    }

    void OffscreenSubpass::GatherPresentDependencies(std::vector<CommandBuffer*>& t_CmdBuffs,
                                                     std::vector<VkSemaphore>& t_Deps,
                                                     uint32 t_ActiveFrameIndex,
                                                     uint32 t_CurrentFrameInFlight)
    {
        // The upload goes first: it copies this frame's data, after the frame before has finished
        t_CmdBuffs.emplace_back(m_UploadCmd);
        t_CmdBuffs.emplace_back(m_OffscreenCmdBufs[t_ActiveFrameIndex]);
        t_Deps.emplace_back(m_OffscreenSemaphores[t_CurrentFrameInFlight]);
    }

    void OffscreenSubpass::CleanUp()
    {
        assert(m_Device != nullptr);

        // Called after the device is idle, so nothing uses these now
        for (GpuMesh& Mesh : m_GpuMeshes)
        {
            RetireGpuMesh(Mesh);
        }
        m_GpuMeshes.clear();
        m_Culler.Clear();
        m_Occlusion.reset();
        m_Proxies.Clear();
        m_ItemSlots.clear();
        ReleaseRetiredMeshResources(true);

        if (m_DescriptorPool != VK_NULL_HANDLE)
        {
            vkDestroyDescriptorPool(m_Device->GetVkDevice(), m_DescriptorPool, nullptr);
            m_DescriptorPool = VK_NULL_HANDLE;
        }
    }

    void OffscreenSubpass::OnSwapchainResized()
    {
        const VkExtent2D NewSize = m_SwapChain->GetExtents();
        if (m_OffscreenFrameBuf)
        {
            m_OffscreenFrameBuf->ResizeAndRecreate(NewSize.width, NewSize.height);
            if (m_Occlusion)
            {
                // The depth image is new, so the pyramid is made again
                SetOcclusionDepth();
            }
            // The offscreen render pass is new. Rebuild the pipeline that uses it.
            CreateGraphicsPipeline();
        }

        // The old recordings use the old frame buffer and pipeline
        InvalidateCommandBuffers();
    }

    void OffscreenSubpass::ReleaseRetiredMeshResources(const bool t_All)
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
} // namespace Fling
