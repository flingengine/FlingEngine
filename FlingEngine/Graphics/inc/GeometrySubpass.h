#pragma once

#include "Subpass.h"

#include "Lighting/DirectionalLight.hpp"
#include "Lighting/PointLight.hpp"
#include "Lighting/Lighting.hpp"

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
	class FirstPersonCamera;

	struct DeferredLightSettings
	{
		/** Dir Lights */
		static const UINT32 MaxDirectionalLights = 32;

		/** Point Lights */
		static const UINT32 MaxPointLights = 32;
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
			FirstPersonCamera* t_Cam,
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

		void OnPointLightAdded(entt::entity t_Ent, entt::registry& t_Reg, PointLight& t_Light);

		void UpdateLightingUBO(entt::registry& t_Reg, UINT32 t_ActiveFrame);

		// Global render pass for frame buffer writes
		// A quad model for displaying shit
		std::shared_ptr<Model> m_QuadModel;

		const FirstPersonCamera* m_Camera;

		/** The offscreen frame buffer that has the G Buffer attachments */
		FrameBuffer* m_OffscreenFrameBuf = nullptr;

		// Descriptor sets and Uniform buffers -- one per swap image
		std::vector<VkDescriptorSet> m_DescriptorSets;
		std::vector<Buffer*> m_LightingUboBuffers;

		LightingUbo m_LightingUBO = {};

		std::vector<Buffer*> m_QuadUboBuffer;
		std::vector<VkDescriptorSet> m_QuadDescriptor;

		// #TODO Callbacks when Lights are added
	};

}   // namespace Fling