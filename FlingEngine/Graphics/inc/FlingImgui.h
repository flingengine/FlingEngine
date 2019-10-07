#pragma once 

#include <imgui.h>
#include <algorithm>

#include "Renderer.h"
#include "FlingVulkan.h"
#include "MappedBuffer.h"
#include "LogicalDevice.h"
#include "GraphicsHelpers.h"
#include "ResourceManager.h"
#include "File.h"

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

		FlingImgui(LogicalDevice* t_logicalDevice);
		~FlingImgui();
		
		void Init(float t_width, float t_height);
		
		void InitResources(VkRenderPass t_renderPass, VkQueue t_copyQueue);
		
		void UpdateBuffers();
		
		void DrawFrame(VkCommandBuffer t_commandBuffer);

		template <auto Candidate, typename Type>
		void NewFrame(Type& t_instance)
		{
			entt::delegate<void()> delegate{};
			delegate.connect<Candidate>(t_instance);
			delegate();
		}

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

		std::unique_ptr<class Buffer> m_vertexBuffer;
		std::unique_ptr<class Buffer> m_indexBuffer;
	};
} //namespace fling