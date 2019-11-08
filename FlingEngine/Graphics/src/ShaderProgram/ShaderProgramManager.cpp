#pragma once

#include "FlingVulkan.h"
#include "Shader.h"
#include "Singleton.hpp"
#include "MeshRenderer.h"
#include "ShaderPrograms/ShaderProgram.h"
#include "entt/entity/registry.hpp"

namespace Fling
{
    class ShaderProgramManager : Singleton<ShaderProgramManager>
    {
    public:
        ShaderProgramManager();
        ~ShaderProgramManager();

        void Shutdown() override;

        void SortMeshRender();

        void InitGraphicsPipeline(VkRenderPass t_RenderPass);

        void CreateDescriptors();

        void BindCmdBuffer(VkCommandBuffer& t_CommandBuffer);

        void UpdateUniformBuffers(UINT32 t_CurrentImage, Camera* t_Camera);

        void CreateLightBuffers();

        entt::registry* m_Registry;

    private:
    };
}