#include "Subpass.h"
#include "LogicalDevice.h"
#include "PhyscialDevice.h"

namespace Fling
{
	Subpass::Subpass(LogicalDevice* t_Dev, std::shared_ptr<Fling::Shader> t_Vert, std::shared_ptr<Fling::Shader> t_Frag)
		: m_Device(t_Dev)
		, m_VertexShader(t_Vert)
		, m_FragShader(t_Frag)
	{
		assert(m_Device);
		std::vector<Shader*> Shaders = { m_VertexShader.get(), m_FragShader.get() };
		m_DescriptorLayout = Shader::CreateSetLayout(m_Device->GetVkDevice(), Shaders);
		m_PipelineLayout = Shader::CreatePipelineLayout(m_Device->GetVkDevice(), m_DescriptorLayout, 0, 0);

		// Build the VkPipeline
	}
}
