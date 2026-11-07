#pragma once

#include "Ecs/EntityStorage.h"
#include "FlingMath.h"
#include "Subpass.h"

#include <memory>

namespace Fling
{
	class CommandBuffer;
	class LogicalDevice;
	class Swapchain;
	class IDebugUI;
	class FlingWindow;

	class ImGuiSubpass : public Subpass
	{
	public:
		ImGuiSubpass(
			const LogicalDevice* t_Dev,
			const Swapchain* t_Swap,
			Fling::EntityStorage& t_reg,
			FlingWindow* t_Window,
			VkRenderPass t_GlobalRenderPass,
			std::shared_ptr<Fling::IDebugUI> t_DebugUI,
			std::shared_ptr<Fling::Shader> t_Vert,
			std::shared_ptr<Fling::Shader> t_Frag
		);

		virtual ~ImGuiSubpass();

		void Draw(CommandBuffer& t_CmdBuf, uint32 t_ActiveFrameInFlight, const RenderSnapshot& t_Snapshot, float DeltaTime) override;

		void CreateDescriptorSets(VkDescriptorPool t_Pool) override;

		void PrepareAttachments() override;

		void CreateGraphicsPipeline() override;

		void CleanUp() override;

		void OnSwapchainResized() override;

	private:
		/** The world storage that the editor UI reads and changes. The render passes never see it. */
		Fling::EntityStorage* m_EditorStorage = nullptr;

		void PrepImGuiStyleSettings();

		void CreateUiPipeline();

		void PrepareResources();

		void BuildCommandBuffer(VkCommandBuffer t_commandBuffer);

		void UpdateUniforms();

		struct PushConstBlock
		{
			glm::vec2 scale;
			glm::vec2 translate;
		} pushConstBlock;

		std::unique_ptr<class Buffer> m_vertexBuffer;
		std::unique_ptr<class Buffer> m_indexBuffer;

		VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;
		VkDescriptorSetLayout m_descriptorSetLayout = VK_NULL_HANDLE;
		VkPipelineCache m_pipelineCache  = VK_NULL_HANDLE;
		VkPipelineLayout m_pipelineLayout  = VK_NULL_HANDLE;
		VkPipeline m_pipeLine = VK_NULL_HANDLE;

		VkDescriptorSet m_descriptorSet;

		FlingWindow* m_Window = nullptr;

		VkDeviceMemory m_fontMemory = VK_NULL_HANDLE;
		VkImage m_fontImage = VK_NULL_HANDLE;
		VkImageView m_fontImageView = VK_NULL_HANDLE;
		VkSampler m_sampler = VK_NULL_HANDLE;

		/** Instance of the editor that we will get what commands to build from */
		std::shared_ptr<Fling::IDebugUI> m_DebugUI;

		int32 m_vertexCount = 0;
		int32 m_indexCount = 0;

		VkRenderPass m_GlobalRenderPass = VK_NULL_HANDLE;
	};
}   // namespace Fling