#include "GeometrySubpass.h"
#include "FrameBuffer.h"
#include "CommandBuffer.h"
#include "PhyscialDevice.h"
#include "LogicalDevice.h"
#include "GraphicsHelpers.h"
#include "MeshRenderer.h"
#include "SwapChain.h"
#include "UniformBufferObject.h"

#define FRAME_BUF_DIM 2048

namespace Fling
{
	GeometrySubpass::GeometrySubpass(const LogicalDevice* t_Dev,
		const Swapchain* t_Swap,
		entt::registry& t_reg,
		std::shared_ptr<Fling::Shader> t_Vert,
		std::shared_ptr<Fling::Shader> t_Frag)
		: Subpass(t_Dev, t_Swap, t_Vert, t_Frag)
	{
		t_reg.on_construct<MeshRenderer>().connect<&GeometrySubpass::OnMeshRendererAdded>(*this);
		t_reg.on_destroy<MeshRenderer>().connect<&GeometrySubpass::OnMeshRendererRemoved>(*this);

		// Set the clear values for the G Buffer
		m_ClearValues.resize(4);
		m_ClearValues[0].color = m_ClearValues[1].color = { { 0.0f, 0.0f, 0.0f, 0.0f } };
		m_ClearValues[2].color = { { 0.0f, 0.0f, 0.0f, 0.0f } };
		m_ClearValues[3].depthStencil = { 1.0f, 0 };
	}

	GeometrySubpass::~GeometrySubpass()
	{
		// Clean up any allocated UBO's 
		// Clean up any allocated descriptor sets
	}

	void GeometrySubpass::Draw(CommandBuffer& t_CmdBuf, UINT32 t_ActiveFrameInFlight, FrameBuffer& t_FrameBuf, entt::registry& t_reg)
	{
		// Update UBO's 
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
				CreateMeshDescriptorSet(t_MeshRend, VK_NULL_HANDLE, t_FrameBuf);
			}

			// Bind the descriptor set for rendering a mesh using the dynamic offset
			vkCmdBindDescriptorSets(
				t_CmdBuf.GetHandle(),
				VK_PIPELINE_BIND_POINT_GRAPHICS,
				m_PipelineLayout,
				0,
				1,
				&t_MeshRend.m_DescriptorSets[t_ActiveFrameInFlight],
				0,
				nullptr);

