#include "OffscreenSubpass.h"
#include "FrameBuffer.h"
#include "CommandBuffer.h"
#include "PhyscialDevice.h"
#include "LogicalDevice.h"
#include "GraphicsHelpers.h"
#include "MeshRenderer.h"
#include "SwapChain.h"
#include "UniformBufferObject.h"

#include "VulkanApp.h"	// #TODO See if we can get rid of this with VMA or something

#define FRAME_BUF_DIM 2048

namespace Fling
{
	OffscreenSubpass::OffscreenSubpass(
		const LogicalDevice* t_Dev,
		const Swapchain* t_Swap,
		entt::registry& t_reg,
		std::shared_ptr<Fling::Shader> t_Vert,
		std::shared_ptr<Fling::Shader> t_Frag)
		: Subpass(t_Dev, t_Swap, t_Vert, t_Frag)
	{
		t_reg.on_construct<MeshRenderer>().connect<&OffscreenSubpass::OnMeshRendererAdded>(*this);

		// Set the clear values for the G Buffer
		m_ClearValues.resize(4);
		m_ClearValues[0].color = m_ClearValues[1].color = { { 0.0f, 0.0f, 0.0f, 0.0f } };
		m_ClearValues[2].color = { { 0.0f, 0.0f, 0.0f, 0.0f } };
		m_ClearValues[3].depthStencil = { 1.0f, 0 };

		// Build offscreen semaphores -------
		m_OffscreenSemaphores.resize(VulkanApp::MAX_FRAMES_IN_FLIGHT);
		for (INT32 i = 0; i < VulkanApp::MAX_FRAMES_IN_FLIGHT; i++)
		{
			m_OffscreenSemaphores[i] = GraphicsHelpers::CreateSemaphore(m_Device->GetVkDevice());
		}

		// Build offscreen command buffers
		m_OffscreenCmdBufs.resize(m_SwapChain->GetImageCount());
		for (size_t i = 0; i < m_OffscreenCmdBufs.size(); ++i)
		{
			m_OffscreenCmdBufs[i] = VulkanApp::Get().RequestCommandBuffer();
			assert(m_OffscreenCmdBufs[i] != nullptr);
		}

		// Tell the Vulkan app that the draw command buffers need to WAIT on this offscreen semaphore
		PrepareAttachments();
	}

	OffscreenSubpass::~OffscreenSubpass()
	{
		// Destroy any allocated semaphores
		for (size_t i = 0; i < m_OffscreenSemaphores.size(); ++i)
		{
			vkDestroySemaphore(m_Device->GetVkDevice(), m_OffscreenSemaphores[i], nullptr);
		}

		// Clean up command buffers we used
		for (CommandBuffer* CmdBuf : m_OffscreenCmdBufs)
		{
			if (CmdBuf)
			{
				delete CmdBuf;
				CmdBuf = nullptr;
			}
		}
		m_OffscreenCmdBufs.clear();

		if (m_OffscreenFrameBuf)
		{
			delete m_OffscreenFrameBuf;
			m_OffscreenFrameBuf = nullptr;
		}

		// Clean up any allocated UBO's 
		// Clean up any allocated descriptor sets
	}

	void OffscreenSubpass::Draw(CommandBuffer& t_CmdBuf, UINT32 t_ActiveFrameInFlight, entt::registry& t_reg)
	{
		assert(m_GraphicsPipeline);
		// Don't use the given command buffer, instead build the OFFSCREEN command buffer
		CommandBuffer* OffscreenCmdBuf = m_OffscreenCmdBufs[t_ActiveFrameInFlight];
		assert(OffscreenCmdBuf);
		vkResetCommandBuffer(OffscreenCmdBuf->GetHandle(), VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT );

		// Set viewport and scissors to the offscreen frame buffer
		VkViewport viewport{};
		viewport.width = static_cast<float>(m_OffscreenFrameBuf->GetWidth());
		viewport.height = static_cast<float>(m_OffscreenFrameBuf->GetHeight());
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;

		VkRect2D scissor{};
		scissor.extent.width = static_cast<UINT32>(m_OffscreenFrameBuf->GetWidth());
		scissor.extent.height = static_cast<UINT32>(m_OffscreenFrameBuf->GetHeight());
		scissor.offset.x = 0;
		scissor.offset.y = 0;

		OffscreenCmdBuf->Begin();
		OffscreenCmdBuf->BeginRenderPass(*m_OffscreenFrameBuf, m_ClearValues);

		OffscreenCmdBuf->SetViewport(0, { viewport });
		OffscreenCmdBuf->SetScissor(0, { scissor });

		vkCmdBindPipeline(OffscreenCmdBuf->GetHandle(), VK_PIPELINE_BIND_POINT_GRAPHICS, m_GraphicsPipeline->GetPipeline());

		VkDeviceSize offsets[1] = { 0 };

		// For every mesh bind it's model and descriptor set info
		t_reg.view<MeshRenderer>().each([&](MeshRenderer& t_MeshRend)
		{
			Fling::Model* Model = t_MeshRend.m_Model;
			if (!Model)
			{
				return;
			}

			// #TODO UPDATE UNIFORM BUF of the mesh --------

			// If the mesh has no descriptor sets, then build them
			// #TODO Investigate a better way to do this, probably by just moving the 
			// descriptors off of the mesh
			if (t_MeshRend.m_DescriptorSets.empty())
			{
				CreateMeshDescriptorSet(t_MeshRend, VK_NULL_HANDLE, *m_OffscreenFrameBuf);
			}

			// Bind the descriptor set for rendering a mesh using the dynamic offset
			vkCmdBindDescriptorSets(
				OffscreenCmdBuf->GetHandle(),
				VK_PIPELINE_BIND_POINT_GRAPHICS,
				m_GraphicsPipeline->GetPipelineLayout(),
				0,
				1,
				&t_MeshRend.m_DescriptorSets[t_ActiveFrameInFlight],
				0,
				nullptr);

			VkBuffer vertexBuffers[1] = { Model->GetVertexBuffer()->GetVkBuffer() };
			// Render the mesh
			vkCmdBindVertexBuffers(OffscreenCmdBuf->GetHandle(), 0, 1, vertexBuffers, offsets);
			vkCmdBindIndexBuffer(OffscreenCmdBuf->GetHandle(), Model->GetIndexBuffer()->GetVkBuffer(), 0, Model->GetIndexType());
			vkCmdDrawIndexed(OffscreenCmdBuf->GetHandle(), Model->GetIndexCount(), 1, 0, 0, 0);
		});

		OffscreenCmdBuf->EndRenderPass();

		OffscreenCmdBuf->End();
	}

