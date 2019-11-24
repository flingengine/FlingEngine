#include "pch.h"
#include "CommandBuffer.h"
#include "LogicalDevice.h"
#include "GraphicsHelpers.h"

namespace Fling
{
	CommandBuffer::CommandBuffer(LogicalDevice* t_Device, VkCommandPool t_CmdPool)
		: m_Device(t_Device)
		, m_Pool(t_CmdPool)
	{
		assert(m_Device);
		VkCommandBufferAllocateInfo allocate_info{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };

		allocate_info.commandPool = m_Pool;
		allocate_info.commandBufferCount = 1;
		allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

		VkResult result = vkAllocateCommandBuffers(m_Device->GetVkDevice(), &allocate_info, &m_Handle);

		if (result != VK_SUCCESS)
		{
			F_LOG_ERROR("Failed to create command buffers");
		}
	}

	CommandBuffer::~CommandBuffer()
	{
		assert(m_Device);

		if (m_Handle != VK_NULL_HANDLE)
		{
			vkFreeCommandBuffers(m_Device->GetVkDevice(), m_Pool, 1, &m_Handle);
		}
	}
	
	void CommandBuffer::Begin()
	{
		VkCommandBufferBeginInfo beginInfo = {};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
		beginInfo.pInheritanceInfo = nullptr;

		VK_CHECK_RESULT(vkBeginCommandBuffer(GetHandle(), &beginInfo));

	}

	void CommandBuffer::SetViewport(UINT32 first_viewport, const std::vector<VkViewport>& viewports)
	{
		vkCmdSetViewport(GetHandle(), first_viewport, to_u32(viewports.size()), viewports.data());
	}

	void CommandBuffer::SetScissor(UINT32 first_scissor, const std::vector<VkRect2D>& scissors)
	{
		vkCmdSetScissor(GetHandle(), first_scissor, to_u32(scissors.size()), scissors.data());
	}

	void CommandBuffer::EndRenderPass()
	{
		vkCmdEndRenderPass(GetHandle());
	}

	void CommandBuffer::End()
	{
		vkEndCommandBuffer(GetHandle());
	}
}