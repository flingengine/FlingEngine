#pragma once

#include "FlingMath.h"
#include "GpuMesh.h"
#include "MeshProxyTable.h"
#include "Subpass.h"

#include <memory>
#include <vector>

namespace Fling
{
	class CommandBuffer;
	class LogicalDevice;
	class FrameBuffer;	
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
			FirstPersonCamera* t_Cam,
			std::shared_ptr<Fling::Shader> t_Vert,
			std::shared_ptr<Fling::Shader> t_Frag
		);

		virtual ~OffscreenSubpass();

		FrameBuffer* GetOffscreenFrameBuffer() const { return m_OffscreenFrameBuf; }

		void Draw(CommandBuffer& t_CmdBuf, uint32 t_ActiveSwapImage, const RenderSnapshot& t_Snapshot, float DeltaTime) override final;

		void PrepareAttachments() override final;

		void CreateGraphicsPipeline() override final;

		void GatherPresentDependencies(
			std::vector<CommandBuffer*>& t_CmdBuffs,
			std::vector<VkSemaphore>& t_Deps,
			uint32 t_ActiveFrameIndex,
			uint32 t_CurrentFrameInFlight) override final;

		void CleanUp() override final;

		virtual void OnSwapchainResized() override final;

	private:

		/**
		* Match the proxies to the snapshot: release the resources of meshes that are
		* gone, and make them for new meshes and meshes whose paths changed.
		*/
		void SyncProxies(const RenderSnapshot& t_Snapshot);

		/** Retire the resources of one proxy. ReleaseRetiredMeshResources frees them on the next Draw. */
		void RetireGpuMesh(GpuMesh& t_Mesh);

		/** Free the descriptor sets and UBOs of destroyed meshes */
		void ReleaseRetiredMeshResources();

		void CreateMeshDescriptorSet(GpuMesh& t_Mesh);

		/** Write the UBO of each mesh whose world matrix changed since its last upload */
		void UpdateMeshUniformBuffers(const RenderSnapshot& t_Snapshot);

		/** Record the draw calls for every drawn proxy into the given command buffer */
		void BuildOffscreenCommandBuffer(CommandBuffer& t_CmdBuf);

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

		/** One proxy for each mesh in the snapshot */
		MeshProxyTable m_Proxies;

		/** The GPU side of each proxy, indexed by slot */
		std::vector<GpuMesh> m_GpuMeshes;

		/** The proxy slot of each snapshot mesh, from the last SyncProxies */
		std::vector<uint32> m_ItemSlots;

		/**
		* Descriptor sets of meshes that are gone. They are freed at the start of the next
		* Draw. The previous frame has finished on the GPU then, and every command buffer
		* records again.
		*/
		std::vector<VkDescriptorSet> m_RetiredDescriptorSets;

		/** UBOs of destroyed meshes. Deleted at the same time as m_RetiredDescriptorSets. */
		std::vector<Buffer*> m_RetiredUniformBuffers;
	};
}   // namespace Fling