	void OffscreenSubpass::CreateDescriptorSets(VkDescriptorPool t_Pool, entt::registry& t_reg)
	{
		
	}

	void OffscreenSubpass::CreateMeshDescriptorSet(MeshRenderer& t_MeshRend, VkDescriptorPool t_Pool, FrameBuffer& t_FrameBuf)
	{
		size_t ImageCount = m_SwapChain->GetImageCount();
		t_MeshRend.m_DescriptorSets.resize(ImageCount);

		std::vector<VkDescriptorSetLayout> layouts(ImageCount, m_GraphicsPipeline->GetDescriptorSetLayout());
		VkDescriptorSetAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		// If we have specified a specific pool then use that, otherwise use the one on the mesh
		allocInfo.descriptorPool = t_MeshRend.m_DescriptorPool;
		allocInfo.descriptorSetCount = static_cast<UINT32>(ImageCount);
		allocInfo.pSetLayouts = layouts.data();

		VK_CHECK_RESULT(vkAllocateDescriptorSets(m_Device->GetVkDevice(), &allocInfo, t_MeshRend.m_DescriptorSets.data()));

		for (size_t i = 0; i < t_MeshRend.m_DescriptorSets.size(); ++i)
		{	
			std::vector<VkWriteDescriptorSet> writeDescriptorSets =
			{
				// 0: UBO
				Initializers::WriteDescriptorSetUniform(
					t_MeshRend.m_UniformBuffers[i],
					t_MeshRend.m_DescriptorSets[i],
					0
				),
				// 1: Color map 
				Initializers::WriteDescriptorSetImage(
					t_MeshRend.m_Material->GetTexture().m_AlbedoTexture,
					t_MeshRend.m_DescriptorSets[i],
					1),
				// 2: Normal map
				Initializers::WriteDescriptorSetImage(
					t_MeshRend.m_Material->GetTexture().m_NormalTexture,
					t_MeshRend.m_DescriptorSets[i],
					2)
				// Any other PBR textures or other samplers go HERE and you add to the MRT shader
			};

			vkUpdateDescriptorSets(m_Device->GetVkDevice(), static_cast<UINT32>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr);
		}
	}

	void OffscreenSubpass::PrepareAttachments()
	{
		assert(m_OffscreenFrameBuf == nullptr);

		m_OffscreenFrameBuf = new FrameBuffer(m_Device->GetVkDevice(), FRAME_BUF_DIM, FRAME_BUF_DIM);

		AttachmentCreateInfo attachmentInfo = {};

		// Four attachments (3 color, 1 depth)
		attachmentInfo.Width = FRAME_BUF_DIM;
		attachmentInfo.Height = FRAME_BUF_DIM;
		attachmentInfo.LayerCount = 1;
		attachmentInfo.Usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

		// Color attachments
		// Attachment 0: (World space) Positions
		attachmentInfo.Format = VK_FORMAT_R16G16B16A16_SFLOAT;
		m_OffscreenFrameBuf->AddAttachment(attachmentInfo);

		// Attachment 1: (World space) Normals
		attachmentInfo.Format = VK_FORMAT_R16G16B16A16_SFLOAT;
		m_OffscreenFrameBuf->AddAttachment(attachmentInfo);

		// Attachment 2: Albedo (color)
		attachmentInfo.Format = VK_FORMAT_R8G8B8A8_UNORM;
		m_OffscreenFrameBuf->AddAttachment(attachmentInfo);

		// Depth attachment
		// Find a suitable depth format
		VkFormat attDepthFormat;
		const PhysicalDevice* PhysDevice = m_Device->GetPhysicalDevice();
		assert(PhysDevice);

		VkBool32 validDepthFormat = PhysDevice->GetSupportedDepthFormat(&attDepthFormat);
		assert(validDepthFormat);

		attachmentInfo.Format = attDepthFormat;
		attachmentInfo.Usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
		m_OffscreenFrameBuf->AddAttachment(attachmentInfo);

		// Create sampler to sample from the color attachments
		VK_CHECK_RESULT(m_OffscreenFrameBuf->CreateSampler(VK_FILTER_NEAREST, VK_FILTER_NEAREST, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE));
		VK_CHECK_RESULT(m_OffscreenFrameBuf->CreateRenderPass());
		F_LOG_TRACE("Offscreen render pass created...");
	}

