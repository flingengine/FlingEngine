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

	/** UBO for mesh data */
	struct alignas(16) GeometryUBO
	{
		glm::mat4 Model;
		glm::mat4 Projection;
		glm::mat4 View;
		glm::vec3 CamPos;
		glm::vec3 ObjPos;
	};

	/**
	* @brief	The geometry subpass is in charge of sending the geometry portion of 
	*			the Deferred pipeline to the GPU. This includes frame buffer attachments for 
	*			albedo, normals, and depth as well some actual mesh data via a Uniform buffer
	*/
	class GeometrySubpass : public Subpass
	{
	public:
		GeometrySubpass(
			const LogicalDevice* t_Dev,
			const Swapchain* t_Swap,
			entt::registry& t_reg,
			std::shared_ptr<Fling::Shader> t_Vert,
			std::shared_ptr<Fling::Shader> t_Frag
		);

		virtual ~GeometrySubpass();

		void Draw(CommandBuffer& t_CmdBuf, UINT32 t_ActiveFrameInFlight, FrameBuffer& t_FrameBuf, entt::registry& t_reg) override;

		void CreateDescriptorSets(VkDescriptorPool t_Pool, FrameBuffer& t_FrameBuf, entt::registry& t_reg) override;

		void PrepareAttachments(FrameBuffer& t_FrameBuffer) override;

		void CreateGraphicsPipeline(FrameBuffer& t_FrameBuffer) override;

	private:

		void OnMeshRendererAdded(entt::entity t_Ent, entt::registry& t_Reg, MeshRenderer& t_MeshRend);

		void OnMeshRendererRemoved(entt::entity t_Ent, entt::registry& t_Reg);

		void CreateMeshDescriptorSet(MeshRenderer& t_MeshRend, VkDescriptorPool t_Pool, FrameBuffer& t_FrameBuf);

		// #TODO Uniform buffers
		// #TODO Callbacks when mesh renderers are added and removed? 
	};

}   // namespace Fling