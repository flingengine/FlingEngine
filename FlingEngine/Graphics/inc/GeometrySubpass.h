#pragma once

#include "Subpass.h"

namespace Fling
{
	class CommandBuffer;
	class LogicalDevice;
	class FrameBuffer;	
	class MeshRenderer;
	class Swapchain;
	class GraphicsPipeline;
	class Model;
	class Buffer;

	struct DeferredLight
	{
		glm::vec4 position;
		glm::vec3 color;
		float radius;
	};

	/**
	* @brief	The geometry subpass is in charge of sending the geometry portion of 
	*			the Deferred pipeline to the GPU. This includes frame buffer attachments for 
	*			albedo, normals, and depth as well some actual mesh data via a Uniform buffer
	*			Uses the Deferred shaders
	*/
	class GeometrySubpass : public Subpass
	{
	public:
		GeometrySubpass(
			const LogicalDevice* t_Dev,
			const Swapchain* t_Swap,
			entt::registry& t_reg,
			FrameBuffer* t_OffscreenDep,
			std::shared_ptr<Fling::Shader> t_Vert,
			std::shared_ptr<Fling::Shader> t_Frag
		);

		virtual ~GeometrySubpass();

		void Draw(CommandBuffer& t_CmdBuf, UINT32 t_ActiveFrameInFlight, entt::registry& t_reg) override;

		void CreateDescriptorSets(VkDescriptorPool t_Pool, entt::registry& t_reg) override;

		/** 
		* @param t_FrameBuffer	The swap chain frame buffer
		*/
		void CreateGraphicsPipeline() override;

	private:

		void UpdateLightingUBO(UINT32 t_ActiveFrame);

		// Global render pass for frame buffer writes
		// A quad model for displaying shit
		std::shared_ptr<Model> m_QuadModel;

		/** The offscreen frame buffer that has the G Buffer attachments */
		FrameBuffer* m_OffscreenFrameBuf = nullptr;

		// UBO for Lights that will be sent to the frag shader
		// Use 6 for now just because itll be easier to setup deferred
		struct 
		{
			DeferredLight lights[6];
			glm::vec4 viewPos;
		} uboFragmentLights;

		// Descriptor sets and Uniform buffers -- one per swap image
		std::vector<VkDescriptorSet> m_DescriptorSets;
		std::vector<Buffer*> m_LightingUBOs;

		std::vector<Buffer*> m_QuadUBO;
		std::vector<VkDescriptorSet> m_QuadDescriptor;

		// #TODO Callbacks when Lights are added
	};

}   // namespace Fling