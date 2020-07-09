#pragma once

#include "FlingVulkan.h"

namespace Fling
{
	class LogicalDevice;
	class FrameBuffer;

	// #TODO Pipeline state class definition (just grab it from the official vulkan samples)
	// #TODO Resource binding state class definition

    /**
     * @brief Encapsulates functionality of a Vulkan Command buffer
     * 
     */
    class CommandBuffer
    {
	public:
		enum class ResetMode
		{
			ResetPool,
			ResetIndividually,
			AlwaysAllocate,
		};

		enum class State
		{
			Invalid,
			Initial,
			Recording,
			Executable,
		};

		CommandBuffer(const LogicalDevice* t_Device, VkCommandPool t_CmdPool);
		~CommandBuffer();

		inline VkCommandBuffer GetHandle() const { return m_Handle; }
		inline const LogicalDevice* GetDevice() const { return m_Device; }
		inline const VkCommandPool& GetPoolHandle() const { return m_Pool; }

		/** Begin recording for this command buffer */
		void Begin(VkCommandBufferUsageFlagBits t_Usage = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT);

		void BeginRenderPass(const FrameBuffer& t_frameBuf, const std::vector<VkClearValue>& t_ClearVales);

		void NextSubpass();

		void BindPipeline(VkPipelineBindPoint t_BindPoint, VkPipeline t_Pipeline);

		void SetViewport(uint32 first_viewport, const std::vector<VkViewport>& viewports);

		void SetScissor(uint32 first_scissor, const std::vector<VkRect2D> &scissors);

		void EndRenderPass();

		/** Stop recording commands to the command buffer */
		void End();

		inline bool IsRecording() const { return m_State == State::Recording; }

	private:
		const LogicalDevice* m_Device;

		const VkCommandPool m_Pool;

		VkCommandBuffer m_Handle = VK_NULL_HANDLE;

		// Keep track of the state of this command buffer
		State m_State = State::Initial;

    };
}   // namespace Fling