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

        Buffer stagingBuffer(
            uploadSize, 
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            fontData);

		Fling::GraphicsHelpers::TransitionImageLayout(
			m_fontImage,
			VK_FORMAT_R8G8B8A8_UNORM,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);



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