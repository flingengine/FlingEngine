#pragma once

#include "Shader.h"
#include "NonCopyable.hpp"

#include <entt/entity/registry.hpp>

namespace Fling
{
	class CommandBuffer;
	class LogicalDevice;

	class Subpass : public NonCopyable
	{
	public:
		Subpass(LogicalDevice* t_Dev, std::shared_ptr<Fling::Shader> t_Vert, std::shared_ptr<Fling::Shader> t_Frag);
		
		virtual ~Subpass() = default;

		/** create and map any uniform buffers that we may need for this subpass */
		virtual void Prepare() {}

		/** Add any attachments to the frame buffer that this subpass may need */
		virtual void PrepareAttachments(class FrameBuffer& t_FrameBuffer) {}

		virtual void Draw(CommandBuffer& t_CmdBuf, entt::registry& t_reg) = 0;

		// Given Swap chain image
		virtual void CreateDescriptorSets(VkImage t_SwapChainImg, entt::registry& t_reg) = 0;

		VkDescriptorSetLayout GetDescriptorLayout() const { return m_DescriptorLayout; }

	protected:

		LogicalDevice* m_Device = nullptr;

		std::shared_ptr<Fling::Shader> m_VertexShader;
		
		std::shared_ptr<Fling::Shader> m_FragShader;

		VkDescriptorSetLayout m_DescriptorLayout = VK_NULL_HANDLE;
	};
}