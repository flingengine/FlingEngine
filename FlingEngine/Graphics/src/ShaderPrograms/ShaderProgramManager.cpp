#include "ShaderPrograms\ShaderProgramManager.h"
#include "Renderer.h"

namespace Fling
{
    void ShaderProgramManager::Init()
    {
        assert(m_Registry);

        VkDevice Device = Renderer::Get().GetLogicalVkDevice();

        //Initialize Shader Programs
        m_PBRShaderProgram = new ShaderPrograms(
            Device,
            HS("Shaders/PBRDefault_vert.spv"),
            HS("Shaders/PBRDefault_frag.spv"));
    }

    void ShaderProgramManager::Shutdown()
    {
        if (m_PBRShaderProgram)
        {
            delete m_PBRShaderProgram;
            m_PBRShaderProgram = nullptr;
        }
    }

    void ShaderProgramManager::SortMeshRender()
    {
        m_Registry->sort<MeshRenderer>([](const auto& lhs, const auto& rhs)
            {
                return lhs.m_Material->GetShaderProgramType() < rhs.m_Material->GetShaderProgramType();
            });

    }

    void ShaderProgramManager::InitGraphicsPipeline(VkRenderPass t_RenderPass, Multisampler* t_Sampler)
    {
        m_PBRShaderProgram->InitGraphicPipeline(t_RenderPass, t_Sampler);
    }

    void ShaderProgramManager::CreateDescriptors()
    {
        auto PBRGroup = m_Registry->group<MeshRenderer, entt::tag<HS("PBR")>>();

        for (auto entity : PBRGroup)
        {
            auto& meshRender = PBRGroup.get<MeshRenderer>(entity);
            ShaderProgramPBR::CreateDescriptorPool(meshRender);
            ShaderProgramPBR::CreateDescriptorSets(meshRender, m_Lighting, m_PBRShaderProgram->GetDescriptorLayout());
        }
    }

    void ShaderProgramManager::BindCmdBuffer(VkCommandBuffer& t_CommandBuffer, UINT32 t_CommandBufferIndex)
    {
        auto PBRGroup = m_Registry->group<MeshRenderer, entt::tag<HS("PBR")>>();

        GraphicsPipeline* pipeline = m_PBRShaderProgram->GetPipeline().get();
        pipeline->BindGraphicsPipeline(t_CommandBuffer);
        for (auto entity : PBRGroup)
        {
            auto& meshRender = PBRGroup.get<MeshRenderer>(entity);
            ShaderProgramPBR::BindCmdBuffer(meshRender, t_CommandBuffer, pipeline, t_CommandBufferIndex);
        }
    }

    void ShaderProgramManager::UpdateUniformBuffers(UINT32 t_CurrentImage, Camera* t_Camera)
    {
        //auto PBRGroup = m_Registry->group<Transform, MeshRenderer, entt::tag<HS("PBR")>>(); //Investigate why this doesn't work
        auto PBRGroup = m_Registry->view<MeshRenderer, Transform, entt::tag<HS("PBR")>>();

        for (auto entity : PBRGroup)
        {
            auto& meshRender = PBRGroup.get<MeshRenderer>(entity);
            auto& transform = PBRGroup.get<Transform>(entity);

            ShaderProgramPBR::UpdateUniformBuffer(meshRender, transform, t_CurrentImage, t_Camera);
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
        auto LightView = m_Registry->group<DirectionalLight, PointLight>();
        UINT32 CurLightCount = 0;

        for (auto entity : LightView)
        {
            if (CurLightCount < Lighting::MaxDirectionalLights)
            {
                DirectionalLight& Light = LightView.get<DirectionalLight>(entity);
                // Copy the dir light info to the buffer
                size_t size = sizeof(DirectionalLight);
                memcpy((m_LightingUbo.DirLightBuffer + (CurLightCount++)), &Light, size);
            }
        }

        m_LightingUbo.DirLightCount = CurLightCount;

        CurLightCount = 0;

        //Copy the point light data to the UBO
        for (auto entity : LightView)
        {
            if (CurLightCount < Lighting::MaxPointLights)
            {
                PointLight& Light = LightView.get<PointLight>(entity);
                // Copy the point light info to the buffer
                size_t size = sizeof(DirectionalLight);
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
