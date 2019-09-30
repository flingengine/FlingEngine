#pragma once 

#include "FlingVulkan.h"
#include "Renderer.h"
#include "Buffer.h"
#include "LogicalDevice.h"
#include "PhyscialDevice.h"
#include "imgui.h"
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

		FlingImgui(const LogicalDevice& t_logicalDevice);
		~FlingImgui();
		
		void Init(float width, float height);
		void InitResources(VkRenderPass renderPass, VkQueue copyQueue);
		//To do : look into delegates 
		void NewFrame();
		void UpdateBuffers();
		void DrawFrame();

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

		const LogicalDevice& m_LogicalDevice;
	};
} //namespace fling