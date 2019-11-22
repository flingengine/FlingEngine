#pragma once

#include "Subpass.h"
#include "NonCopyable.hpp"
#include "FrameBuffer.h"
#include <entt/entity/registry.hpp>

namespace Fling
{
	class CommandBuffer;

	class RenderPipeline : public NonCopyable
	{
	public:

		RenderPipeline(std::vector<std::unique_ptr<Subpass>>& t_Subpasses);
		~RenderPipeline() = default;

		void Prepare();

		void Draw(CommandBuffer& t_CmdBuf, entt::registry& t_Reg);

	private:
		std::vector<std::unique_ptr<Subpass>> m_Subpasses;

		std::vector<FrameBuffer> m_FrameBuffers;

		VkDescriptorPool m_DescriptorPool = VK_NULL_HANDLE;
	};
}	// namespace Fling