#include "GeometrySubpass.h"
#include "FrameBuffer.h"
#include "CommandBuffer.h"
#include "PhyscialDevice.h"
#include "LogicalDevice.h"
#include "GraphicsHelpers.h"
#include "MeshRenderer.h"
#include "SwapChain.h"
#include "UniformBufferObject.h"
#include "Model.h"
#include "Buffer.h"
#include "OffscreenSubpass.h"

#include "VulkanApp.h"		// #TODO: Lets get rid of this

namespace Fling
{
	GeometrySubpass::GeometrySubpass(
		const LogicalDevice* t_Dev,
		const Swapchain* t_Swap,
		entt::registry& t_reg,
		FrameBuffer* t_OffscreenDep,
		std::shared_ptr<Fling::Shader> t_Vert,
		std::shared_ptr<Fling::Shader> t_Frag)
		: Subpass(t_Dev, t_Swap, t_Vert, t_Frag)
		, m_OffscreenFrameBuf(t_OffscreenDep)
	{
		// Set clear values
		m_ClearValues.resize(2);
		m_ClearValues[0].color = { { 0.0f, 0.0f, 0.2f, 0.0f } };
		m_ClearValues[1].depthStencil = { 1.0f, 0 };

		m_QuadModel = Model::Quad();

		// Initializes the lighting UBO's  --------
		VkDeviceSize bufferSize = sizeof(uboFragmentLights);

		m_LightingUBOs.resize(m_SwapChain->GetImageCount());
		for (size_t i = 0; i < m_LightingUBOs.size(); i++)
		{
			m_LightingUBOs[i] = new Buffer(
				bufferSize,
				VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

			m_LightingUBOs[i]->MapMemory(bufferSize);
		}
	}

	GeometrySubpass::~GeometrySubpass()
	{
		// Clean up any allocated UBO's 
		for (Buffer* buf : m_LightingUBOs)
		{
			if (buf)
			{
				delete buf;
				buf = nullptr;
			}
		}
		m_LightingUBOs.clear();
		// Clean up any allocated descriptor sets
	}

	void GeometrySubpass::Draw(CommandBuffer& t_CmdBuf, UINT32 t_ActiveFrameInFlight, entt::registry& t_reg)
	{
		VkViewport viewport{};
		viewport.width = static_cast<float>(m_SwapChain->GetExtents().width);
		viewport.height = static_cast<float>(m_SwapChain->GetExtents().height);
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;

		VkRect2D scissor{};
		scissor.extent = m_SwapChain->GetExtents();
		scissor.offset.x = 0;
		scissor.offset.y = 0;

		UpdateLightingUBO(t_ActiveFrameInFlight);

		// Build the command buffer where t_CmdBuf is the drawing command buffer for the swap chain
		t_CmdBuf.Begin();

		// Start a render pass using the global render pass settings
		VkRenderPass GlobalPass = VulkanApp::Get().GetGlobalRenderPass();
		VkFramebuffer SwapFrameBuf = VulkanApp::Get().GetFrameBufferAtIndex(t_ActiveFrameInFlight);

		VkRenderPassBeginInfo renderPassBeginInfo = Initializers::RenderPassBeginInfo();
		renderPassBeginInfo.renderPass = GlobalPass;
		renderPassBeginInfo.framebuffer = SwapFrameBuf;
		
		renderPassBeginInfo.renderArea.offset.x = 0;
		renderPassBeginInfo.renderArea.offset.y = 0;
		renderPassBeginInfo.renderArea.extent.width = m_SwapChain->GetExtents().width;
		renderPassBeginInfo.renderArea.extent.height = m_SwapChain->GetExtents().height;
		renderPassBeginInfo.clearValueCount = m_ClearValues.size();
		renderPassBeginInfo.pClearValues = m_ClearValues.data();

		vkCmdBeginRenderPass(t_CmdBuf.GetHandle(), &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

		t_CmdBuf.SetViewport(0, { viewport });
		t_CmdBuf.SetScissor(0, { scissor });

		VkDeviceSize offsets[1] = { 0 };
		vkCmdBindDescriptorSets(
			t_CmdBuf.GetHandle(), 
			VK_PIPELINE_BIND_POINT_GRAPHICS, 
			m_GraphicsPipeline->GetPipelineLayout(), 
			0, 
			1, 
			&m_DescriptorSets[t_ActiveFrameInFlight],
			0, 
			nullptr
		);

		// Final composition as full screen quad
		VkBuffer vertexBuffers[1] = { m_QuadModel->GetVertexBuffer()->GetVkBuffer() };

		vkCmdBindPipeline(t_CmdBuf.GetHandle(), VK_PIPELINE_BIND_POINT_GRAPHICS, m_GraphicsPipeline->GetPipeline());
		vkCmdBindVertexBuffers(t_CmdBuf.GetHandle(), 0, 1, vertexBuffers, offsets);
		vkCmdBindIndexBuffer(t_CmdBuf.GetHandle(), m_QuadModel->GetIndexBuffer()->GetVkBuffer(), 0, m_QuadModel->GetIndexType());
		vkCmdDrawIndexed(t_CmdBuf.GetHandle(), 6, 1, 0, 0, 1);

		t_CmdBuf.EndRenderPass();

		// End command buffer recording
		t_CmdBuf.End();
	}

	void GeometrySubpass::CreateDescriptorSets(VkDescriptorPool t_Pool, entt::registry& t_reg)
	{
		assert(m_OffscreenFrameBuf);

		size_t ImageCount = m_SwapChain->GetImageCount();
		m_DescriptorSets.resize(ImageCount);

		std::vector<VkDescriptorSetLayout> layouts(ImageCount, m_GraphicsPipeline->GetDescriptorSetLayout());
		VkDescriptorSetAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		// If we have specified a specific pool then use that, otherwise use the one on the mesh
		allocInfo.descriptorPool = t_Pool;
		allocInfo.descriptorSetCount = static_cast<UINT32>(ImageCount);
		allocInfo.pSetLayouts = layouts.data();

		VK_CHECK_RESULT(vkAllocateDescriptorSets(m_Device->GetVkDevice(), &allocInfo, m_DescriptorSets.data()));

		for (size_t i = 0; i < m_DescriptorSets.size(); ++i)
		{
			// Create the image info's for the write sets to reference
			// that will give us access to the G-Buffer in the shaders
			VkDescriptorImageInfo texDescriptorPosition =
				Initializers::DescriptorImageInfo(
					m_OffscreenFrameBuf->GetSamplerHandle(),
					m_OffscreenFrameBuf->GetAttachmentAtIndex(0)->GetViewHandle(),
					VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

			VkDescriptorImageInfo texDescriptorNormal =
				Initializers::DescriptorImageInfo(
					m_OffscreenFrameBuf->GetSamplerHandle(),
					m_OffscreenFrameBuf->GetAttachmentAtIndex(1)->GetViewHandle(),
					VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

			VkDescriptorImageInfo texDescriptorAlbedo =
				Initializers::DescriptorImageInfo(
					m_OffscreenFrameBuf->GetSamplerHandle(),
					m_OffscreenFrameBuf->GetAttachmentAtIndex(2)->GetViewHandle(),
					VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

			std::vector<VkWriteDescriptorSet> writeDescriptorSets =
			{
				// 1 : Position sampler
				Initializers::WriteDescriptorSet(
					m_DescriptorSets[i],
					VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
					1,
					&texDescriptorPosition),
				// 2 : Normal sampler
				Initializers::WriteDescriptorSet(
					m_DescriptorSets[i],
					VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
					2,
					&texDescriptorNormal),
				// 3 : Albedo sampler
				Initializers::WriteDescriptorSet(
					m_DescriptorSets[i],
					VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
					3,
					&texDescriptorAlbedo),
				// 4 : Lighting UBO to the fragment shader
				Initializers::WriteDescriptorSetUniform(
					m_LightingUBOs[i],
					m_DescriptorSets[i],
					4
				),
			};

			vkUpdateDescriptorSets(m_Device->GetVkDevice(), static_cast<UINT32>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr);
		}
	}

	void GeometrySubpass::CreateGraphicsPipeline()
	{
		// Use empty vertex descriptions here
		VkPipelineVertexInputStateCreateInfo emptyInputState = {};
		emptyInputState.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		m_GraphicsPipeline->m_PipelineCreateInfo.pVertexInputState = {};
		m_GraphicsPipeline->m_PipelineCreateInfo.pVertexInputState = &emptyInputState;
		m_GraphicsPipeline->m_VertexInputStateCreateInfo = { VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
		m_GraphicsPipeline->m_VertexInputStateCreateInfo.vertexBindingDescriptionCount = 0;

		// Create it otherwise with defaults
		VkRenderPass RenderPass = VulkanApp::Get().GetGlobalRenderPass();
		m_GraphicsPipeline->CreateGraphicsPipeline(RenderPass, nullptr);
	}

	void GeometrySubpass::UpdateLightingUBO(UINT32 t_ActiveFrame)
	{
		// White
		uboFragmentLights.lights[0].position = glm::vec4(0.0f, 0.0f, 1.0f, 0.0f);
		uboFragmentLights.lights[0].color = glm::vec3(1.5f);
		uboFragmentLights.lights[0].radius = 15.0f * 0.25f;
		// Red
		uboFragmentLights.lights[1].position = glm::vec4(-2.0f, 0.0f, 0.0f, 0.0f);
		uboFragmentLights.lights[1].color = glm::vec3(1.0f, 0.0f, 0.0f);
		uboFragmentLights.lights[1].radius = 15.0f;
		// Blue
		uboFragmentLights.lights[2].position = glm::vec4(2.0f, 1.0f, 0.0f, 0.0f);
		uboFragmentLights.lights[2].color = glm::vec3(0.0f, 0.0f, 2.5f);
		uboFragmentLights.lights[2].radius = 5.0f;
		// Yellow
		uboFragmentLights.lights[3].position = glm::vec4(0.0f, 0.9f, 0.5f, 0.0f);
		uboFragmentLights.lights[3].color = glm::vec3(1.0f, 1.0f, 0.0f);
		uboFragmentLights.lights[3].radius = 2.0f;
		// Green
		uboFragmentLights.lights[4].position = glm::vec4(0.0f, 0.5f, 0.0f, 0.0f);
		uboFragmentLights.lights[4].color = glm::vec3(0.0f, 1.0f, 0.2f);
		uboFragmentLights.lights[4].radius = 5.0f;
		// Yellow
		uboFragmentLights.lights[5].position = glm::vec4(0.0f, 1.0f, 0.0f, 0.0f);
		uboFragmentLights.lights[5].color = glm::vec3(1.0f, 0.7f, 0.3f);
		uboFragmentLights.lights[5].radius = 25.0f;

		// Memcpy to the buffer
		Buffer* buf = m_LightingUBOs[t_ActiveFrame];
		memcpy(buf->m_MappedMem, &uboFragmentLights, buf->GetSize());
	}
}   // namespace Fling