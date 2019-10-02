#pragma once 

#include <imgui.h>

#include "Renderer.h"
#include "FlingVulkan.h"
#include "Buffer.h"
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

		FlingImgui(LogicalDevice* t_logicalDevice);
		~FlingImgui();
		
		void Init(float t_width, float t_height);
		void InitResources(VkRenderPass t_renderPass, VkQueue t_copyQueue);
		//To do : look into delegates 
		void NewFrame();
		void UpdateBuffers();
		void DrawFrame(VkCommandBuffer t_commandBuffer);

	private:
		VkSampler m_sampler;
		Buffer m_vertexBuffer;
		Buffer m_indexBuffer;
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

		void* m_vertexMappedMemory = nullptr;
		void* m_indexMappedMemory = nullptr;

		LogicalDevice* m_LogicalDevice;
	};
} //namespace fling