#pragma once

#include "Subpass.h"
#include "FlingMath.h"
#include <memory>
#include <vector>

namespace Fling
{
	class CommandBuffer;
	class LogicalDevice;
	class FrameBuffer;	
	struct MeshRenderer;
	class Swapchain;
	class FirstPersonCamera;

	class Buffer;

	/** UBO for mesh data. Written only when the mesh's transform changes. */
	struct alignas(16) OffscreenUBO
	{
		glm::mat4 Model;
		glm::vec3 ObjPos;
	};

	/** UBO for camera data. All meshes share one, written once per frame. */
	struct alignas(16) OffscreenCameraUBO
	{
		glm::mat4 Projection;
		glm::mat4 View;
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

		void Draw(CommandBuffer& t_CmdBuf, uint32 t_ActiveSwapImage, entt::registry& t_reg, float DeltaTime) override final;

		void PrepareAttachments() override final;

		void CreateGraphicsPipeline() override final;

		void GatherPresentDependencies(
			std::vector<CommandBuffer*>& t_CmdBuffs,
			std::vector<VkSemaphore>& t_Deps,
			uint32 t_ActiveFrameIndex,
			uint32 t_CurrentFrameInFlight) override final;

		void CleanUp(entt::registry& t_reg) override final;

		virtual void OnSwapchainResized(entt::registry& t_reg) override final;

	private:

		void OnMeshRendererAdded(entt::entity t_Ent, entt::registry& t_Reg, MeshRenderer& t_MeshRend);

		/**
		* The model or material changed. The descriptor set update must wait for
		* the next Draw, because entt calls this before it writes the new value.
		*/
		void OnMeshRendererReplaced(entt::entity t_Ent, entt::registry& t_Reg, MeshRenderer& t_MeshRend);

		/** Any change to the set of drawn entities. The command buffers must be recorded again. */
		void OnDrawListChanged(entt::entity t_Ent, entt::registry& t_Reg);

		void CreateMeshDescriptorSet(MeshRenderer& t_MeshRend);

		/** Write the UBO of each mesh whose transform changed since its last upload */
		void UpdateMeshUniformBuffers(entt::registry& t_reg);

		/** Record the draw calls for every mesh into the given command buffer */
		void BuildOffscreenCommandBuffer(CommandBuffer& t_CmdBuf, entt::registry& t_reg);

		/** Make each offscreen command buffer record again on its next use */
		inline void InvalidateCommandBuffers() { ++m_DrawListVersion; }

		// We need an offscreen semaphore for each possible frame in flight because the swap chain
		// presentation will depend on this command buffer being complete
		std::vector<VkSemaphore> m_OffscreenSemaphores;

		VkCommandPool m_CommandPool = VK_NULL_HANDLE;

		// Offscreen command buffers for populating the GBuffer
		std::vector<CommandBuffer*> m_OffscreenCmdBufs;

		FrameBuffer* m_OffscreenFrameBuf = nullptr;

		const FirstPersonCamera* m_Camera;

		VkDescriptorPool m_DescriptorPool = VK_NULL_HANDLE;

		/** Camera UBO that every mesh descriptor set points to */
		Buffer* m_CameraUboBuffer = nullptr;

		/**
		* Goes up by one each time the draw list changes. A command buffer records
		* again only when its recorded version is different from this value.
		*/
		uint64 m_DrawListVersion = 1;

		/** The m_DrawListVersion that each offscreen command buffer holds. 0 means not recorded. */
		std::vector<uint64> m_RecordedDrawListVersions;

		/** Entities with a replaced MeshRenderer. Their descriptor sets update on the next Draw. */
		std::vector<entt::entity> m_ReplacedMeshes;
	};
}   // namespace Fling