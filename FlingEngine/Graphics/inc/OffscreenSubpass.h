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

	/** UBO for mesh data */
	struct alignas(16) OffscreenUBO
	{
		glm::mat4 Projection;
		glm::mat4 Model;
		glm::mat4 View;
		glm::vec3 ObjPos;
	};

	// Uses the MRT shaders (mulitple render targets)
	class OffscreenSubpass : public Subpass
	{
	public:
		OffscreenSubpass(
			const LogicalDevice* t_Dev,
			const Swapchain* t_Swap,
			entt::registry& t_reg,
			FirstPersonCamera* t_Cam,
			std::shared_ptr<Fling::Shader> t_Vert,
			std::shared_ptr<Fling::Shader> t_Frag
		);

		virtual ~OffscreenSubpass();

		FrameBuffer* GetOffscreenFrameBuffer() const { return m_OffscreenFrameBuf; }

		void Draw(CommandBuffer& t_CmdBuf, VkFramebuffer t_PresentFrameBuf, uint32 t_ActiveSwapImage, entt::registry& t_reg, float DeltaTime) override;

		void CreateDescriptorSets(VkDescriptorPool t_Pool, entt::registry& t_reg) override;

		void PrepareAttachments() override;

		void CreateGraphicsPipeline() override;

		void GatherPresentDependencies(
			std::vector<CommandBuffer*>& t_CmdBuffs,
			std::vector<VkSemaphore>& t_Deps,
			uint32 t_ActiveFrameIndex,
			uint32 t_CurrentFrameInFlight) override;

		void CleanUp(entt::registry& t_reg) override;

	private:

		void OnMeshRendererAdded(entt::entity t_Ent, entt::registry& t_Reg, MeshRenderer& t_MeshRend);

		void OnMeshRendererDestroyed(entt::registry& t_Reg, MeshRenderer& t_MeshRend);

		void CreateMeshDescriptorSet(MeshRenderer& t_MeshRend, VkDescriptorPool t_Pool, FrameBuffer& t_FrameBuf);

		void BuildOffscreenCommandBuffer(entt::registry& t_reg, uint32 t_ActiveFrameInFlight);

		// We need an offscreen semaphore for each possible frame in flight because the swap chain
		// presentation will depend on this command buffer being complete
		std::vector<VkSemaphore> m_OffscreenSemaphores;

		VkCommandPool m_CommandPool = VK_NULL_HANDLE;

		// Offscreen command buffers for populating the GBuffer
		std::vector<CommandBuffer*> m_OffscreenCmdBufs;

		FrameBuffer* m_OffscreenFrameBuf = nullptr;

		const FirstPersonCamera* m_Camera;

		VkDescriptorPool m_DescriptorPool = VK_NULL_HANDLE;
	};
}   // namespace Fling