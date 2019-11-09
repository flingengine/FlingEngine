#include "ShaderPrograms\ShaderProgramManager.h"
#include "Renderer.h"

namespace Fling
{
    void ShaderProgramManager::Init()
    {
        VkDevice Device = Renderer::Get().GetLogicalVkDevice();

        //Initialize Shader Programs
        m_PBRShaderProgram = new ShaderPrograms(
            Device,
            HS("Shaders/PBRDefault_vert.spv"),
            HS("Shaders/PBRDefault_frag.spv"));
    }

    void ShaderProgramManager::Shutdown()
    {
    }

    void ShaderProgramManager::SortMeshRender()
    {
    }

    void ShaderProgramManager::InitGraphicsPipeline(VkRenderPass t_RenderPass, Multisampler* t_Sampler)
    {
         
    }

    void ShaderProgramManager::CreateDescriptors()
    {
    }

    void ShaderProgramManager::BindCmdBuffer(VkCommandBuffer& t_CommandBuffer)
    {
    }

    void ShaderProgramManager::UpdateUniformBuffers(UINT32 t_CurrentImage, Camera* t_Camera)
    {
    }

    void ShaderProgramManager::CreateLightBuffers()
    {
    }

    void ShaderProgramManager::UpdateLightBuffers()
    {
    }
}
