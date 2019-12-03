#pragma once

#include "FlingVulkan.h"

namespace Fling
{
	class LogicalDevice;

	// #TODO Pipeline state class definition
	// #TODO Resource binding state class definition

    /**
     * @brief Encapsulates functionality of a Vulkan Command buffer
     * 
     */
    class CommandBuffer
    {
	public:
		CommandBuffer(LogicalDevice* t_Device, VkCommandPool t_CmdPool);
		~CommandBuffer();

		inline VkCommandBuffer GetHandle() const { return m_Handle; }
		inline const LogicalDevice* GetDevice() const { return m_Device; }
		inline const VkCommandPool& GetPoolHandle() const { return m_Pool; }

		/** Begin recording for this command buffer */
		void Begin();

		void BeginRenderPass();

		void NextSubpass();

		void BindPipeline(VkPipelineBindPoint t_BindPoint, VkPipeline t_Pipeline);

		void SetViewport(UINT32 first_viewport, const std::vector<VkViewport>& viewports);

		void SetScissor(UINT32 first_scissor, const std::vector<VkRect2D> &scissors);

		void EndRenderPass();

		/** Stop recording commands to the command buffer */
		void End();

	private:
		const LogicalDevice* m_Device;

		const VkCommandPool m_Pool;

		VkCommandBuffer m_Handle = VK_NULL_HANDLE;
    };
}   // namespace Fling