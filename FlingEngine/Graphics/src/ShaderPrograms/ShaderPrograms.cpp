#include "ShaderPrograms/ShaderPrograms.h"
#include "ResourceManager.h"
#include <vector>

namespace Fling
{
    ShaderPrograms::ShaderPrograms(VkDevice t_Device, const std::vector<Shader*>& t_Shaders) 
        : m_Device(t_Device), m_Shaders(t_Shaders)
    {
        m_Pipeline = std::make_shared<GraphicsPipeline>(t_Shaders, m_Device);
    }

    ShaderPrograms::~ShaderPrograms()
    {
        vkDestroyPipelineLayout(m_Device, m_PipelineLayout, nullptr);
        vkDestroyDescriptorSetLayout(m_Device, m_DescriptorLayout, nullptr);
    }

    void ShaderPrograms::InitGraphicPipeline(VkRenderPass t_Renderpass, Multisampler* t_Sampler)
    {
        m_Pipeline->CreateGraphicsPipeline(t_Renderpass, t_Sampler);
        m_DescriptorLayout = Shader::CreateSetLayout(m_Device, m_Shaders);
        m_PipelineLayout = Shader::CreatePipelineLayout(m_Device, m_DescriptorLayout, 0, 0);
    }
}