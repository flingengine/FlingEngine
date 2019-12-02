#pragma once

#include "Subpass.h"

namespace Fling
{
	class CommandBuffer;
	class LogicalDevice;
	class FrameBuffer;	
	
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
		GeometrySubpass(const LogicalDevice* t_Dev, std::shared_ptr<Fling::Shader> t_Vert, std::shared_ptr<Fling::Shader> t_Frag);

		virtual ~GeometrySubpass();

		void Draw(CommandBuffer& t_CmdBuf, FrameBuffer& t_FrameBuf, entt::registry& t_reg) override;

		void CreateDescriptorSets(VkDescriptorPool t_Pool, FrameBuffer& t_FrameBuf, entt::registry& t_reg) override;

		void PrepareAttachments(FrameBuffer& t_FrameBuffer) override;

	private:

		// #TODO Uniform buffers
		// #TODO Callbacks when mesh renderers are added and removed? 
	};

}   // namespace Fling