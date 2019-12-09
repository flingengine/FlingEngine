#include "ImGuiSubpass.h"
#include "FrameBuffer.h"
#include "CommandBuffer.h"
#include "PhyscialDevice.h"
#include "LogicalDevice.h"
#include "GraphicsHelpers.h"
#include "Components/Transform.h"
#include "MeshRenderer.h"
#include "SwapChain.h"
#include "UniformBufferObject.h"
#include "FirstPersonCamera.h"
#include "FlingVulkan.h"
#include "BaseEditor.h"

#include <imgui.h>
#include <algorithm>

namespace Fling
{
	ImGuiSubpass::ImGuiSubpass(
		const LogicalDevice* t_Dev,
		const Swapchain* t_Swap,
		entt::registry& t_reg,
		FlingWindow* t_Window,
		VkRenderPass t_GlobalRenderPass,
		std::shared_ptr<Fling::BaseEditor> t_Editor,
		std::shared_ptr<Fling::Shader> t_Vert,
		std::shared_ptr<Fling::Shader> t_Frag)
		: Subpass(t_Dev, t_Swap, t_Vert, t_Frag)
		, m_Window(t_Window)
		, m_Editor(t_Editor)
		, m_GlobalRenderPass(t_GlobalRenderPass)
	{
		assert(m_Window);

		// Set the clear values for the G Buffer
		m_ClearValues.resize(2);
		m_ClearValues[0].color = { 0.0f, 0.0f, 0.0f, 0.2F };
		m_ClearValues[1].depthStencil = { 1.0f, ~0U };

		// Initialize ImGui
		if (!ImGui::GetCurrentContext())
		{
			ImGui::CreateContext();
		}

		PrepareResources();

		PrepImGuiStyleSettings();

		PrepareAttachments();
	}

	ImGuiSubpass::~ImGuiSubpass()
	{
		ImGui::DestroyContext();

		VkDevice logicalDevice = m_Device->GetVkDevice();

		vkDestroyImage(logicalDevice, m_fontImage, nullptr);
		vkDestroyImageView(logicalDevice, m_fontImageView, nullptr);
		vkFreeMemory(logicalDevice, m_fontMemory, nullptr);
		vkDestroySampler(logicalDevice, m_sampler, nullptr);
		vkDestroyPipelineCache(logicalDevice, m_pipelineCache, nullptr);
		vkDestroyPipeline(logicalDevice, m_pipeLine, nullptr);
		vkDestroyPipelineLayout(logicalDevice, m_pipelineLayout, nullptr);
		vkDestroyDescriptorPool(logicalDevice, m_descriptorPool, nullptr);
		vkDestroyDescriptorSetLayout(logicalDevice, m_descriptorSetLayout, nullptr);
	}

	void ImGuiSubpass::Draw(CommandBuffer& t_CmdBuf, VkFramebuffer t_PresentFrameBuf, UINT32 t_ActiveFrameInFlight, entt::registry& t_reg, float DeltaTime)
	{
		ImGui::NewFrame();

		if (m_Editor)
		{
			m_Editor->Draw(t_reg, DeltaTime);
		}

		ImGui::Render();

		UpdateUniforms();

		BuildCommandBuffer(t_CmdBuf.GetHandle());
	}

