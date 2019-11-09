#pragma once

#include "FlingVulkan.h"
#include "Shader.h"
#include "Singleton.hpp"
#include "MeshRenderer.h"
#include "entt/entity/registry.hpp"
#include "ShaderPrograms/ShaderPrograms.h"
#include "Lighting/Lighting.hpp"
#include "Camera.h"
#include "Sampler.hpp"

namespace Fling
{
    /**
     * @brief   A shader program represents what
     */
    class ShaderProgramManager : public Singleton<ShaderProgramManager>
    {
    public:
        void Init() override;

        void Shutdown() override;

        void SortMeshRender();

        void InitGraphicsPipeline(VkRenderPass t_RenderPass, Multisampler* t_Sampler);

        void CreateDescriptors();

        void BindCmdBuffer(VkCommandBuffer& t_CommandBuffer);

        void UpdateUniformBuffers(UINT32 t_CurrentImage, Camera* t_Camera);

        void CreateLightBuffers();

        void UpdateLightBuffers();

        entt::registry* m_Registry;

    private:
        ShaderPrograms* m_PBRShaderProgram;
        Lighting m_Lighting = {};
        LightingUbo m_LightingUbo = {};
    };

}   // namespace Fling