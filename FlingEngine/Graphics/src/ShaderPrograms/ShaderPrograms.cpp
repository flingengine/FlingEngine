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

	ShaderProgramType ShaderPrograms::ShaderProgramFromStr(std::string& t_Str)
	{
		// Make the string all uppercase
		std::locale loc;
		for (std::string::size_type i = 0; i < t_Str.length(); ++i)
		{
			t_Str[i] = std::toupper(t_Str[i], loc);
		}

		// Parse string and return what type it is
		if (t_Str == "PBR")
		{
			return ShaderProgramType::PBR;
		}
		else if (t_Str == "REFLECTION")
		{
			return ShaderProgramType::Reflection;
		}
		else if (t_Str == DeferredStr_CAP)
		{
			return ShaderProgramType::Deferred;
		}
		else
		{
			F_LOG_WARN("Shader type '{}' is not supported! See ShaderPrograms::ShaderProgramFromStr", t_Str);
		}

		return ShaderProgramType();
	}
}