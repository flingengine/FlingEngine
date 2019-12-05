#pragma once

#include "Subpass.h"

namespace Fling
{
	class CommandBuffer;
	class LogicalDevice;
	class FrameBuffer;	
	class MeshRenderer;
	class Swapchain;

	// Uses the MRT shaders (mulitple render targets)
	class OffscreenSubpass : public Subpass
	{
	public:
		OffscreenSubpass(
			const LogicalDevice* t_Dev,
			const Swapchain* t_Swap,
			entt::registry& t_reg,
			std::shared_ptr<Fling::Shader> t_Vert,
			std::shared_ptr<Fling::Shader> t_Frag
		);

		virtual ~OffscreenSubpass();

		FrameBuffer* GetOffscreenFrameBuffer() const { return m_OffscreenFrameBuf; }

		void Draw(CommandBuffer& t_CmdBuf, UINT32 t_ActiveFrameInFlight, entt::registry& t_reg) override;

		void CreateDescriptorSets(VkDescriptorPool t_Pool, entt::registry& t_reg) override;

		void PrepareAttachments() override;

		void CreateGraphicsPipeline() override;

		void GatherPresentDependencies(std::vector<CommandBuffer*>& t_CmdBuffs, std::vector<VkSemaphore>& t_Deps, UINT32 t_ActiveFrameIndex) override;

	private:

		void OnMeshRendererAdded(entt::entity t_Ent, entt::registry& t_Reg, MeshRenderer& t_MeshRend);

		void CreateMeshDescriptorSet(MeshRenderer& t_MeshRend, VkDescriptorPool t_Pool, FrameBuffer& t_FrameBuf);

		// Offscreen resources that we can use to create the G Buffer

		// We need an offscreen semaphore for each possible frame in flight because the swap chain
		// presentation will depend on this command buffer being complete
		std::vector<VkSemaphore> m_OffscreenSemaphores;

		// Offscreen command buffers for populating the GBuffer
		std::vector<CommandBuffer*> m_OffscreenCmdBufs;

		FrameBuffer* m_OffscreenFrameBuf = nullptr;
	};

}   // namespace Fling