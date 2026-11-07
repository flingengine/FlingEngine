#include "Buffer.h"
#include "CommandBuffer.h"
#include "FirstPersonCamera.h"
#include "FlingVulkan.h"
#include "FrameBuffer.h"
#include "GraphicsHelpers.h"
#include "GraphicsPipeline.h"
#include "Logger.h"
#include "LogicalDevice.h"
#include "Material.h"
#include "Model.h"
#include "OffscreenSubpass.h"
#include "PhyscialDevice.h"
#include "SwapChain.h"
#include "UniformBufferObject.h"

namespace Fling
{
	OffscreenSubpass::OffscreenSubpass(
		const LogicalDevice* t_Dev,
		const Swapchain* t_Swap,
		FirstPersonCamera* t_Cam,
		std::shared_ptr<Fling::Shader> t_Vert,
		std::shared_ptr<Fling::Shader> t_Frag)
		: Subpass(t_Dev, t_Swap, t_Vert, t_Frag)
		, m_Camera(t_Cam)
	{
		// Set the clear values for the G Buffer
		m_ClearValues.resize(6);
		m_ClearValues[0].color = m_ClearValues[1].color = { { 0.0f, 0.0f, 0.0f, 0.0f } };
		m_ClearValues[2].color = m_ClearValues[3].color = m_ClearValues[4].color = { { 0.0f, 0.0f, 0.0f, 0.0f } };
		m_ClearValues[5].depthStencil = { 1.0f, 0 };

		// Build offscreen semaphores -------
		m_OffscreenSemaphores.resize(VkConfig::MAX_FRAMES_IN_FLIGHT);
		for (int32 i = 0; i < VkConfig::MAX_FRAMES_IN_FLIGHT; i++)
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
		m_RecordedDrawListVersions.resize(m_OffscreenCmdBufs.size(), 0);

		// One camera UBO is enough because each frame waits for the GPU to finish
		// before the next frame writes it again
		VkDeviceSize CameraBufferSize = sizeof(OffscreenCameraUBO);
		m_CameraUboBuffer = new Buffer(CameraBufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
		m_CameraUboBuffer->MapMemory(CameraBufferSize);

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
			delete CmdBuf;
			CmdBuf = nullptr;
		}
		m_OffscreenCmdBufs.clear();

		vkDestroyCommandPool(m_Device->GetVkDevice(), m_CommandPool, nullptr);

		delete m_OffscreenFrameBuf;
		m_OffscreenFrameBuf = nullptr;

		delete m_CameraUboBuffer;
		m_CameraUboBuffer = nullptr;
	}

	void OffscreenSubpass::Draw(
		CommandBuffer& t_CmdBuf,
		uint32 t_ActiveSwapImage,
		const RenderSnapshot& t_Snapshot,
		float DeltaTime)
	{
		assert(m_GraphicsPipeline);
		// Don't use the given command buffer, instead build the OFFSCREEN command buffer
		CommandBuffer* OffscreenCmdBuf = m_OffscreenCmdBufs[t_ActiveSwapImage];
		assert(OffscreenCmdBuf);

		// The previous frame is done on the GPU, so the resources of destroyed meshes can go
		ReleaseRetiredMeshResources();

		// The meshes in this frame's snapshot: new ones get a proxy, gone ones release theirs
		SyncProxies(t_Snapshot);

		// The camera moves on most frames, but it is only one small write
		OffscreenCameraUBO CameraUBO = {};
		// Invert the project value to match the proper coordinate space compared to OpenGL
		CameraUBO.Projection = m_Camera->GetProjectionMatrix();
		CameraUBO.Projection[1][1] *= -1.0f;
		CameraUBO.View = m_Camera->GetViewMatrix();
		memcpy(m_CameraUboBuffer->m_MappedMem, &CameraUBO, sizeof(CameraUBO));

		UpdateMeshUniformBuffers(t_Snapshot);

		// The command buffer holds only draw calls and descriptor bindings, not
		// UBO data. Keep the last recording if the draw list did not change.
		if (m_RecordedDrawListVersions[t_ActiveSwapImage] != m_DrawListVersion)
		{
			BuildOffscreenCommandBuffer(*OffscreenCmdBuf);
			m_RecordedDrawListVersions[t_ActiveSwapImage] = m_DrawListVersion;
		}
	}

