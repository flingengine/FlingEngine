#pragma once

#include "Lighting/DeferredLightSettings.h"
#include "Lighting/DirectionalLight.hpp"
#include "Lighting/Lighting.hpp"
#include "Lighting/PointLight.hpp"
#include "Subpass.h"

#include <cstddef>
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
    static_assert(offsetof(LightingUbo, PointLightCount) == 4 && offsetof(LightingUbo, DirLightBuffer) == 16 &&
                  offsetof(LightingUbo, PointLightBuffer) == 16 + 48 * DeferredLightSettings::MaxDirectionalLights);

    /** The std140 layout of deferred.frag's UBO at binding 7. */
    struct CameraInfoUbo
    {
        glm::mat4 Projection;
        glm::mat4 ModelView;
        glm::vec4 CamPos = {};
        float Gamma = 2.2f;
        float Exposure = 4.5f;
        /** std140 rounds the block to 16 bytes (160); packed GLM types would make the struct 152. */
        float Padding[2] = {};
    };
    static_assert(offsetof(CameraInfoUbo, ModelView) == 64 && offsetof(CameraInfoUbo, CamPos) == 128 &&
                  offsetof(CameraInfoUbo, Gamma) == 144 && offsetof(CameraInfoUbo, Exposure) == 148 &&
                  sizeof(CameraInfoUbo) == 160);

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
                        VkRenderPass t_GlobalRenderPass,
                        FirstPersonCamera* t_Cam,
                        FrameBuffer* t_OffscreenDep,
                        std::shared_ptr<Fling::Shader> t_Vert,
                        std::shared_ptr<Fling::Shader> t_Frag);

        virtual ~GeometrySubpass();

        void Draw(CommandBuffer& t_CmdBuf,
                  uint32 t_ActiveFrameInFlight,
                  const RenderSnapshot& t_Snapshot,
                  float DeltaTime) override;

        void CreateDescriptorSets(VkDescriptorPool t_Pool) override;

        /**
         * @param t_FrameBuffer	The swap chain frame buffer
         */
        const char* DebugName() const override { return "Lighting"; }

        void CreateGraphicsPipeline() override;

        virtual void OnSwapchainResized() override final;

    private:
        void UpdateLightingUBO(const RenderSnapshot& t_Snapshot, uint32 t_ActiveFrame);

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