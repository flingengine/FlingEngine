#pragma once

#include "FlingVulkan.h"
#include "Shader.h"
#include "ShaderPrograms/ShaderProgram.h"
#include "Singleton.hpp"
#include "Camera.h"
#include "Sampler.hpp"

#include "entt/entity/registry.hpp"
#include "entt/entity/group.hpp"

#include "ShaderPrograms/ShaderProgramReflections.h"

#include "Components/Transform.h"
#include "MeshRenderer.h"
#include "Lighting/Lighting.hpp"
#include "GeometrySubpass.h"

namespace Fling
{
    /**
     * @brief   A shader program represents what
     */
    class ShaderProgramManager : public Singleton<ShaderProgramManager>
    {
    public:
        virtual void Init() override;

        virtual void Shutdown() override;

        void PrepShutdown();

        void SortMeshRender();

        void InitGraphicsPipeline(VkRenderPass t_RenderPass, Multisampler* t_Sampler);

        void CreateDescriptors();

        void BindCmdBuffer(VkCommandBuffer& t_CommandBuffer, UINT32 t_CommandBufferIndex);

        void UpdateUniformBuffers(UINT32 t_CurrentImage, Camera* t_Camera);

        void CreateLightBuffers();

        entt::registry* m_Registry;

    private:
        void UpdateLightBuffers(UINT32 t_CurrentImage);

        ShaderProgram* m_ReflectionProgram = nullptr;

        Lighting m_Lighting = {};
        LightingUbo m_LightingUbo = {};
    };

}   // namespace Fling