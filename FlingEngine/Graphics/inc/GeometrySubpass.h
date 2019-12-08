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

	/**
	* @brief	Settings for the max directional lights and max point lights.
	*			These settings are used 
	* @todo		Ideally we would load these settings in from the game config file
	*/
	struct DeferredLightSettings
	{
		/** Dir Lights */
		static const UINT32 MaxDirectionalLights = 32;

		/** Point Lights */
		static const UINT32 MaxPointLights = 32;
	};

	/** Uniform buffer for passing lights to our final screen pass */
	struct LightingUbo
	{
		alignas(4) UINT32 DirLightCount = 0;
		alignas(4) UINT32 PointLightCount = 0;

		alignas(16) DirectionalLight DirLightBuffer[Lighting::MaxDirectionalLights] = {};

		alignas(16) PointLight PointLightBuffer[Lighting::MaxPointLights] = {};
	};

	struct CameraInfoUbo
	{
		glm::mat4 Projection;
		glm::mat4 ModelView;
		glm::vec3 CamPos = {};
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
			VkRenderPass t_GlobalRenderPass,
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
		std::shared_ptr<Model> m_QuadModel;

		VkRenderPass m_GlobalRenderPass = VK_NULL_HANDLE;

		const FirstPersonCamera* m_Camera;

		/** The offscreen frame buffer that has the G Buffer attachments */
		FrameBuffer* m_OffscreenFrameBuf = nullptr;

		// Descriptor sets and Uniform buffers -- one per swap image
		std::vector<VkDescriptorSet> m_DescriptorSets;
		std::vector<Buffer*> m_LightingUboBuffers;
		std::vector<Buffer*> m_CameraUboBuffers;

		std::vector<Buffer*> m_QuadUboBuffer;

		LightingUbo m_LightingUBO = {};

		CameraInfoUbo m_CamInfoUBO = {};
	};
}   // namespace Fling