			VkBuffer vertexBuffers[1] = { Model->GetVertexBuffer()->GetVkBuffer() };
			VkDeviceSize offsets[1] = { 0 };
			// Render the mesh
			vkCmdBindVertexBuffers(t_CmdBuf.GetHandle(), 0, 1, vertexBuffers, offsets);
			vkCmdBindIndexBuffer(t_CmdBuf.GetHandle(), Model->GetIndexBuffer()->GetVkBuffer(), 0, Model->GetIndexType());
			vkCmdDrawIndexed(t_CmdBuf.GetHandle(), Model->GetIndexCount(), 1, 0, 0, 0);		
		});
	}

	void GeometrySubpass::CreateDescriptorSets(VkDescriptorPool t_Pool, FrameBuffer& t_FrameBuf, entt::registry& t_reg)
	{
		size_t ImageCount = m_SwapChain->GetImageCount();

		t_reg.view<MeshRenderer>().each([&](MeshRenderer& t_MeshRend)
		{
			CreateMeshDescriptorSet(t_MeshRend, t_Pool, t_FrameBuf);
		});
	}

	void GeometrySubpass::CreateMeshDescriptorSet(MeshRenderer& t_MeshRend, VkDescriptorPool t_Pool, FrameBuffer& t_FrameBuf)
	{
		size_t ImageCount = m_SwapChain->GetImageCount();
		t_MeshRend.m_DescriptorSets.resize(ImageCount);

		std::vector<VkDescriptorSetLayout> layouts(ImageCount, m_DescriptorLayout);
		VkDescriptorSetAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		// If we have specified a specific pool then use that, otherwise use the one on the mesh
		allocInfo.descriptorPool = t_Pool != VK_NULL_HANDLE ? t_Pool : t_MeshRend.m_DescriptorPool;
		allocInfo.descriptorSetCount = static_cast<UINT32>(ImageCount);
		allocInfo.pSetLayouts = layouts.data();

		VK_CHECK_RESULT(vkAllocateDescriptorSets(m_Device->GetVkDevice(), &allocInfo, t_MeshRend.m_DescriptorSets.data()));

		for (size_t i = 0; i < t_MeshRend.m_DescriptorSets.size(); ++i)
		{

			// Create the image info's for the write sets to reference
			// that will give us access to the G-Buffer in the shaders
			VkDescriptorImageInfo texDescriptorPosition =
				Initializers::DescriptorImageInfo(
					t_FrameBuf.GetSamplerHandle(),
					t_FrameBuf.GetAttachmentAtIndex(0)->GetViewHandle(),
					VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

			VkDescriptorImageInfo texDescriptorNormal =
				Initializers::DescriptorImageInfo(
					t_FrameBuf.GetSamplerHandle(),
					t_FrameBuf.GetAttachmentAtIndex(1)->GetViewHandle(),
					VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

			VkDescriptorImageInfo texDescriptorAlbedo =
				Initializers::DescriptorImageInfo(
					t_FrameBuf.GetSamplerHandle(),
					t_FrameBuf.GetAttachmentAtIndex(2)->GetViewHandle(),
					VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

			std::vector<VkWriteDescriptorSet> writeDescriptorSets =
			{
				// Uniform buffer for the mesh
				Initializers::WriteDescriptorSetUniform(
					t_MeshRend.m_UniformBuffers[i],
					t_MeshRend.m_DescriptorSets[i],
					0
				),
				// 1 : Position sampler
				Initializers::WriteDescriptorSet(
					t_MeshRend.m_DescriptorSets[i],
					VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
					1,
					&texDescriptorPosition),
				// 2 : Normal sampler
				Initializers::WriteDescriptorSet(
					t_MeshRend.m_DescriptorSets[i],
					VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
					2,
					&texDescriptorNormal),
				// 3 : Albedo sampler
				Initializers::WriteDescriptorSet(
					t_MeshRend.m_DescriptorSets[i],
					VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
					3,
					&texDescriptorAlbedo),
			};

			vkUpdateDescriptorSets(m_Device->GetVkDevice(), static_cast<UINT32>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr);
		}
	}

	void GeometrySubpass::PrepareAttachments(FrameBuffer& t_FrameBuffer)
	{
		AttachmentCreateInfo attachmentInfo = {};

		// Four attachments (3 color, 1 depth)
		attachmentInfo.Width = FRAME_BUF_DIM;
		attachmentInfo.Height = FRAME_BUF_DIM;
		attachmentInfo.LayerCount = 1;
		attachmentInfo.Usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

		// Color attachments
		// Attachment 0: (World space) Positions
		attachmentInfo.Format = VK_FORMAT_R16G16B16A16_SFLOAT;
		t_FrameBuffer.AddAttachment(attachmentInfo);

		// Attachment 1: (World space) Normals
		attachmentInfo.Format = VK_FORMAT_R16G16B16A16_SFLOAT;
		t_FrameBuffer.AddAttachment(attachmentInfo);

		// Attachment 2: Albedo (color)
		attachmentInfo.Format = VK_FORMAT_R8G8B8A8_UNORM;
		t_FrameBuffer.AddAttachment(attachmentInfo);

		// Depth attachment
		// Find a suitable depth format
		VkFormat attDepthFormat;
		const PhysicalDevice* PhysDevice = m_Device->GetPhysicalDevice();
		assert(PhysDevice);

		VkBool32 validDepthFormat = PhysDevice->GetSupportedDepthFormat(&attDepthFormat);
		assert(validDepthFormat);

		attachmentInfo.Format = attDepthFormat;
		attachmentInfo.Usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
		t_FrameBuffer.AddAttachment(attachmentInfo);

		// Create sampler to sample from the color attachments
		VK_CHECK_RESULT(t_FrameBuffer.CreateSampler(VK_FILTER_NEAREST, VK_FILTER_NEAREST, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE));
	}

	void GeometrySubpass::CreateGraphicsPipeline(FrameBuffer& t_FrameBuffer)
	{
		VkRenderPass RenderPass = t_FrameBuffer.GetRenderPassHandle();
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

	void GeometrySubpass::OnMeshRendererAdded(entt::entity t_Ent, entt::registry& t_Reg, MeshRenderer& t_MeshRend)
	{
		// Build the UBO's for mesh renderers
		F_LOG_TRACE("Geom Subpass mesh renderer added!");
		// Initalize and map the UBO of each mesh renderer
		size_t ImageCount = m_SwapChain->GetImageCount();
		t_MeshRend.m_UniformBuffers.resize(ImageCount);

		VkDeviceSize bufferSize = sizeof(UboVS);
		for (size_t i = 0; i < ImageCount; i++)
		{
			t_MeshRend.m_UniformBuffers[i] = new Buffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
			t_MeshRend.m_UniformBuffers[i]->MapMemory(bufferSize);
		}
	}

	void GeometrySubpass::OnMeshRendererRemoved(entt::entity t_Ent, entt::registry& t_Reg)
	{
		F_LOG_TRACE("Geom Subpass mesh renderer REMOVED!");
	}

}   // namespace Fling