	void OffscreenSubpass::CreateGraphicsPipeline()
	{
		assert(m_OffscreenFrameBuf);
		VkRenderPass RenderPass = m_OffscreenFrameBuf->GetRenderPassHandle();
		assert(RenderPass != VK_NULL_HANDLE);

		// Change anything about the graphics pipeline here
		// Add color blend states
		m_GraphicsPipeline->m_RasterizationState = 
			Initializers::PipelineRasterizationStateCreateInfo(
				VK_POLYGON_MODE_FILL,
				VK_CULL_MODE_BACK_BIT,
				VK_FRONT_FACE_CLOCKWISE,
				0);

		// Color Attachment --------
		// Blend attachment states required for all color attachments
		// This is important, as color write mask will otherwise be 0x0 and you
		// won't see anything rendered to the attachment
		m_GraphicsPipeline->m_ColorBlendAttachmentStates = {
			Initializers::PipelineColorBlendAttachmentState(0xf, VK_FALSE),
			Initializers::PipelineColorBlendAttachmentState(0xf, VK_FALSE),
			Initializers::PipelineColorBlendAttachmentState(0xf, VK_FALSE)
		};

		m_GraphicsPipeline->m_ColorBlendState.attachmentCount =
			static_cast<uint32_t>(m_GraphicsPipeline->m_ColorBlendAttachmentStates.size());

		m_GraphicsPipeline->m_ColorBlendState.pAttachments = 
			m_GraphicsPipeline->m_ColorBlendAttachmentStates.data();

		// Depth attachment -----
		m_GraphicsPipeline->m_DepthStencilState =
			Initializers::PipelineDepthStencilStateCreateInfo(
				VK_TRUE,
				VK_TRUE,
				VK_COMPARE_OP_LESS_OR_EQUAL);

		m_GraphicsPipeline->m_ViewportState =
			Initializers::PipelineViewportStateCreateInfo(1, 1, 0);

		m_GraphicsPipeline->m_MultisampleState =
			Initializers::PipelineMultiSampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT, 0);
		
		std::vector<VkDynamicState> dynamicStateEnables = 
		{
			VK_DYNAMIC_STATE_VIEWPORT,
			VK_DYNAMIC_STATE_SCISSOR
		};

		m_GraphicsPipeline->m_DynamicState =
			Initializers::PipelineDynamicStateCreateInfo(
				dynamicStateEnables.data(), 
				dynamicStateEnables.size(), 
				0);

		m_GraphicsPipeline->CreateGraphicsPipeline(RenderPass, nullptr);
	}

	void OffscreenSubpass::GatherPresentDependencies(std::vector<CommandBuffer*>& t_CmdBuffs, std::vector<VkSemaphore>& t_Deps, UINT32 t_ActiveFrameIndex)
	{
		t_CmdBuffs.emplace_back(m_OffscreenCmdBufs[t_ActiveFrameIndex]);
		t_Deps.emplace_back(m_OffscreenSemaphores[t_ActiveFrameIndex]);
	}

	void OffscreenSubpass::OnMeshRendererAdded(entt::entity t_Ent, entt::registry& t_Reg, MeshRenderer& t_MeshRend)
	{
		// Build the UBO's for mesh renderers
		F_LOG_TRACE("[OffscreenSubpass] Mesh renderer added!");
		// Initialize and map the UBO of each mesh renderer
		size_t ImageCount = m_SwapChain->GetImageCount();
		t_MeshRend.m_UniformBuffers.resize(ImageCount);

		VkDeviceSize bufferSize = sizeof(UboVS);
		for (size_t i = 0; i < ImageCount; i++)
		{
			t_MeshRend.m_UniformBuffers[i] = new Buffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
			t_MeshRend.m_UniformBuffers[i]->MapMemory(bufferSize);
		}
		
		// I would love to create some descriptor sets here		
		assert(m_OffscreenFrameBuf);
		CreateMeshDescriptorSet(t_MeshRend, VK_NULL_HANDLE, *m_OffscreenFrameBuf);
	}
}   // namespace Fling