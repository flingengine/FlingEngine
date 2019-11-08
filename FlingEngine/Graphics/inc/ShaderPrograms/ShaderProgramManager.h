#pragma once

#include "FlingVulkan.h"
#include "Shader.h"
#include "Singleton.hpp"
#include "MeshRenderer.h"
#include "entt/entity/registry.hpp"
#include "ShaderPrograms/ShaderPrograms.h"
#include "Lighting/Lighting.hpp"

namespace Fling
{
    /**
     * @brief   A shader program represents what
     */
    class ShaderProgramManager : public Singleton<ShaderProgramManager>
    {
    public:

        void Shutdown() override;

        void SortMeshRender();

        void AddMeshRender(MeshRenderer& t_MeshRender);

        void RemoveMeshRender(MeshRenderer& t_MeshRender);

        void InitGraphicsPipeline(VkRenderPass t_RenderPass);

        void CreateDescriptors();

        void BindCmdBuffer(VkCommandBuffer& t_CommandBuffer);

        void UpdateUniformBuffers(UINT32 t_CurrentImage, Camera* t_Camera);

        void CreateLightBuffers();

        entt::registry* m_Registry;

    private:
        Lighting m_Lighting = {};
    };

}   // namespace Fling