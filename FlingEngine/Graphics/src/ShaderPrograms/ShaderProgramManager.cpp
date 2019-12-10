#include "ShaderPrograms/ShaderProgramManager.h"
#include "VulkanApp.h"
#include "MeshRenderer.h"
#include "LogicalDevice.h"

#include "Renderer.h"

namespace Fling
{
    void ShaderProgramManager::Init()
    {
        assert(m_Registry);

		LogicalDevice* Dev = VulkanApp::Get().GetLogicalDevice();
		assert(Dev);
		VkDevice Device = Dev->GetVkDevice();

        const std::vector<Shader*> ReflectionShaders =
        {
			Shader::Create(HS("Shaders/CubeMapReflections_vert.spv"), Dev).get(),
			Shader::Create(HS("Shaders/CubeMapReflections_frag.spv"), Dev).get(),
        };

        m_ReflectionProgram = new ShaderProgram(
            Device,
            ReflectionShaders
        );
    }

    void ShaderProgramManager::Shutdown()
    {
        if (m_ReflectionProgram)
        {
            delete m_ReflectionProgram;
            m_ReflectionProgram = nullptr;
        }
    }

    void ShaderProgramManager::PrepShutdown()
    {
		LogicalDevice* Dev = VulkanApp::Get().GetLogicalDevice();
		assert(Dev);
		VkDevice Device = Dev->GetVkDevice();


        // Delete light buffers
        for (size_t i = 0; i < m_Lighting.m_LightingUBOs.size(); i++)
        {
            if (m_Lighting.m_LightingUBOs[i])
            {
                delete m_Lighting.m_LightingUBOs[i];
                m_Lighting.m_LightingUBOs[i] = nullptr;
            }
        }

        m_Lighting.m_LightingUBOs.clear();
    }

    void ShaderProgramManager::SortMeshRender()
    {

    }

    void ShaderProgramManager::InitGraphicsPipeline(VkRenderPass t_RenderPass, Multisampler* t_Sampler)
    {
		// #TODO  Break graphics pipeline out into each type of shader program
        m_ReflectionProgram->InitGraphicPipeline(t_RenderPass, t_Sampler);
    }

    void ShaderProgramManager::CreateDescriptors()
    {
        auto PBRGroup = m_Registry->view<MeshRenderer, entt::tag<HS("PBR")>>();
        auto ReflectionGroup = m_Registry->view<MeshRenderer, entt::tag<HS("Reflection")>>();
		auto DeferredGroup = m_Registry->view<MeshRenderer, entt::tag<HS(DeferredStr)>>();

        for (auto entity : ReflectionGroup)
        {
            auto& meshRender = ReflectionGroup.get<MeshRenderer>(entity);
            ShaderProgramReflections::CreateDescriptorPool(meshRender);
            ShaderProgramReflections::CreateDescriptorSets(meshRender, m_Lighting, m_ReflectionProgram->GetDescriptorLayout());
        }
    }

    void ShaderProgramManager::BindCmdBuffer(VkCommandBuffer& t_CommandBuffer, UINT32 t_CommandBufferIndex)
    {
        auto PBRGroup = m_Registry->view<MeshRenderer, entt::tag<HS("PBR")>>();
        auto ReflectionGroup = m_Registry->view<MeshRenderer, entt::tag<HS("Reflection")>>();
		auto DeferredGroup = m_Registry->view<MeshRenderer, entt::tag<HS(DeferredStr)>>();

        GraphicsPipeline* pipeline = nullptr;

        //Reflections
        {
            pipeline = m_ReflectionProgram->GetPipeline().get();
            pipeline->BindGraphicsPipeline(t_CommandBuffer);
            for (auto entity : ReflectionGroup)
            {
                auto& meshRender = ReflectionGroup.get<MeshRenderer>(entity);
                ShaderProgramReflections::BindCmdBuffer(meshRender, t_CommandBuffer, pipeline, t_CommandBufferIndex);
            }
        }
    }

    void ShaderProgramManager::UpdateUniformBuffers(UINT32 t_CurrentImage, Camera* t_Camera)
    {
        auto PBRview = m_Registry->view<MeshRenderer, Transform, entt::tag<HS("PBR")>>();
        auto ReflectionView = m_Registry->view<MeshRenderer, Transform, entt::tag<HS("Reflection")>>();
		auto DeferredGroup = m_Registry->view<MeshRenderer, Transform, entt::tag<HS(DeferredStr)>>();

        for (auto entity : ReflectionView)
        {
            auto& meshRender = ReflectionView.get<MeshRenderer>(entity);
            auto& transform = ReflectionView.get<Transform>(entity);

            ShaderProgramReflections::UpdateUniformBuffer(meshRender, transform, t_CurrentImage, t_Camera);
        }

        UpdateLightBuffers(t_CurrentImage);
    }

    void ShaderProgramManager::CreateLightBuffers()
    {
        const std::vector<VkImage>& Images = Renderer::Get().GetSwapChain()->GetImages();
        VkDeviceSize bufferSize = sizeof(m_LightingUbo);

        m_Lighting.m_LightingUBOs.resize(Images.size());
        for (size_t i = 0; i < Images.size(); i++)
        {
            m_Lighting.m_LightingUBOs[i] = new Buffer(
                bufferSize,
                VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

            m_Lighting.m_LightingUBOs[i]->MapMemory(bufferSize);
        }
    }

    void ShaderProgramManager::UpdateLightBuffers(UINT32 t_CurrentImage)
    {
        //copy directional lights to the fragment shader
        auto DirectionalLightView = m_Registry->view<DirectionalLight>();
        auto PointLightView = m_Registry->view<PointLight, Transform>();

        UINT32 CurLightCount = 0;

        for (auto entity : DirectionalLightView)
        {
            if (CurLightCount < Lighting::MaxDirectionalLights)
            {
                DirectionalLight& Light = DirectionalLightView.get(entity);
                // Copy the dir light info to the buffer
                size_t size = sizeof(DirectionalLight);
                memcpy((m_LightingUbo.DirLightBuffer + (CurLightCount++)), &Light, size);
            }
        }

        m_LightingUbo.DirLightCount = CurLightCount;

        CurLightCount = 0;

        //Copy the point light data to the UBO
        for (auto entity : PointLightView)
        {
            if (CurLightCount < Lighting::MaxPointLights)
            {
                PointLight& Light = PointLightView.get<PointLight>(entity);
                Transform& Trans = PointLightView.get<Transform>(entity);

                Light.SetPos(glm::vec4(Trans.GetPos(), 1.0f));
                // Copy the point light info to the buffer
                size_t size = sizeof(PointLight);
                memcpy((m_LightingUbo.DirLightBuffer + (CurLightCount++)), &Light, size);

            }
        }

        m_LightingUbo.PointLightCount = CurLightCount;

        memcpy(
            m_Lighting.m_LightingUBOs[t_CurrentImage]->m_MappedMem,
            &m_LightingUbo,
            sizeof(m_LightingUbo));
    }
}
