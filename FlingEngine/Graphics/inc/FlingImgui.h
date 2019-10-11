#pragma once 

#include <imgui.h>
#include <algorithm>

#include "Renderer.h"
#include "FlingVulkan.h"
#include "LogicalDevice.h"
#include "GraphicsHelpers.h"
#include "ResourceManager.h"
#include "File.h"
#include "SwapChain.h"

namespace Fling 
{
	class FlingImgui
	{
	public:
		// UI params are set via push constants
		struct PushConstBlock {
			glm::vec2 scale;
			glm::vec2 translate;
		} pushConstBlock;


		struct UISettings
		{
			bool mouseClickLeft = false;
			bool mouseClickRight = false;
		} uiSettings;

		FlingImgui(LogicalDevice* t_logicalDevice, Swapchain* t_swapChain);
		~FlingImgui();
		
		void Init(float t_width, float t_height);

		void Release();
		
		void InitResources(VkQueue t_copyQueue);
		
		void UpdateBuffers();
		
		void DrawFrame(VkCommandBuffer t_commandBuffer);

		template <auto Candidate, typename Type>
		void Display(Type& t_instance)
		{
			m_display.connect<Candidate>(t_instance);
		}

		void BuildCommandBuffers(bool t_displayOn);

		const VkCommandBuffer GetCommandBuffer(UINT32 t_index) const { return m_commandBuffers[t_index];  }
		const VkCommandPool GetCommandPool() const { return m_commandPool; }

	private:
		VkSampler m_sampler;
		INT32 m_vertexCount = 0;
		INT32 m_indexCount = 0;
		VkDeviceMemory m_fontMemory = VK_NULL_HANDLE;
		VkImage m_fontImage = VK_NULL_HANDLE;
		VkImageView m_fontImageView = VK_NULL_HANDLE;
		VkPipelineCache m_pipelineCache;
		VkPipelineLayout m_pipelineLayout;
		VkPipeline m_pipeLine;
		VkDescriptorPool m_descriptorPool;
		VkDescriptorSetLayout m_descriptorSetLayout;
		VkDescriptorSet m_descriptorSet;
		LogicalDevice* m_LogicalDevice;
		
		VkRenderPass m_renderPass;

		VkCommandPool m_commandPool;
		std::vector<VkCommandBuffer> m_commandBuffers;
		std::vector<VkFramebuffer> m_frameBuffers;

		std::unique_ptr<class Buffer> m_vertexBuffer;
		std::unique_ptr<class Buffer> m_indexBuffer;

		Swapchain* m_swapChain;

		entt::delegate<void()> m_display{};
	};
} //namespace fling