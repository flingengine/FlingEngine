#pragma once

#include "Shader.h"
#include "NonCopyable.hpp"

#include <entt/entity/registry.hpp>

namespace Fling
{
	class CommandBuffer;
	class LogicalDevice;
	class FrameBuffer;
	class Swapchain;
	class GraphicsPipeline;

	/**
	* @breif	A subpass represents one part of a RenderPipeline. Each subpass should 
	*			can add attachments to the frame buffer, build it's own command buffers, 
	*			and create its own descriptors. When overriding this class, add any additional
	*			uniform buffers or bindings you may need into the child class. 
	* 
	* @see GeometrySubpass for an example
	*/
	class Subpass : public NonCopyable
	{
	public:
		Subpass(const LogicalDevice* t_Dev, const Swapchain* t_Swap, std::shared_ptr<Fling::Shader> t_Vert, std::shared_ptr<Fling::Shader> t_Frag);
		
		virtual ~Subpass();

		/** Add any attachments to a frame buffer that this subpass may need */
		virtual void PrepareAttachments(FrameBuffer& t_FrameBuffer) {}

		virtual void CreateGraphicsPipeline(FrameBuffer& t_FrameBuffer) = 0;

		virtual void Draw(CommandBuffer& t_CmdBuf, FrameBuffer& t_FrameBuf, entt::registry& t_reg) = 0;

		/**
		* @brief	Given the frame buffers and the registry, create any descriptor sets that we may need
		*			Assumes that the frame buffer has been prepared with it's attachments already.
		*/
		virtual void CreateDescriptorSets(VkDescriptorPool t_Pool, FrameBuffer& t_FrameBuf, entt::registry& t_reg) = 0;

		VkDescriptorSetLayout GetDescriptorLayout() const noexcept { return m_DescriptorLayout; }

		inline GraphicsPipeline* GetGraphicsPipeline() const noexcept { return m_GraphicsPipeline; }

	protected:

		// Get default graphics Pipeline

		const LogicalDevice* m_Device;
		const Swapchain* m_SwapChain;

		std::shared_ptr<Fling::Shader> m_VertexShader;
		
		std::shared_ptr<Fling::Shader> m_FragShader;

		/** Layouts created in the constructor via shader reflection */
		VkDescriptorSetLayout m_DescriptorLayout = VK_NULL_HANDLE;
		VkPipelineLayout m_PipelineLayout = VK_NULL_HANDLE;

		GraphicsPipeline* m_GraphicsPipeline = nullptr;
	};
}