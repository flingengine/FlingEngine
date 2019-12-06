#include "Subpass.h"
#include "LogicalDevice.h"
#include "PhyscialDevice.h"
#include "SwapChain.h"
#include "GraphicsPipeline.h"

namespace Fling
{
	Subpass::Subpass(const LogicalDevice* t_Dev, const Swapchain* t_Swap, std::shared_ptr<Fling::Shader> t_Vert, std::shared_ptr<Fling::Shader> t_Frag)
		: m_Device(t_Dev)
		, m_SwapChain(t_Swap)
		, m_VertexShader(t_Vert)
		, m_FragShader(t_Frag)
	{
		assert(m_Device && m_VertexShader && m_FragShader);

		// Default clear values
		m_ClearValues[0].color = { 0.0f, 0.0f, 0.0f, 1.0f };
		m_ClearValues[1].depthStencil = { 1.0f, ~0U };

		// Initialize the layouts that this subpass will use for descriptors and pipeline creation
		std::vector<Shader*> Shaders = { m_VertexShader.get(), m_FragShader.get() };

		// Add entt callbacks for handling mesh renderers

        //Might want to change this per subpass?
		m_GraphicsPipeline = new GraphicsPipeline(
            Shaders, 
            m_Device->GetVkDevice(),
            VK_POLYGON_MODE_FILL,
            GraphicsPipeline::Depth::ReadWrite,
            VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
            VK_CULL_MODE_BACK_BIT,
            VK_FRONT_FACE_CLOCKWISE);

		PrepareAttachments();
	}

	Subpass::~Subpass()
	{
		assert(m_Device);
		
		if (m_GraphicsPipeline)
		{
			delete m_GraphicsPipeline;
			m_GraphicsPipeline = nullptr;
		}
	}
}
