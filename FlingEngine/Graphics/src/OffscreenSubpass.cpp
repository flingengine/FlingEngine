#include "OffscreenSubpass.h"
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

#define FRAME_BUF_DIM 2048

namespace Fling
{
	OffscreenSubpass::OffscreenSubpass(
		const LogicalDevice* t_Dev,
		const Swapchain* t_Swap,
		entt::registry& t_reg,
		FirstPersonCamera* t_Cam,
		std::shared_ptr<Fling::Shader> t_Vert,
		std::shared_ptr<Fling::Shader> t_Frag)
		: Subpass(t_Dev, t_Swap, t_Vert, t_Frag)
		, m_Camera(t_Cam)
	{
		assert(m_Camera);

		t_reg.on_construct<MeshRenderer>().connect<&OffscreenSubpass::OnMeshRendererAdded>(*this);

		// Set the clear values for the G Buffer
		m_ClearValues.resize(6);
		m_ClearValues[0].color = m_ClearValues[1].color = { { 0.0f, 0.0f, 0.0f, 0.0f } };
		m_ClearValues[2].color = m_ClearValues[3].color = m_ClearValues[4].color = { { 0.0f, 0.0f, 0.0f, 0.0f } };
		m_ClearValues[5].depthStencil = { 1.0f, 0 };

		// Build offscreen semaphores -------
		m_OffscreenSemaphores.resize(VkConfig::MAX_FRAMES_IN_FLIGHT);
		for (INT32 i = 0; i < VkConfig::MAX_FRAMES_IN_FLIGHT; i++)
		{
			m_OffscreenSemaphores[i] = GraphicsHelpers::CreateSemaphore(m_Device->GetVkDevice());
		}

		GraphicsHelpers::CreateCommandPool(&m_CommandPool, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

		// Build offscreen command buffers
		m_OffscreenCmdBufs.resize(m_SwapChain->GetImageCount());
		for (size_t i = 0; i < m_OffscreenCmdBufs.size(); ++i)
		{
			m_OffscreenCmdBufs[i] = new Fling::CommandBuffer(m_Device, m_CommandPool);
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

		vkDestroyCommandPool(m_Device->GetVkDevice(), m_CommandPool, nullptr);

		if (m_OffscreenFrameBuf)
		{
			delete m_OffscreenFrameBuf;
			m_OffscreenFrameBuf = nullptr;
		}
	}

	void OffscreenSubpass::Draw(CommandBuffer& t_CmdBuf, VkFramebuffer t_PresentFrameBuf, UINT32 t_ActiveFrameInFlight, entt::registry& t_reg, float DeltaTime)
	{
		assert(m_GraphicsPipeline);
		// Don't use the given command buffer, instead build the OFFSCREEN command buffer
		CommandBuffer* OffscreenCmdBuf = m_OffscreenCmdBufs[t_ActiveFrameInFlight];
		assert(OffscreenCmdBuf);

		// Set viewport and scissors to the offscreen frame buffer
		VkViewport viewport = Initializers::Viewport(
			static_cast<float>(m_OffscreenFrameBuf->GetWidth()), 
			static_cast<float>(m_OffscreenFrameBuf->GetHeight()),
			0.0f, 1.0f
		);

		VkRect2D scissor = Initializers::Rect2D(
			m_OffscreenFrameBuf->GetWidth(),
			m_OffscreenFrameBuf->GetHeight(),
			/** offsetX */ 0,
			/** offsetY */ 0
		);

		OffscreenCmdBuf->Begin();
		OffscreenCmdBuf->BeginRenderPass(*m_OffscreenFrameBuf, m_ClearValues);

		OffscreenCmdBuf->SetViewport(0, { viewport });
		OffscreenCmdBuf->SetScissor(0, { scissor });

		vkCmdBindPipeline(OffscreenCmdBuf->GetHandle(), VK_PIPELINE_BIND_POINT_GRAPHICS, m_GraphicsPipeline->GetPipeline());

		VkDeviceSize offsets[1] = { 0 };

		// For every mesh bind it's model and descriptor set info
		t_reg.view<MeshRenderer, Transform>().each([&](MeshRenderer& t_MeshRend, Transform& t_trans)
		{
			Fling::Model* Model = t_MeshRend.m_Model;
			if (!Model)
			{
				return;
			}

			// UPDATE UNIFORM BUF of the mesh --------
			OffscreenUBO CurrentUBO = {};
			Transform::CalculateWorldMatrix(t_trans);

			// Invert the project value to match the proper coordinate space compared to OpenGL
			CurrentUBO.Projection = m_Camera->GetProjectionMatrix();
			CurrentUBO.Projection[1][1] *= -1.0f;
			CurrentUBO.Model = t_trans.GetWorldMatrix();
			CurrentUBO.View = m_Camera->GetViewMatrix();
			CurrentUBO.ObjPos = t_trans.GetPos();

			// Memcpy to the buffer
			Buffer* buf = t_MeshRend.m_UniformBuffers[t_ActiveFrameInFlight];
			memcpy(buf->m_MappedMem, &CurrentUBO, buf->GetSize());

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
		allocInfo.descriptorPool = m_DescriptorPool;
		allocInfo.descriptorSetCount = static_cast<UINT32>(ImageCount);
		allocInfo.pSetLayouts = layouts.data();

		VK_CHECK_RESULT(vkAllocateDescriptorSets(m_Device->GetVkDevice(), &allocInfo, t_MeshRend.m_DescriptorSets.data()));

		// Ensure that we have a material to try and sample from
		if (t_MeshRend.m_Material == nullptr)
		{
			t_MeshRend.LoadMaterialFromPath("Materials/Default.mat");
		}

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
					t_MeshRend.m_Material->GetPBRTextures().m_AlbedoTexture,
					t_MeshRend.m_DescriptorSets[i],
					1),
				// 2: Normal map
				Initializers::WriteDescriptorSetImage(
					t_MeshRend.m_Material->GetPBRTextures().m_NormalTexture,
					t_MeshRend.m_DescriptorSets[i],
					2),
				// 3: Metal map
				Initializers::WriteDescriptorSetImage(
					t_MeshRend.m_Material->GetPBRTextures().m_MetalTexture,
					t_MeshRend.m_DescriptorSets[i],
					3),
				// 4: Roughness map
				Initializers::WriteDescriptorSetImage(
					t_MeshRend.m_Material->GetPBRTextures().m_RoughnessTexture,
					t_MeshRend.m_DescriptorSets[i],
					4)
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

		// Attachment 3: Metal
		attachmentInfo.Format = VK_FORMAT_R8G8B8A8_UNORM;
		m_OffscreenFrameBuf->AddAttachment(attachmentInfo);

		// Attachment 4: Roughness
		attachmentInfo.Format = VK_FORMAT_R8G8B8A8_UNORM;
		m_OffscreenFrameBuf->AddAttachment(attachmentInfo);

		// Depth attachment
		// Find a suitable depth format
		VkFormat attDepthFormat;
		const PhysicalDevice* PhysDevice = m_Device->GetPhysicalDevice();
		assert(PhysDevice);

		VkBool32 validDepthFormat = PhysDevice->GetSupportedDepthFormat(&attDepthFormat);
		assert(validDepthFormat);
		// Attachment 3: Depth
		attachmentInfo.Format = attDepthFormat;
		attachmentInfo.Usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
		m_OffscreenFrameBuf->AddAttachment(attachmentInfo);

		// Create sampler to sample from the color attachments
		VK_CHECK_RESULT(m_OffscreenFrameBuf->CreateSampler(VK_FILTER_NEAREST, VK_FILTER_NEAREST, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE));
		VK_CHECK_RESULT(m_OffscreenFrameBuf->CreateRenderPass());
		F_LOG_TRACE("Offscreen render pass created...");


		// Create the descriptor pool for offscreen thigns
		UINT32 DescriptorCount = 100;

		std::vector<VkDescriptorPoolSize> poolSizes =
		{
			Initializers::DescriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 		DescriptorCount),
			Initializers::DescriptorPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 			DescriptorCount),
			Initializers::DescriptorPoolSize(VK_DESCRIPTOR_TYPE_SAMPLER, 				DescriptorCount),
			Initializers::DescriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, DescriptorCount),
			Initializers::DescriptorPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 		DescriptorCount)
		};

