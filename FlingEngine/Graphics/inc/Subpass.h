#pragma once

#include "Shader.h"
#include "NonCopyable.hpp"

#include <entt/entity/registry.hpp>

namespace Fling
{
	class CommandBuffer;
	class LogicalDevice;
	class FrameBuffer;

	class Subpass : public NonCopyable
	{
	public:
		Subpass(LogicalDevice* t_Dev, std::shared_ptr<Fling::Shader> t_Vert, std::shared_ptr<Fling::Shader> t_Frag);
		
		virtual ~Subpass() = default;

		/** create and map any uniform buffers that we may need for this subpass */
		virtual void Prepare() {}

		/** Add any attachments to a frame buffer that this subpass may need */
		virtual void PrepareAttachments(FrameBuffer& t_FrameBuffer) {}

		virtual void Draw(CommandBuffer& t_CmdBuf, entt::registry& t_reg) = 0;

		/**
		* @brief	Given the frame buffers and the registry, create any descriptor sets that we may need 
		*/
		virtual void CreateDescriptorSets(VkDescriptorPool t_Pool, const std::vector<FrameBuffer*>& t_FrameBufs, entt::registry& t_reg) = 0;

		VkDescriptorSetLayout GetDescriptorLayout() const { return m_DescriptorLayout; }

	protected:

		LogicalDevice* m_Device = nullptr;

		std::shared_ptr<Fling::Shader> m_VertexShader;
		
		std::shared_ptr<Fling::Shader> m_FragShader;

		VkDescriptorSetLayout m_DescriptorLayout = VK_NULL_HANDLE;
		VkPipelineLayout m_PipelineLayout = VK_NULL_HANDLE;
	};
}