	void OffscreenSubpass::SyncProxies(const RenderSnapshot& t_Snapshot)
	{
		MeshProxyTable::SyncResult Result = m_Proxies.Sync(t_Snapshot.Meshes);
		m_ItemSlots = std::move(Result.ItemSlots);
		m_GpuMeshes.resize(m_Proxies.Capacity());

		// Removed first: a removed slot can be used again by an added mesh
		for (const uint32 Slot : Result.Removed)
		{
			RetireGpuMesh(m_GpuMeshes[Slot]);
		}

		auto Prepare = [&](const uint32 Slot)
		{
			GpuMesh& Mesh = m_GpuMeshes[Slot];
			const bool bHasAssets = ResolveMeshAssets(m_Proxies.At(Slot), Mesh);
			Mesh.Drawn = bHasAssets && Mesh.MeshMaterial->GetType() == Material::Type::Default;
			if (!Mesh.Drawn)
			{
				// Another pass draws it (a debug material), or there is no model
				RetireGpuMesh(Mesh);
				return;
			}

			if (Mesh.Ubo == nullptr)
			{
				const VkDeviceSize BufferSize = sizeof(OffscreenUBO);
				Mesh.Ubo = new Buffer(BufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
				Mesh.Ubo->MapMemory(BufferSize);
			}
			CreateMeshDescriptorSet(Mesh);
			Mesh.UploadedVersion = 0;
		};

		for (const uint32 Slot : Result.Added)
		{
			Prepare(Slot);
		}
		for (const uint32 Slot : Result.Changed)
		{
			// The descriptor set may be in a recorded command buffer, so take a new one
			RetireGpuMesh(m_GpuMeshes[Slot]);
			Prepare(Slot);
		}

		if (Result.DrawListChanged())
		{
			InvalidateCommandBuffers();
		}
	}

	void OffscreenSubpass::RetireGpuMesh(GpuMesh& t_Mesh)
	{
		if (t_Mesh.Set != VK_NULL_HANDLE)
		{
			m_RetiredDescriptorSets.emplace_back(t_Mesh.Set);
		}
		if (t_Mesh.Ubo != nullptr)
		{
			m_RetiredUniformBuffers.emplace_back(t_Mesh.Ubo);
		}
		t_Mesh = GpuMesh{};
	}

	void OffscreenSubpass::UpdateMeshUniformBuffers(const RenderSnapshot& t_Snapshot)
	{
		for (size_t i = 0; i < t_Snapshot.Meshes.size() && i < m_ItemSlots.size(); ++i)
		{
			const MeshDrawItem& Item = t_Snapshot.Meshes[i];
			GpuMesh& Mesh = m_GpuMeshes[m_ItemSlots[i]];
			if (!Mesh.Drawn || Mesh.Ubo == nullptr)
			{
				continue;
			}

			// Version 0 has no LocalToWorld yet, so its matrix can change on any frame
			if (Item.WorldVersion != 0 && Item.WorldVersion == Mesh.UploadedVersion)
			{
				continue;
			}

			OffscreenUBO MeshUBO = {};
			MeshUBO.Model = Item.World;
			MeshUBO.ObjPos = glm::vec3(Item.World[3]);
			memcpy(Mesh.Ubo->m_MappedMem, &MeshUBO, sizeof(MeshUBO));
			Mesh.UploadedVersion = Item.WorldVersion;
		}
	}

	void OffscreenSubpass::BuildOffscreenCommandBuffer(CommandBuffer& t_CmdBuf)
	{
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

		t_CmdBuf.Begin();
		t_CmdBuf.BeginRenderPass(*m_OffscreenFrameBuf, m_ClearValues);

		t_CmdBuf.SetViewport(0, { viewport });
		t_CmdBuf.SetScissor(0, { scissor });

		vkCmdBindPipeline(t_CmdBuf.GetHandle(), VK_PIPELINE_BIND_POINT_GRAPHICS, m_GraphicsPipeline->GetPipeline());

		VkDeviceSize offsets[1] = { 0 };

		for (const GpuMesh& Mesh : m_GpuMeshes)
		{
			if (!Mesh.Drawn || Mesh.MeshModel == nullptr || Mesh.Set == VK_NULL_HANDLE)
			{
				continue;
			}
			Fling::Model* Model = Mesh.MeshModel;

			vkCmdBindDescriptorSets(
				t_CmdBuf.GetHandle(),
				VK_PIPELINE_BIND_POINT_GRAPHICS,
				m_GraphicsPipeline->GetPipelineLayout(),
				0,
				1,
				&Mesh.Set,
				0,
				nullptr);

			VkBuffer vertexBuffers[1] = { Model->GetVertexBuffer()->GetVkBuffer() };
			// Render the mesh
			vkCmdBindVertexBuffers(t_CmdBuf.GetHandle(), 0, 1, vertexBuffers, offsets);
			vkCmdBindIndexBuffer(t_CmdBuf.GetHandle(), Model->GetIndexBuffer()->GetVkBuffer(), 0, Model->GetIndexType());
			vkCmdDrawIndexed(t_CmdBuf.GetHandle(), Model->GetIndexCount(), 1, 0, 0, 0);
		}

		t_CmdBuf.EndRenderPass();

		t_CmdBuf.End();
	}

	void OffscreenSubpass::CreateMeshDescriptorSet(GpuMesh& t_Mesh)
	{
		if (t_Mesh.Set == VK_NULL_HANDLE)
		{
			VkDescriptorSetLayout layout = m_GraphicsPipeline->GetDescriptorSetLayout();
			VkDescriptorSetAllocateInfo allocInfo = {};
			allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
			allocInfo.descriptorPool = m_DescriptorPool;
			allocInfo.descriptorSetCount = 1;
			allocInfo.pSetLayouts = &layout;

			VK_CHECK_RESULT(vkAllocateDescriptorSets(m_Device->GetVkDevice(), &allocInfo, &t_Mesh.Set));
		}

		const PBRTextures& Textures = t_Mesh.MeshMaterial->GetPBRTextures();
		std::vector<VkWriteDescriptorSet> writeDescriptorSets =
		{
			// 0: UBO
			Initializers::WriteDescriptorSetUniform(t_Mesh.Ubo, t_Mesh.Set, 0),
			// 1: Color map
			Initializers::WriteDescriptorSetImage(Textures.m_AlbedoTexture, t_Mesh.Set, 1),
			// 2: Normal map
			Initializers::WriteDescriptorSetImage(Textures.m_NormalTexture, t_Mesh.Set, 2),
			// 3: Metal map
			Initializers::WriteDescriptorSetImage(Textures.m_MetalTexture, t_Mesh.Set, 3),
			// 4: Roughness map
			Initializers::WriteDescriptorSetImage(Textures.m_RoughnessTexture, t_Mesh.Set, 4),
			// Any other PBR textures or other samplers go HERE and you add to the MRT shader

			// 5: Camera UBO
			Initializers::WriteDescriptorSetUniform(m_CameraUboBuffer, t_Mesh.Set, 5)
		};

		vkUpdateDescriptorSets(m_Device->GetVkDevice(), static_cast<uint32>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr);
	}

	void OffscreenSubpass::PrepareAttachments()
	{
		assert(m_OffscreenFrameBuf == nullptr);

		VkExtent2D Extents = m_SwapChain->GetExtents();

		m_OffscreenFrameBuf = new FrameBuffer(m_Device, Extents.width, Extents.height);

		AttachmentCreateInfo attachmentInfo = {};

		// Four attachments (3 color, 1 depth)
		attachmentInfo.Width = Extents.width;
		attachmentInfo.Height = Extents.height;
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

		PhysDevice->GetSupportedDepthFormat(&attDepthFormat);
		
		// Attachment 3: Depth
		attachmentInfo.Format = attDepthFormat;
		attachmentInfo.Usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
		m_OffscreenFrameBuf->AddAttachment(attachmentInfo);

		// Create sampler to sample from the color attachments
		VK_CHECK_RESULT(m_OffscreenFrameBuf->CreateSampler(VK_FILTER_NEAREST, VK_FILTER_NEAREST, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE));
		VK_CHECK_RESULT(m_OffscreenFrameBuf->CreateRenderPass());
		F_LOG_TRACE("Offscreen render pass created...");

		// Create the descriptor pool for off screen things
		uint32 DescriptorCount = 2000 * m_SwapChain->GetImageViewCount();

		static std::vector<VkDescriptorPoolSize> poolSizes =
		{
			Initializers::DescriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 		DescriptorCount),
			Initializers::DescriptorPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 			DescriptorCount),
			Initializers::DescriptorPoolSize(VK_DESCRIPTOR_TYPE_SAMPLER, 				DescriptorCount),
			Initializers::DescriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, DescriptorCount),
			Initializers::DescriptorPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 		DescriptorCount)
		};

		VkDescriptorPoolCreateInfo poolInfo = {};
		poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		// Each mesh frees its set when it is destroyed (see ReleaseRetiredMeshResources)
		poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
		poolInfo.poolSizeCount = static_cast<uint32>(poolSizes.size());
		poolInfo.pPoolSizes = poolSizes.data();
		poolInfo.maxSets = to_u32(1000 * m_SwapChain->GetImageViewCount());

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

	void OffscreenSubpass::GatherPresentDependencies(std::vector<CommandBuffer*>& t_CmdBuffs, std::vector<VkSemaphore>& t_Deps, uint32 t_ActiveFrameIndex, uint32 t_CurrentFrameInFlight)
	{
		t_CmdBuffs.emplace_back(m_OffscreenCmdBufs[t_ActiveFrameIndex]);
		t_Deps.emplace_back(m_OffscreenSemaphores[t_CurrentFrameInFlight]);
	}

	void OffscreenSubpass::CleanUp()
	{
		assert(m_Device != nullptr);

		// Called after the device is idle, so nothing uses these now
		for (GpuMesh& Mesh : m_GpuMeshes)
		{
			RetireGpuMesh(Mesh);
		}
		m_GpuMeshes.clear();
		m_Proxies.Clear();
		m_ItemSlots.clear();
		ReleaseRetiredMeshResources();

		if (m_DescriptorPool != VK_NULL_HANDLE)
		{
			vkDestroyDescriptorPool(m_Device->GetVkDevice(), m_DescriptorPool, nullptr);
			m_DescriptorPool = VK_NULL_HANDLE;
		}
	}

	void OffscreenSubpass::OnSwapchainResized()
	{
		const VkExtent2D NewSize = m_SwapChain->GetExtents();
		if (m_OffscreenFrameBuf)
		{
			m_OffscreenFrameBuf->ResizeAndRecreate(NewSize.width, NewSize.height);
			// The offscreen render pass is new. Rebuild the pipeline that uses it.
			CreateGraphicsPipeline();
		}

		// The old recordings use the old frame buffer and pipeline
		InvalidateCommandBuffers();
	}

	void OffscreenSubpass::ReleaseRetiredMeshResources()
	{
		if (!m_RetiredDescriptorSets.empty() && m_DescriptorPool != VK_NULL_HANDLE)
		{
			VK_CHECK_RESULT(vkFreeDescriptorSets(
				m_Device->GetVkDevice(),
				m_DescriptorPool,
				static_cast<uint32>(m_RetiredDescriptorSets.size()),
				m_RetiredDescriptorSets.data()));
		}
		m_RetiredDescriptorSets.clear();

		for (Buffer* Ubo : m_RetiredUniformBuffers)
		{
			delete Ubo;
		}
		m_RetiredUniformBuffers.clear();
	}
}   // namespace Fling