	void ImGuiSubpass::BuildCommandBuffer(VkCommandBuffer t_commandBuffer)
	{
		ImGuiIO& io = ImGui::GetIO();

		vkCmdBindDescriptorSets(t_commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1, &m_descriptorSet, 0, nullptr);
		vkCmdBindPipeline(t_commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeLine);

		//for minimizing screen 
		float displayWidth = ImGui::GetIO().DisplaySize.x ? ImGui::GetIO().DisplaySize.x : .0001f;
		float displayHeight = ImGui::GetIO().DisplaySize.y ? ImGui::GetIO().DisplaySize.y : .0001f;

		VkViewport viewport = Initializers::Viewport(
			displayWidth,
			displayHeight,
			0.0f,
			1.0f);
		vkCmdSetViewport(t_commandBuffer, 0, 1, &viewport);

		//UI scale and translate via push constants
		pushConstBlock.scale = glm::vec2(2.0f / io.DisplaySize.x, 2.0f / io.DisplaySize.y);
		pushConstBlock.translate = glm::vec2(-1.0f);
		vkCmdPushConstants(
			t_commandBuffer,
			m_pipelineLayout,
			VK_SHADER_STAGE_VERTEX_BIT,
			0,
			sizeof(PushConstBlock),
			&pushConstBlock);

		//Render commands 
		ImDrawData* imDrawData = ImGui::GetDrawData();
		UINT32 vertexOffset = 0;
		UINT32 indexOffset = 0;

		if (imDrawData->CmdListsCount > 0)
		{
			VkDeviceSize offsets[1] = { 0 };
			vkCmdBindVertexBuffers(
				t_commandBuffer,
				0,
				1,
				&m_vertexBuffer->GetVkBuffer(),
				offsets);

			vkCmdBindIndexBuffer(
				t_commandBuffer,
				m_indexBuffer->GetVkBuffer(),
				0,
				VK_INDEX_TYPE_UINT16);

			for (INT32 i = 0; i < imDrawData->CmdListsCount; ++i)
			{
				const ImDrawList* cmd_list = imDrawData->CmdLists[i];
				for (INT32 j = 0; j < cmd_list->CmdBuffer.Size; ++j)
				{
					const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[j];
					VkRect2D scissorRect;
					scissorRect.offset.x = std::max((INT32)(pcmd->ClipRect.x), 0);
					scissorRect.offset.y = std::max((INT32)(pcmd->ClipRect.y), 0);
					scissorRect.extent.width = (INT32)(pcmd->ClipRect.z - pcmd->ClipRect.x);
					scissorRect.extent.height = (INT32)(pcmd->ClipRect.w - pcmd->ClipRect.y);
					vkCmdSetScissor(t_commandBuffer, 0, 1, &scissorRect);
					vkCmdDrawIndexed(t_commandBuffer, pcmd->ElemCount, 1, indexOffset, vertexOffset, 0);
					indexOffset += pcmd->ElemCount;
				}

				vertexOffset += cmd_list->VtxBuffer.Size;
			}
		}
	}

	void ImGuiSubpass::CreateDescriptorSets(VkDescriptorPool t_Pool, entt::registry& t_reg)
	{
		
	}

	void ImGuiSubpass::PrepareAttachments()
	{

	}

