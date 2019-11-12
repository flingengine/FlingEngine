#include "ShaderPrograms/ShaderPrograms.h"
#include "ResourceManager.h"
#include <vector>

namespace Fling
{
    ShaderPrograms::ShaderPrograms(VkDevice t_Device, Guid t_VertexShader, Guid t_FragShader) 
        : m_Device(t_Device)
    {
        m_VertexShader = Shader::Create(t_VertexShader).get();
        m_FragShader = Shader::Create(t_FragShader).get();

        m_Pipeline = std::make_shared<GraphicsPipeline>(m_VertexShader, m_FragShader, m_Device);
    }

    ShaderPrograms::~ShaderPrograms()
    {
        vkDestroyPipelineLayout(m_Device, m_PipelineLayout, nullptr);
        vkDestroyDescriptorSetLayout(m_Device, m_DescriptorLayout, nullptr);
    }

    void ShaderPrograms::InitGraphicPipeline(VkRenderPass t_Renderpass, Multisampler* t_Sampler)
    {
        std::vector<Shader*> Shaders =
        {
            m_VertexShader,
            m_FragShader
        };

        m_Pipeline->CreateGraphicsPipeline(t_Renderpass, t_Sampler);
        m_DescriptorLayout = Shader::CreateSetLayout(m_Device, Shaders);
        m_PipelineLayout = Shader::CreatePipelineLayout(m_Device, m_DescriptorLayout, 0, 0);
    }
}