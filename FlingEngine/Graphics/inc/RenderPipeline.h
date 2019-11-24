#pragma once

#include "Subpass.h"
#include "NonCopyable.hpp"
#include <entt/entity/registry.hpp>

namespace Fling
{
	class CommandBuffer;
	class LogicalDevice;
	class Swapchain;
	class FrameBuffer;

	/**
	* @brief	A render pipeline encapsulates the functionality of a 
	* 
	*/
	class RenderPipeline : public NonCopyable
	{
	public:

		RenderPipeline(entt::registry& t_Reg, LogicalDevice* t_dev, Swapchain* t_Swap, std::vector<std::unique_ptr<Subpass>>& t_Subpasses);
		~RenderPipeline();

		void Draw(CommandBuffer& t_CmdBuf, entt::registry& t_Reg);

	private:

		/**
		* @brief	Creates the descriptor pool and the descriptor sets for each sub pass to use
		*/
		void CreateDescriptors(entt::registry& t_Reg);

		std::vector<std::unique_ptr<Subpass>> m_Subpasses;

		std::vector<FrameBuffer*> m_FrameBuffers;

		VkDescriptorPool m_DescriptorPool = VK_NULL_HANDLE;

		const LogicalDevice* m_Device;

		/** Keep track of the swap chain so that we know how many frame buffers to create and what extents to use */
		const Swapchain* m_SwapChain;
	};
}	// namespace Fling