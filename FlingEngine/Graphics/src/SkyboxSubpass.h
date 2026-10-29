#pragma once

#include "Subpass.h"
#include "FlingMath.h"
#include <memory>
#include <vector>

namespace Fling
{
    class Buffer;
    class CommandBuffer;
    class Cubemap;
    class FirstPersonCamera;
    class LogicalDevice;
    class Swapchain;

    /**
     * Draws a cube map behind the scene. It draws one full screen triangle with no depth test
     * in the global render pass, before the deferred composite. The composite discards pixels
     * that have no geometry, so the sky shows through there.
     *
     * @see GeometrySubpass
     */
    class SkyboxSubpass : public Subpass
    {
    public:
        SkyboxSubpass(const LogicalDevice* t_Dev,
                      const Swapchain* t_Swap,
                      VkRenderPass t_GlobalRenderPass,
                      FirstPersonCamera* t_Cam,
                      std::shared_ptr<Cubemap> t_Cubemap,
                      std::shared_ptr<Fling::Shader> t_Vert,
                      std::shared_ptr<Fling::Shader> t_Frag);

        virtual ~SkyboxSubpass();

        void
        Draw(CommandBuffer& t_CmdBuf, uint32 t_ActiveFrameInFlight, entt::registry& t_reg, float DeltaTime) override;

        void CreateDescriptorSets(VkDescriptorPool t_Pool, entt::registry& t_reg) override;

        void CreateGraphicsPipeline() override;

        void OnSwapchainResized(entt::registry& t_reg) override;

    private:
        /** Matches the UBO at binding 0 of skybox.frag */
        struct SkyboxUbo
        {
            /** Inverse of projection * view, with the camera translation removed */
            glm::mat4 InvViewProjection;
        };

        VkRenderPass m_GlobalRenderPass = VK_NULL_HANDLE;

        const FirstPersonCamera* m_Camera = nullptr;

        std::shared_ptr<Cubemap> m_Cubemap;

        /** One UBO for each swap chain image */
        std::vector<std::unique_ptr<Buffer>> m_UboBuffers;

        std::vector<VkDescriptorSet> m_DescriptorSets;

        VkDescriptorPool m_DescPool = VK_NULL_HANDLE;
    };
} // namespace Fling
