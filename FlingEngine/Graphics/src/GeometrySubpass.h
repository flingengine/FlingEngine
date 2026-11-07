#pragma once

#include "Lighting/DeferredLightSettings.h"
#include "Lighting/DirectionalLight.hpp"
#include "Lighting/Lighting.hpp"
#include "Lighting/PointLight.hpp"
#include "Subpass.h"

#include <memory>
#include <vector>

namespace Fling
{
    class CommandBuffer;
    class LogicalDevice;
    class FrameBuffer;
    struct MeshRenderer;
    class Swapchain;
    class GraphicsPipeline;
    class Model;
    class Buffer;
    class FirstPersonCamera;

    /** Uniform buffer for passing lights to our final screen pass */
    struct LightingUbo
    {
        alignas(4) uint32 DirLightCount = 0;
        alignas(4) uint32 PointLightCount = 0;

        alignas(16) DirectionalLight DirLightBuffer[DeferredLightSettings::MaxDirectionalLights] = {};

        alignas(16) PointLight PointLightBuffer[DeferredLightSettings::MaxPointLights] = {};
    };

    struct CameraInfoUbo
    {
        glm::mat4 Projection;
        glm::mat4 ModelView;
        glm::vec4 CamPos = {};
        float Gamma = 2.2f;
        float Exposure = 4.5f;
    };

    /**
     * The geometry subpass is in charge of sending the geometry portion of
     *			the Deferred pipeline to the GPU. This includes frame buffer attachments for
     *			albedo, normals, and depth as well some actual mesh data via a Uniform buffer
     *			Uses the Deferred shaders
     */
    class GeometrySubpass : public Subpass
    {
    public:
        GeometrySubpass(const LogicalDevice* t_Dev,
                        const Swapchain* t_Swap,
                        Fling::EntityStorage& t_reg,
                        VkRenderPass t_GlobalRenderPass,
                        FirstPersonCamera* t_Cam,
                        FrameBuffer* t_OffscreenDep,
                        std::shared_ptr<Fling::Shader> t_Vert,
                        std::shared_ptr<Fling::Shader> t_Frag);

        virtual ~GeometrySubpass();

        void
        Draw(CommandBuffer& t_CmdBuf, uint32 t_ActiveFrameInFlight, Fling::EntityStorage& t_reg, float DeltaTime) override;

        void CreateDescriptorSets(VkDescriptorPool t_Pool, Fling::EntityStorage& t_reg) override;

        /**
         * @param t_FrameBuffer	The swap chain frame buffer
         */
        void CreateGraphicsPipeline() override;

        virtual void OnSwapchainResized(Fling::EntityStorage& t_reg) override final;

    private:
        void OnPointLightAdded(Fling::EntityStorage& t_Reg, Fling::Entity t_Ent);

        void UpdateLightingUBO(Fling::EntityStorage& t_Reg, uint32 t_ActiveFrame);

        // Global render pass for frame buffer writes
        std::shared_ptr<Model> m_QuadModel;

        VkRenderPass m_GlobalRenderPass = VK_NULL_HANDLE;
        VkDescriptorPool m_DescPool = VK_NULL_HANDLE;

        const FirstPersonCamera* m_Camera;

        /** The offscreen frame buffer that has the G Buffer attachments */
        FrameBuffer* m_OffscreenFrameBuf = nullptr;

        // Descriptor sets and Uniform buffers -- one per swap image
        std::vector<VkDescriptorSet> m_DescriptorSets;
        std::vector<Buffer*> m_LightingUboBuffers;
        std::vector<Buffer*> m_CameraUboBuffers;

        std::vector<Buffer*> m_QuadUboBuffer;

        LightingUbo m_LightingUBO = {};

        CameraInfoUbo m_CamInfoUBO = {};
    };
} // namespace Fling