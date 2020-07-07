#pragma once

#include "Subpass.h"

namespace Fling
{
	class CommandBuffer;
	class LogicalDevice;
	class FrameBuffer;	
	struct MeshRenderer;
	class Swapchain;
	class FirstPersonCamera;

	class DebugSubpass : public Subpass
	{
	public:
		DebugSubpass(
			const LogicalDevice* t_Dev,
			const Swapchain* t_Swap,
			entt::registry& t_reg,
			VkRenderPass t_GlobalRenderPass,
			FirstPersonCamera* t_Cam,
			std::shared_ptr<Fling::Shader> t_Vert,
			std::shared_ptr<Fling::Shader> t_Frag
		);

		virtual ~DebugSubpass();

		void Draw(CommandBuffer& t_CmdBuf, VkFramebuffer t_PresentFrameBuf, uint32 t_ActiveFrameInFlight, entt::registry& t_reg, float DeltaTime) override;

		void CreateDescriptorSets(VkDescriptorPool t_Pool, entt::registry& t_reg) override;

		void PrepareAttachments() override;

		void CreateGraphicsPipeline() override;

		void CleanUp(entt::registry& t_reg) override;

	private:

		void OnMeshRendererAdded(entt::entity t_Ent, entt::registry& t_Reg, MeshRenderer& t_MeshRend);

		void CreateMeshDescriptorSet(MeshRenderer& t_MeshRend);

		VkRenderPass m_GlobalRenderPass = VK_NULL_HANDLE;

		const FirstPersonCamera* m_Camera;

		struct DebugUBO
		{
			glm::mat4 Projection;
			glm::mat4 Model;
		} m_Ubo;

		VkDescriptorPool m_DescriptorPool = VK_NULL_HANDLE;
	};
}   // namespace Fling