		VkDescriptorPoolCreateInfo poolInfo = {};
		poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		poolInfo.poolSizeCount = static_cast<UINT32>(poolSizes.size());
		poolInfo.pPoolSizes = poolSizes.data();
		poolInfo.maxSets = to_u32(m_SwapChain->GetImageViewCount());

		if (vkCreateDescriptorPool(m_Device->GetVkDevice(), &poolInfo, nullptr, &m_DescriptorPool) != VK_SUCCESS)
		{
			F_LOG_FATAL("Failed to create descriptor pool");
		}
	}

	void OffscreenSubpass::CreateGraphicsPipeline()
	{
		assert(m_OffscreenFrameBuf);
		VkRenderPass RenderPass = m_OffscreenFrameBuf->GetRenderPassHandle();
		assert(RenderPass != VK_NULL_HANDLE);

		m_GraphicsPipeline->m_RasterizationState =
			Initializers::PipelineRasterizationStateCreateInfo(
				VK_POLYGON_MODE_FILL,
				VK_CULL_MODE_BACK_BIT,
				VK_FRONT_FACE_COUNTER_CLOCKWISE
			);

		// Color Attachment --------
		// Blend attachment states required for all color attachments
		// This is important, as color write mask will otherwise be 0x0 and you
		// won't see anything rendered to the attachment
		m_GraphicsPipeline->m_ColorBlendAttachmentStates = 
		{
			Initializers::PipelineColorBlendAttachmentState(0xf, VK_FALSE),
			Initializers::PipelineColorBlendAttachmentState(0xf, VK_FALSE),
			Initializers::PipelineColorBlendAttachmentState(0xf, VK_FALSE),
			Initializers::PipelineColorBlendAttachmentState(0xf, VK_FALSE),
			Initializers::PipelineColorBlendAttachmentState(0xf, VK_FALSE)
		};

		m_GraphicsPipeline->m_ColorBlendState.attachmentCount =
			static_cast<uint32_t>(m_GraphicsPipeline->m_ColorBlendAttachmentStates.size());

		m_GraphicsPipeline->m_ColorBlendState.pAttachments = 
			m_GraphicsPipeline->m_ColorBlendAttachmentStates.data();

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

	void OffscreenSubpass::GatherPresentDependencies(std::vector<CommandBuffer*>& t_CmdBuffs, std::vector<VkSemaphore>& t_Deps, UINT32 t_ActiveFrameIndex, UINT32 t_CurrentFrameInFlight)
	{
		t_CmdBuffs.emplace_back(m_OffscreenCmdBufs[t_ActiveFrameIndex]);
		t_Deps.emplace_back(m_OffscreenSemaphores[t_CurrentFrameInFlight]);
	}

	void OffscreenSubpass::CleanUp(entt::registry& t_reg)
	{
		assert(m_Device != nullptr);
		// t_reg.view<MeshRenderer>().each([&](MeshRenderer& t_MeshRend)
		// {
		// 	t_MeshRend.Release();
		// 	if (t_MeshRend.m_DescriptorPool != VK_NULL_HANDLE)
		// 	{
		// 		//vkDestroyDescriptorPool(m_Device->GetVkDevice(), t_MeshRend.m_DescriptorPool, nullptr);
		// 		//t_MeshRend.m_DescriptorPool = VK_NULL_HANDLE;
		// 	}
		// });


		vkDestroyDescriptorPool(m_Device->GetVkDevice(), m_DescriptorPool, nullptr);
		m_DescriptorPool = VK_NULL_HANDLE;
	}

	void OffscreenSubpass::OnMeshRendererAdded(entt::entity t_Ent, entt::registry& t_Reg, MeshRenderer& t_MeshRend)
	{
		// #TODO If this mesh renderer material is set to deferred, then do this
		if (t_MeshRend.m_Material && t_MeshRend.m_Material->GetType() == Material::Type::Default)
		{
			t_Reg.assign<entt::tag<HS("Default")>>(t_Ent);
		}

		// Initialize the mesh renderer to have a descriptor pool that it can use
		size_t ImageCount = m_SwapChain->GetImageCount();
		VkDevice Device = m_Device->GetVkDevice();

		// Create descriptor pools for this mesh
		// {
		// 	UINT32 DescriptorCount = 128;

		// 	std::vector<VkDescriptorPoolSize> poolSizes =
		// 	{
		// 		Initializers::DescriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, DescriptorCount),
		// 		Initializers::DescriptorPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, DescriptorCount),
		// 		Initializers::DescriptorPoolSize(VK_DESCRIPTOR_TYPE_SAMPLER, DescriptorCount),
		// 		Initializers::DescriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, DescriptorCount),
		// 		Initializers::DescriptorPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, DescriptorCount)
		// 	};

		// 	VkDescriptorPoolCreateInfo poolInfo = {};
		// 	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		// 	poolInfo.poolSizeCount = static_cast<UINT32>(poolSizes.size());
		// 	poolInfo.pPoolSizes = poolSizes.data();
		// 	poolInfo.maxSets = ImageCount;

		// 	if (vkCreateDescriptorPool(Device, &poolInfo, nullptr, &t_MeshRend.m_DescriptorPool) != VK_SUCCESS)
		// 	{
		// 		F_LOG_FATAL("Failed to create descriptor pool");
		// 	}
		// }

		t_MeshRend.m_DescriptorPool = m_DescriptorPool;

		// Initialize and map the UBO of each mesh renderer
		t_MeshRend.m_UniformBuffers.resize(ImageCount);

		VkDeviceSize bufferSize = sizeof(OffscreenUBO);
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