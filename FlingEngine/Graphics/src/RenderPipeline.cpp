#include "RenderPipeline.h"
#include "CommandBuffer.h"
#include "LogicalDevice.h"

namespace Fling
{
	RenderPipeline::RenderPipeline(LogicalDevice* t_Dev, std::vector<std::unique_ptr<Subpass>>& t_Subpasses)
		: m_Subpasses ( std::move(t_Subpasses) )
		, m_Device(t_Dev)
	{
		// build Render pass here ----
		// For each subpass 
				// give it a pointer to something so that we know what attachments we need
		//

		// Build frame buffers -----
			// For each subpass 
				// pass in a pointer to the vector or VkImageViews 
			// VkCreateFrameBufferino with attachment size

		// Build Descriptor sets -------
		// Create descripor set POOL	
		// For each subpass
				// Create descrpiptor sets given a pointer to a pool

		// Build command buffers!
	}

	RenderPipeline::~RenderPipeline()
	{
		assert(m_Device);

		// Cleanup all subpasses
		m_Subpasses.clear();

		// Clear each frame buffer
		vkDestroyRenderPass(m_Device->GetVkDevice(), m_RenderPass, nullptr);
	}

	void RenderPipeline::Draw(CommandBuffer& t_CmdBuf, entt::registry& t_Reg)
	{
		// Vk Bgein command buffer
		// Begin render pass info struct
		// Set clear values
		// vkCmdBeginRenderPass
		// vkCmdSetViewport

		// vkCmdSetScissor

		for (const auto& sub : m_Subpasses)
		{
			// Draw each subpass
			sub->Draw(t_CmdBuf, t_Reg);
		}

		// vkEndRenderPass
		// vkEndCommandBuffer
	}
}