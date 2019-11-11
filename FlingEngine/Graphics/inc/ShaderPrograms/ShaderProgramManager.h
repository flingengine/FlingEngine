#pragma once

#include "FlingVulkan.h"
#include "Shader.h"
#include "Singleton.hpp"
#include "MeshRenderer.h"
#include "ShaderPrograms/ShaderPrograms.h"
#include "Lighting/Lighting.hpp"
#include "Camera.h"
#include "Sampler.hpp"

#include "entt/entity/registry.hpp"
#include "entt/entity/group.hpp"

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

        void BindCmdBuffer(VkCommandBuffer& t_CommandBuffer, UINT32 t_CommandBufferIndex);

        void UpdateUniformBuffers(UINT32 t_CurrentImage, Camera* t_Camera);

        void CreateLightBuffers();

        void UpdateLightBuffers(UINT32 t_CurrentImage);

        entt::registry* m_Registry;

    private:
        ShaderPrograms* m_PBRShaderProgram;
        Lighting m_Lighting = {};
        LightingUbo m_LightingUbo = {};
    };

}   // namespace Fling