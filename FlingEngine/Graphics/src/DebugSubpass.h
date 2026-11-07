#pragma once

#include "FlingMath.h"
#include "Subpass.h"

#include <memory>

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
			Fling::EntityStorage& t_reg,
			VkRenderPass t_GlobalRenderPass,
			FirstPersonCamera* t_Cam,
			std::shared_ptr<Fling::Shader> t_Vert,
			std::shared_ptr<Fling::Shader> t_Frag
		);

		virtual ~DebugSubpass();

		void Draw(CommandBuffer& t_CmdBuf, uint32 t_ActiveFrameInFlight, Fling::EntityStorage& t_reg, float DeltaTime) override;

		void CreateDescriptorSets(VkDescriptorPool t_Pool, Fling::EntityStorage& t_reg) override;

		void PrepareAttachments() override;

		void CreateGraphicsPipeline() override;

		void CleanUp(Fling::EntityStorage& t_reg) override;

	private:

		void OnMeshRendererAdded(Fling::EntityStorage& t_Reg, Fling::Entity t_Ent);

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