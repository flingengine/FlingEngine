#include "pch.h"
#include "CommandBuffer.h"
#include "LogicalDevice.h"
#include "GraphicsHelpers.h"
#include "FrameBuffer.h"

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
	
	void CommandBuffer::Begin(VkCommandBufferUsageFlagBits t_Usage)
	{
		assert(!IsRecording());
		m_State = State::Recording;

		VkCommandBufferBeginInfo beginInfo = {};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		//beginInfo.flags = t_Usage;
		beginInfo.pInheritanceInfo = nullptr;

		VK_CHECK_RESULT(vkBeginCommandBuffer(GetHandle(), &beginInfo));
	}

	void CommandBuffer::BeginRenderPass(FrameBuffer& t_frameBuf, const std::vector<VkClearValue>& t_ClearVales)
	{
		VkRenderPassBeginInfo begin_info{ VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
		// Frame buf info
		begin_info.renderPass = t_frameBuf.GetRenderPassHandle();
		begin_info.framebuffer = t_frameBuf.GetHandle();

		// Render area
		begin_info.renderArea.offset = { 0, 0 };
		begin_info.renderArea.extent.width = t_frameBuf.GetWidth();
		begin_info.renderArea.extent.height = t_frameBuf.GetHeight();

		// Clear vals
		begin_info.clearValueCount = to_u32(t_ClearVales.size());
		begin_info.pClearValues = t_ClearVales.data();

		vkCmdBeginRenderPass(GetHandle(), &begin_info, VK_SUBPASS_CONTENTS_INLINE);
	}

	void CommandBuffer::NextSubpass()
	{
		// track pipeline state ?

		vkCmdNextSubpass(GetHandle(), VK_SUBPASS_CONTENTS_INLINE);
	}

	void CommandBuffer::BindPipeline(VkPipelineBindPoint t_BindPoint, VkPipeline t_Pipeline)
	{
		vkCmdBindPipeline(GetHandle(), t_BindPoint, t_Pipeline);
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
		assert(IsRecording() && "Command buffer is not recording, please call begin before end");

		vkEndCommandBuffer(GetHandle());

		m_State = State::Executable;
	}
}