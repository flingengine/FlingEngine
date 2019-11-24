#include "RenderPipeline.h"
#include "CommandBuffer.h"
#include "LogicalDevice.h"
#include "GraphicsHelpers.h"
#include "SwapChain.h"
#include "FrameBuffer.h"

namespace Fling
{
	RenderPipeline::RenderPipeline(entt::registry& t_Reg, LogicalDevice* t_Dev, Swapchain* t_Swap, std::vector<std::unique_ptr<Subpass>>& t_Subpasses)
		: m_Subpasses ( std::move(t_Subpasses) )
		, m_Device(t_Dev)
		, m_SwapChain(t_Swap)
	{
		assert(m_Device && m_SwapChain);

		// Build a frame buffer for each swap chain image
		for (size_t i = 0; i < m_SwapChain->GetImageCount(); ++i)
		{
			m_FrameBuffers.emplace_back(new FrameBuffer(m_Device->GetVkDevice()));
		}

		// Let each subpass add their own attachments to the frame buffer ----
		for (const auto& pass : m_Subpasses)
		{
			for (auto& frameBuffer : m_FrameBuffers)
			{
				assert(frameBuffer);
				// Add any attachments and samplers to each frame buffer
				pass->PrepareAttachments(*frameBuffer);
			}
		}

		// Now that we have all the attachments on the frame buffers, build the render passes 
		for (auto& frameBuffer : m_FrameBuffers)
		{
			assert(frameBuffer);
			VK_CHECK_RESULT(frameBuffer->CreateRenderPass());
		}

		// Build Descriptor sets -------
		CreateDescriptors(t_Reg);

		// Build command buffers!
	}

	RenderPipeline::~RenderPipeline()
	{
		assert(m_Device);

		for (auto& buf : m_FrameBuffers)
		{
			if (buf)
			{
				delete buf;
			}
		}
		m_FrameBuffers.clear();

		// Cleanup subpasses
		m_Subpasses.clear();
	}

	void RenderPipeline::Draw(CommandBuffer& t_CmdBuf, entt::registry& t_Reg)
	{
		assert(!m_Subpasses.empty() && "Render pipeline should contain at least one sub-pass");

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

		//t_CmdBuf.EndRenderPass();

		// vkEndRenderPass
		// vkEndCommandBuffer
	}

	void RenderPipeline::CreateDescriptors(entt::registry& t_Reg)
	{
		// Create the descriptor pool for us to use -------
		const UINT32 SwapImageCount = static_cast<UINT32>(m_SwapChain->GetImageCount());
		UINT32 DescriptorCount = 128;

		std::vector<VkDescriptorPoolSize> poolSizes =
		{
			Initializers::DescriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, DescriptorCount),
			Initializers::DescriptorPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, DescriptorCount),
			Initializers::DescriptorPoolSize(VK_DESCRIPTOR_TYPE_SAMPLER, DescriptorCount),
			Initializers::DescriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, DescriptorCount),
			Initializers::DescriptorPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, DescriptorCount)
		};

		VkDescriptorPoolCreateInfo poolInfo = {};
		poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		poolInfo.poolSizeCount = static_cast<UINT32>(poolSizes.size());
		poolInfo.pPoolSizes = poolSizes.data();
		poolInfo.maxSets = SwapImageCount;

		VK_CHECK_RESULT(vkCreateDescriptorPool(m_Device->GetVkDevice(), &poolInfo, nullptr, &m_DescriptorPool));

		// Build all the descriptor SETS in each subpass
		assert(!m_Subpasses.empty() && "Render pipeline should contain at least one sub-pass");

		for (auto& pass : m_Subpasses)
		{
			pass->CreateDescriptorSets(m_DescriptorPool, m_FrameBuffers, t_Reg);
		}

	}
}