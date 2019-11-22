#pragma once

#include "Subpass.h"
#include "NonCopyable.hpp"
#include "FrameBuffer.h"
#include <entt/entity/registry.hpp>

namespace Fling
{
	class CommandBuffer;
	class LogicalDevice;

	/**
	* @brief	A render pipeline encapsulates the functionality of a 
	* 
	*/
	class RenderPipeline : public NonCopyable
	{
	public:

		RenderPipeline(LogicalDevice* t_dev, std::vector<std::unique_ptr<Subpass>>& t_Subpasses);
		~RenderPipeline() = default;

		void Prepare();

		void Draw(CommandBuffer& t_CmdBuf, entt::registry& t_Reg);

	private:
		std::vector<std::unique_ptr<Subpass>> m_Subpasses;

		std::vector<FrameBuffer> m_FrameBuffers;

		VkDescriptorPool m_DescriptorPool = VK_NULL_HANDLE;

		const LogicalDevice* m_Device;
	};
}	// namespace Fling