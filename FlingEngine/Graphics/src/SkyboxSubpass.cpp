#include "SkyboxSubpass.h"
#include "Buffer.h"
#include "CommandBuffer.h"
#include "Cubemap.h"
#include "FirstPersonCamera.h"
#include "GraphicsHelpers.h"
#include "GraphicsPipeline.h"
#include "LogicalDevice.h"
#include "SwapChain.h"
#include "VulkanApp.h"
#include "VulkanAppState.h"

namespace Fling
{
    SkyboxSubpass::SkyboxSubpass(const LogicalDevice* t_Dev,
                                 const Swapchain* t_Swap,
                                 VkRenderPass t_GlobalRenderPass,
                                 FirstPersonCamera* t_Cam,
                                 std::shared_ptr<Cubemap> t_Cubemap,
                                 std::shared_ptr<Fling::Shader> t_Vert,
                                 std::shared_ptr<Fling::Shader> t_Frag)
        : Subpass(t_Dev, t_Swap, t_Vert, t_Frag),
          m_GlobalRenderPass(t_GlobalRenderPass),
          m_Camera(t_Cam),
          m_Cubemap(std::move(t_Cubemap))
    {
        assert(m_GlobalRenderPass != VK_NULL_HANDLE && m_Camera && m_Cubemap && m_Cubemap->IsValid());

        const VkDeviceSize BufferSize = sizeof(SkyboxUbo);
        m_UboBuffers.resize(m_SwapChain->GetImageCount());
        for (std::unique_ptr<Buffer>& Buf : m_UboBuffers)
        {
            Buf = std::make_unique<Buffer>(BufferSize,
                                           VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                           VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

            Buf->MapMemory(BufferSize);
        }
    }

    SkyboxSubpass::~SkyboxSubpass() = default;

    void
    SkyboxSubpass::Draw(CommandBuffer& t_CmdBuf, uint32 t_ActiveFrameInFlight, entt::registry& t_reg, float DeltaTime)
    {
        // Invert the projection Y to match the Vulkan clip space, like the other passes
        glm::mat4 Projection = m_Camera->GetProjectionMatrix();
        Projection[1][1] *= -1.0f;

        // Only the camera rotation moves the sky
        const glm::mat4 ViewRotation = glm::mat4(glm::mat3(m_Camera->GetViewMatrix()));

        SkyboxUbo Ubo;
        Ubo.InvViewProjection = glm::inverse(Projection * ViewRotation);
        memcpy(m_UboBuffers[t_ActiveFrameInFlight]->m_MappedMem, &Ubo, sizeof(Ubo));

        vkCmdBindDescriptorSets(t_CmdBuf.GetHandle(),
                                VK_PIPELINE_BIND_POINT_GRAPHICS,
                                m_GraphicsPipeline->GetPipelineLayout(),
                                0,
                                1,
                                &m_DescriptorSets[t_ActiveFrameInFlight],
                                0,
                                nullptr);

        vkCmdBindPipeline(t_CmdBuf.GetHandle(), VK_PIPELINE_BIND_POINT_GRAPHICS, m_GraphicsPipeline->GetPipeline());

        // One full screen triangle. The vertex shader makes the positions from gl_VertexIndex.
        vkCmdDraw(t_CmdBuf.GetHandle(), 3, 1, 0, 0);
    }

    void SkyboxSubpass::CreateDescriptorSets(VkDescriptorPool t_Pool, entt::registry& t_reg)
    {
        // We only need to do the actual allocation of sets ONCE
        if (m_DescPool == VK_NULL_HANDLE)
        {
            m_DescPool = t_Pool;

            const size_t ImageCount = m_SwapChain->GetImageCount();
            m_DescriptorSets.resize(ImageCount);

            std::vector<VkDescriptorSetLayout> Layouts(ImageCount, m_GraphicsPipeline->GetDescriptorSetLayout());
            VkDescriptorSetAllocateInfo AllocInfo = {};
            AllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
            AllocInfo.descriptorPool = t_Pool;
            AllocInfo.descriptorSetCount = static_cast<uint32>(ImageCount);
            AllocInfo.pSetLayouts = Layouts.data();

            VK_CHECK_RESULT(vkAllocateDescriptorSets(m_Device->GetVkDevice(), &AllocInfo, m_DescriptorSets.data()));
        }

        for (size_t i = 0; i < m_DescriptorSets.size(); ++i)
        {
            // The cube map info is const, but the Vulkan write struct wants a mutable pointer
            VkDescriptorImageInfo CubemapInfo = *m_Cubemap->GetDescriptorInfo();

            std::vector<VkWriteDescriptorSet> WriteDescriptorSets = {
                // 0 : Camera UBO to the fragment shader
                Initializers::WriteDescriptorSetUniform(m_UboBuffers[i].get(), m_DescriptorSets[i], 0),
                // 1 : Cube map sampler
                Initializers::WriteDescriptorSet(
                    m_DescriptorSets[i], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &CubemapInfo),
            };

            vkUpdateDescriptorSets(m_Device->GetVkDevice(),
                                   static_cast<uint32>(WriteDescriptorSets.size()),
                                   WriteDescriptorSets.data(),
                                   0,
                                   nullptr);
        }
    }

    void SkyboxSubpass::CreateGraphicsPipeline()
    {
        // The full screen triangle covers every pixel, so cull nothing and ignore depth
        m_GraphicsPipeline->m_RasterizationState = Initializers::PipelineRasterizationStateCreateInfo(
            VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE);

        m_GraphicsPipeline->m_DepthStencilState.depthTestEnable = VK_FALSE;
        m_GraphicsPipeline->m_DepthStencilState.depthWriteEnable = VK_FALSE;

        m_GraphicsPipeline->CreateGraphicsPipeline(m_GlobalRenderPass, nullptr);
    }

    void SkyboxSubpass::OnSwapchainResized(entt::registry& t_reg)
    {
        m_GlobalRenderPass = VulkanApp::Get().GetState().m_RenderPass;
        CreateGraphicsPipeline();

        if (m_DescPool != VK_NULL_HANDLE)
        {
            CreateDescriptorSets(m_DescPool, t_reg);
        }
    }
} // namespace Fling