	void ImGuiSubpass::CreateGraphicsPipeline()
	{
		ImGuiIO& io = ImGui::GetIO();
		unsigned char* fontData;
		int texWidth;
		int texHeight;
		VkDevice logicalDevice = m_Device->GetVkDevice();
		io.Fonts->GetTexDataAsRGBA32(&fontData, &texWidth, &texHeight);
		VkDeviceSize uploadSize = texWidth * texHeight * 4 * sizeof(char);

		Fling::GraphicsHelpers::CreateVkImage(
			logicalDevice,
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

		Buffer stagingBuffer = Buffer(
			uploadSize,
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			fontData);

		//Copy buffer data to font image
		VkCommandBuffer copycmd = GraphicsHelpers::BeginSingleTimeCommands();

		GraphicsHelpers::SetImageLayout(
			copycmd,
			m_fontImage,
			VK_IMAGE_ASPECT_COLOR_BIT,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			VK_PIPELINE_STAGE_HOST_BIT,
			VK_PIPELINE_STAGE_TRANSFER_BIT);

		//Copy
		VkBufferImageCopy bufferCopyRegion = {};
		bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		bufferCopyRegion.imageSubresource.layerCount = 1;
		bufferCopyRegion.imageExtent.width = texWidth;
		bufferCopyRegion.imageExtent.height = texHeight;
		bufferCopyRegion.imageExtent.depth = 1;

		vkCmdCopyBufferToImage(
			copycmd,
			stagingBuffer.GetVkBuffer(),
			m_fontImage,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			1,
			&bufferCopyRegion
		);

		GraphicsHelpers::SetImageLayout(
			copycmd,
			m_fontImage,
			VK_IMAGE_ASPECT_COLOR_BIT,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			VK_PIPELINE_STAGE_TRANSFER_BIT,
			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

		GraphicsHelpers::EndSingleTimeCommands(copycmd);

		Fling::GraphicsHelpers::CreateVkSampler(
			VK_FILTER_LINEAR,
			VK_FILTER_LINEAR,
			VK_SAMPLER_MIPMAP_MODE_LINEAR,
			VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
			VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
			VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
			VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE,
			m_sampler);

		//Descriptor pool --------------------------------
		std::vector<VkDescriptorPoolSize> poolSizes =
		{
			Initializers::DescriptorPoolSize(VK_DESCRIPTOR_TYPE_SAMPLER, 1000),
			Initializers::DescriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000),
			Initializers::DescriptorPoolSize(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000),
			Initializers::DescriptorPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000),
			Initializers::DescriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000),
			Initializers::DescriptorPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000),
			Initializers::DescriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000),
			Initializers::DescriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000),
			Initializers::DescriptorPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000),
			Initializers::DescriptorPoolSize(VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000),
		};

		VkDescriptorPoolCreateInfo descriptorPoolInfo = Initializers::DescriptorPoolCreateInfo(poolSizes, 10);
		VK_CHECK_RESULT(vkCreateDescriptorPool(logicalDevice, &descriptorPoolInfo, nullptr, &m_descriptorPool));

		//Descriptor set layout ---------------------------------
		std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings =
		{
			Initializers::DescriptorSetLayoutBindings(
				VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				VK_SHADER_STAGE_FRAGMENT_BIT, 0),
		};

		VkDescriptorSetLayoutCreateInfo descriptorLayout = Initializers::DescriptorSetLayoutCreateInfo(setLayoutBindings);
		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(logicalDevice, &descriptorLayout, nullptr, &m_descriptorSetLayout));

		//Descriptor set  --------------------------------
		VkDescriptorSetAllocateInfo allocInfo = Initializers::DescriptorSetAllocateInfo(m_descriptorPool, &m_descriptorSetLayout, 1);
		if (vkAllocateDescriptorSets(logicalDevice, &allocInfo, &m_descriptorSet) != VK_SUCCESS)
		{
			F_LOG_ERROR("Could not allocate descriptor sets for imgui");
		}

		VkDescriptorImageInfo fontDescriptor = Initializers::DescriptorImageInfo(
			m_sampler,
			m_fontImageView,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
		);

		std::vector<VkWriteDescriptorSet> writeDescriptorSet = {
			Initializers::WriteDescriptorSet(m_descriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0, &fontDescriptor),
		};

		vkUpdateDescriptorSets(logicalDevice, static_cast<UINT32>(writeDescriptorSet.size()), writeDescriptorSet.data(), 0, nullptr);

		//Pipeline cache --------------------------------------------------------
		VkPipelineCacheCreateInfo pipelineCacheCreateInfo = {};
		pipelineCacheCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
		if (vkCreatePipelineCache(logicalDevice, &pipelineCacheCreateInfo, nullptr, &m_pipelineCache) != VK_SUCCESS)
		{
			F_LOG_ERROR("Could not create pipeline cache for imgui");
		}

		//Pipeline layout
		//Push constants for UI rendering 
		VkPushConstantRange pushConstantRange = Initializers::PushConstantRange(VK_SHADER_STAGE_VERTEX_BIT, sizeof(PushConstBlock), 0);
		VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = Initializers::PiplineLayoutCreateInfo(&m_descriptorSetLayout, 1);
		pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
		pipelineLayoutCreateInfo.pPushConstantRanges = &pushConstantRange;

		if (vkCreatePipelineLayout(logicalDevice, &pipelineLayoutCreateInfo, nullptr, &m_pipelineLayout) != VK_SUCCESS)
		{
			F_LOG_ERROR("Could not create pipline layout for imgui");
		}

		//Setup graphics pipeline for UI rendering 
		VkPipelineInputAssemblyStateCreateInfo inputAssemblyState =
			Initializers::PipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, false);

		VkPipelineRasterizationStateCreateInfo rasterizationState =
			Initializers::PipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE);

		//Enable Blending 
		VkPipelineColorBlendAttachmentState blendAttachmentState = {};
		blendAttachmentState.blendEnable = VK_TRUE;
		blendAttachmentState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		blendAttachmentState.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
		blendAttachmentState.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		blendAttachmentState.colorBlendOp = VK_BLEND_OP_ADD;
		blendAttachmentState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		blendAttachmentState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
		blendAttachmentState.alphaBlendOp = VK_BLEND_OP_ADD;

		VkPipelineColorBlendStateCreateInfo colorBlendState =
			Initializers::PipelineColorBlendStateCreateInfo(1, &blendAttachmentState);

		VkPipelineDepthStencilStateCreateInfo depthStencilState =
			Initializers::DepthStencilState(VK_FALSE, VK_FALSE, VK_COMPARE_OP_LESS_OR_EQUAL);

		VkPipelineViewportStateCreateInfo viewportState =
			Initializers::PipelineViewportStateCreateInfo(1, 1, 0);

		VkPipelineMultisampleStateCreateInfo multisampleState =
			Initializers::PipelineMultiSampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT);

		std::vector<VkDynamicState> dynamicStateEnables = {
			VK_DYNAMIC_STATE_VIEWPORT,
			VK_DYNAMIC_STATE_SCISSOR
		};

		VkPipelineDynamicStateCreateInfo dynamicState =
			Initializers::PipelineDynamicStateCreateInfo(dynamicStateEnables);

		std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages{};

		VkPipelineShaderStageCreateInfo vertShaderStageInfo = {};
		vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
		vertShaderStageInfo.module = m_VertexShader->GetShaderModule();
		vertShaderStageInfo.pName = "main";

		VkPipelineShaderStageCreateInfo fragShaderStageInfo = {};
		fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		fragShaderStageInfo.module = m_FragShader->GetShaderModule();
		fragShaderStageInfo.pName = "main";

		shaderStages[0] = vertShaderStageInfo;
		shaderStages[1] = fragShaderStageInfo;

		VkGraphicsPipelineCreateInfo pipelineCreateInfo =
			Initializers::PipelineCreateInfo(m_pipelineLayout, m_GlobalRenderPass);

		pipelineCreateInfo.pInputAssemblyState = &inputAssemblyState;
		pipelineCreateInfo.pRasterizationState = &rasterizationState;
		pipelineCreateInfo.pColorBlendState = &colorBlendState;
		pipelineCreateInfo.pMultisampleState = &multisampleState;
		pipelineCreateInfo.pViewportState = &viewportState;
		pipelineCreateInfo.pDepthStencilState = &depthStencilState;
		pipelineCreateInfo.pDynamicState = &dynamicState;
		pipelineCreateInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
		pipelineCreateInfo.pStages = shaderStages.data();

		// Vertex bindings an attributes based on imgui vertex definitions
		std::vector<VkVertexInputBindingDescription> vertexInputBindings =
		{
			Initializers::VertexInputBindingDescription(0, sizeof(ImDrawVert), VK_VERTEX_INPUT_RATE_VERTEX),
		};

		std::vector<VkVertexInputAttributeDescription> vertexInputAttributes =
		{
			Initializers::VertexInputAttributeDescription(0, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(ImDrawVert, pos)),    // Location 0: Position
			Initializers::VertexInputAttributeDescription(0, 1, VK_FORMAT_R32G32_SFLOAT, offsetof(ImDrawVert, uv)),    // Location 1: UV
			Initializers::VertexInputAttributeDescription(0, 2, VK_FORMAT_R8G8B8A8_UNORM, offsetof(ImDrawVert, col)),    // Location 0: Color
		};

		VkPipelineVertexInputStateCreateInfo vertexInputState = Initializers::PiplineVertexInptStateCreateInfo();
		vertexInputState.vertexBindingDescriptionCount = static_cast<uint32_t>(vertexInputBindings.size());
		vertexInputState.pVertexBindingDescriptions = vertexInputBindings.data();
		vertexInputState.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertexInputAttributes.size());
		vertexInputState.pVertexAttributeDescriptions = vertexInputAttributes.data();

		pipelineCreateInfo.pVertexInputState = &vertexInputState;

		if (vkCreateGraphicsPipelines(logicalDevice, m_pipelineCache, 1, &pipelineCreateInfo, nullptr, &m_pipeLine) != VK_SUCCESS)
		{
			F_LOG_ERROR("Could not create graphics pipeline for imgui");
		}
	}

	void ImGuiSubpass::CleanUp(entt::registry& t_reg)
	{
		// Do we need this? 
	}

	void ImGuiSubpass::PrepImGuiStyleSettings()
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
		io.DisplaySize = ImVec2(m_Window->GetWidth(), m_Window->GetHeight());
		io.DisplayFramebufferScale = ImVec2(1.0f, 1.0f);
	}

	void ImGuiSubpass::PrepareResources()
	{
		// Create vert and index buffers for use with imgui geometry
		m_indexBuffer = std::make_unique<Buffer>();
		m_vertexBuffer = std::make_unique<Buffer>();
	}
	
	void ImGuiSubpass::UpdateUniforms()
	{
		ImDrawData* imDrawData = ImGui::GetDrawData();

		VkDeviceSize vertexBufferSize = imDrawData->TotalVtxCount * sizeof(ImDrawVert);
		VkDeviceSize indexBufferSize = imDrawData->TotalIdxCount * sizeof(ImDrawIdx);

		if ((vertexBufferSize == 0) || (indexBufferSize == 0)) 
		{
			return;
		}

		if ((m_vertexBuffer->GetVkBuffer() == VK_NULL_HANDLE) ||
			(m_vertexCount != imDrawData->TotalVtxCount))
		{
			m_vertexBuffer->UnmapMemory();
			m_vertexBuffer->Release();

			m_vertexBuffer->CreateBuffer(vertexBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, true);
			m_vertexCount = imDrawData->TotalVtxCount;
			m_vertexBuffer->MapMemory();
		}

		if ((m_indexBuffer->GetVkBuffer() == VK_NULL_HANDLE) ||
			(m_indexCount < imDrawData->TotalIdxCount))
		{
			m_indexBuffer->UnmapMemory();
			m_indexBuffer->Release();

			m_indexBuffer->CreateBuffer(indexBufferSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, true);
			m_indexCount = imDrawData->TotalIdxCount;
			m_indexBuffer->MapMemory();
		}

		ImDrawVert* vtxDst = (ImDrawVert*)m_vertexBuffer->m_MappedMem;
		ImDrawIdx* idxDst = (ImDrawIdx*)m_indexBuffer->m_MappedMem;

		for (int n = 0; n < imDrawData->CmdListsCount; ++n) {
			const ImDrawList* cmd_list = imDrawData->CmdLists[n];
			memcpy(vtxDst, cmd_list->VtxBuffer.Data, cmd_list->VtxBuffer.Size * sizeof(ImDrawVert));
			memcpy(idxDst, cmd_list->IdxBuffer.Data, cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx));
			vtxDst += cmd_list->VtxBuffer.Size;
			idxDst += cmd_list->IdxBuffer.Size;
		}

		m_vertexBuffer->Flush(VK_WHOLE_SIZE, 0);
		m_indexBuffer->Flush(VK_WHOLE_SIZE, 0);
	}
}   // namespace Fling