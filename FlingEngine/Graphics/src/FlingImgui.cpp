#include "FlingImgui.h"

namespace Fling
{
    FlingImgui::FlingImgui()
	{
        ImGui::CreateContext();
	}

	FlingImgui::~FlingImgui()
	{
        ImGui::DestroyContext();
        m_vertexBuffer.Release();
        m_indexBuffer.Release();
		vkDestroyImage(m_LogicalDevice->GetVkDevice(), m_fontImage, nullptr);
        vkDestroyImageView(m_LogicalDevice->GetVkDevice(), m_fontImageView, nullptr);
        vkFreeMemory(m_LogicalDevice->GetVkDevice(), m_fontMemory, nullptr);
        vkDestroySampler(m_LogicalDevice->GetVkDevice(), m_sampler, nullptr);
        vkDestroyPipelineCache(m_LogicalDevice->GetVkDevice(), m_pipelineCache, nullptr);
        vkDestroyPipelineLayout(m_LogicalDevice->GetVkDevice(), m_pipelineLayout, nullptr);
        vkDestroyDescriptorPool(m_LogicalDevice->GetVkDevice(), m_descriptorPool, nullptr);
        vkDestroyDescriptorSetLayout(m_LogicalDevice->GetVkDevice(), m_descriptorSetLayout, nullptr);
	}

	void FlingImgui::Init(float width, float height)
	{
        // Color scheme
		ImGuiStyle& style = ImGui::GetStyle();
		style.Colors[ImGuiCol_TitleBg] = ImVec4(1.0f, 0.0f, 0.0f, 0.6f);
		style.Colors[ImGuiCol_TitleBgActive] = ImVec4(1.0f, 0.0f, 0.0f, 0.8f);
		style.Colors[ImGuiCol_MenuBarBg] = ImVec4(1.0f, 0.0f, 0.0f, 0.4f);
		style.Colors[ImGuiCol_Header] = ImVec4(1.0f, 0.0f, 0.0f, 0.4f);
		style.Colors[ImGuiCol_CheckMark] = ImVec4(0.0f, 1.0f, 0.0f, 1.0f);
        //Dimensions
        ImGuiIO& io = ImGui::GetIO();
        io.DisplaySize = ImVec2(width, height);
        io.DisplayFramebufferScale = ImVec2(1.0f, 1.0f);
	}

	void FlingImgui::InitResources(VkRenderPass renderPass, VkQueue copyQueue)
	{
        ImGuiIO& io = ImGui::GetIO();
        unsigned char* fontData;
        int texWidth;
        int texHeight;
        VkDevice logicalDevice = m_LogicalDevice->GetVkDevice();
        io.Fonts->GetTexDataAsRGBA32(&fontData, &texWidth, &texHeight);
        VkDeviceSize uploadSize = texWidth * texHeight * 4 * sizeof(char);

        Fling::GraphicsHelpers::CreateVkImage(
            texWidth, 
            texHeight, 
            VK_FORMAT_R8G8B8A8_UNORM,
            VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            m_fontImage,
            m_fontMemory);

        m_fontImageView = Fling::GraphicsHelpers::CreateVkImageView(
            m_fontImage,
            VK_FORMAT_R8G8B8A8_UNORM,
            VK_IMAGE_ASPECT_COLOR_BIT
        );

		Fling::GraphicsHelpers::TransitionImageLayout(
			m_fontImage,
			VK_FORMAT_R8G8B8A8_UNORM,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

		Fling::GraphicsHelpers::CreateVkSampler(
			VK_FILTER_LINEAR,
			VK_FILTER_LINEAR,
			VK_SAMPLER_MIPMAP_MODE_LINEAR,
			VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
			VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
			VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
			VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE,
			m_sampler);

		//Descriptor pool
		std::vector<VkDescriptorPoolSize> poolSizes = 
		{
			Fling::GraphicsHelpers::DescriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1),
		};

		VkDescriptorPoolCreateInfo descriptorPoolInfo = Fling::GraphicsHelpers::DescriptorPoolCreateInfo(poolSizes, 2);

		if (vkCreateDescriptorPool(logicalDevice, &descriptorPoolInfo, nullptr, &m_descriptorPool) != VK_SUCCESS)
		{
			F_LOG_ERROR("Could not create descriptor pool for imgui");
		}

		//Descriptor set layout
		std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings =
		{
			Fling::GraphicsHelpers::DescriptorSetLayoutBindings(
				VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 
				VK_SHADER_STAGE_FRAGMENT_BIT, 0),
		};

		VkDescriptorSetLayoutCreateInfo descriptorLayout = Fling::GraphicsHelpers::DescriptorSetLayoutCreateInfo(setLayoutBindings);
		if (vkCreateDescriptorSetLayout(logicalDevice, &descriptorLayout, nullptr, &m_descriptorSetLayout) != VK_SUCCESS)
		{
			F_LOG_ERROR("Could not create descriptor set layout for imgui");
		}

		//Descriptor set 
		VkDescriptorSetAllocateInfo allocInfo = Fling::GraphicsHelpers::DescriptorSetAllocateInfo(m_descriptorPool, &m_descriptorSetLayout, 1);
		if (vkAllocateDescriptorSets(logicalDevice, &allocInfo, &m_descriptorSet) != VK_SUCCESS)
		{
			F_LOG_ERROR("Could not allocate descriptor sets for imgui");
		}

		VkDescriptorImageInfo fontDescriptor = Fling::GraphicsHelpers::DescriptorImageInfo(
			m_sampler,
			m_fontImageView,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
		);

		std::vector<VkWriteDescriptorSet> writeDescriptorSet = {
			Fling::GraphicsHelpers::WriteDescriptorSet(m_descriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0, &fontDescriptor),
		};
		
		vkUpdateDescriptorSets(logicalDevice, static_cast<UINT32>(writeDescriptorSet.size()), writeDescriptorSet.data(), 0, nullptr);
        
		//Pipeline cache 
		VkPipelineCacheCreateInfo pipelineCacheCreateInfo = {};
		pipelineCacheCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
		if (vkCreatePipelineCache(logicalDevice, &pipelineCacheCreateInfo, nullptr, &m_pipelineCache) != VK_SUCCESS)
		{
			F_LOG_ERROR("Could not create pipeline cache for imgui");
		}


		//Pipeline layout
		//Push constants for UI rendering 
		VkPushConstantRange pushConstantRange = Fling::GraphicsHelpers::PushConstantRange(VK_SHADER_STAGE_VERTEX_BIT, sizeof(PushConstBlock), 0);
		VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = Fling::GraphicsHelpers::PiplineLayoutCreateInfo(&m_descriptorSetLayout, 1);
		pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
		pipelineLayoutCreateInfo.pPushConstantRanges = &pushConstantRange;

		if (vkCreatePipelineLayout(logicalDevice, &pipelineLayoutCreateInfo, nullptr, &m_pipelineLayout) != VK_SUCCESS)
		{
			F_LOG_ERROR("Could not create pipline layout for imgui");
		}

		//Setup graphics pipeline for UI rendering 
		VkPipelineInputAssemblyStateCreateFlags inputAssemblyState = 


	}

	void FlingImgui::NewFrame()
	{
	}

	void FlingImgui::UpdateBuffers()
	{
	}

	void FlingImgui::DrawFrame()
	{
	}
}