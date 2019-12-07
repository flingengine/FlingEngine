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
#include "FirstPersonCamera.h"
#include "Components/Transform.h"

#include "VulkanApp.h"		// #TODO: Lets get rid of this

namespace Fling
{
	GeometrySubpass::GeometrySubpass(
		const LogicalDevice* t_Dev,
		const Swapchain* t_Swap,
		entt::registry& t_reg,
		FirstPersonCamera* t_Cam,
		FrameBuffer* t_OffscreenDep,
		std::shared_ptr<Fling::Shader> t_Vert,
		std::shared_ptr<Fling::Shader> t_Frag)
		: Subpass(t_Dev, t_Swap, t_Vert, t_Frag)
		, m_Camera(t_Cam)
		, m_OffscreenFrameBuf(t_OffscreenDep)
	{
		// Set clear values
		m_ClearValues.resize(2);
		m_ClearValues[0].color = { { 0.0f, 0.0f, 0.2f, 0.0f } };
		m_ClearValues[1].depthStencil = { 1.0f, 0 };

		m_QuadModel = Model::Quad();

		// Initializes the lighting UBO buffers  --------
		VkDeviceSize bufferSize = sizeof(m_LightingUBO);

		m_LightingUboBuffers.resize(m_SwapChain->GetImageCount());
		for (size_t i = 0; i < m_LightingUboBuffers.size(); i++)
		{
			m_LightingUboBuffers[i] = new Buffer(
				bufferSize,
				VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

			m_LightingUboBuffers[i]->MapMemory(bufferSize);
		}
	}

	GeometrySubpass::~GeometrySubpass()
	{
		// Clean up any allocated UBO's 
		for (Buffer* buf : m_LightingUboBuffers)
		{
			if (buf)
			{
				delete buf;
				buf = nullptr;
			}
		}
		m_LightingUboBuffers.clear();
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

		UpdateLightingUBO(t_reg, t_ActiveFrameInFlight);

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
		vkCmdDrawIndexed(t_CmdBuf.GetHandle(), m_QuadModel->GetIndexCount(), 1, 0, 0, 1);

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
					m_LightingUboBuffers[i],
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

	void GeometrySubpass::OnPointLightAdded(entt::entity t_Ent, entt::registry& t_Reg, PointLight& t_Light)
	{
		F_LOG_TRACE("Point Light added!");
		
		// Ensure that we have a transform component before adding a light
		if (!t_Reg.has<Transform>(t_Ent))
		{
			t_Reg.assign<Transform>(t_Ent);
		}

		Transform& t = t_Reg.get<Transform>(t_Ent);

#if FLING_DEBUG
		// Make a cute little debug mesh on a point light	
		t.SetScale(glm::vec3{ 0.1f });
		static std::string PointLightMesh = "Models/sphere.obj";
		if (!t_Reg.has<MeshRenderer>(t_Ent))
		{
			t_Reg.assign<MeshRenderer>(t_Ent, PointLightMesh);
		}

		// Ensure that we have the proper point light mesh on for a nice little gizmo
		MeshRenderer& m = t_Reg.get<MeshRenderer>(t_Ent);
		m.LoadModelFromPath(PointLightMesh);
		m.LoadMaterialFromPath("Materials/Default.mat");

#endif	// FLING_DEBUG
	}

	void GeometrySubpass::UpdateLightingUBO(entt::registry& t_Reg, UINT32 t_ActiveFrame)
	{
		auto PointLightView = t_Reg.view<PointLight, Transform>();
		auto DirectionalLightView = t_Reg.view<DirectionalLight>();

		UINT32 CurLightCount = 0;
		// Directional Lights ----------------
		for (auto entity : DirectionalLightView)
		{
			if (CurLightCount < DeferredLightSettings::MaxDirectionalLights)
			{
				DirectionalLight& Light = DirectionalLightView.get(entity);
				// Copy the dir light info to the buffer
				size_t size = sizeof(DirectionalLight);
				memcpy((m_LightingUBO.DirLightBuffer + (CurLightCount++)), &Light, size);
			}
		}

		m_LightingUBO.DirLightCount = CurLightCount;

		CurLightCount = 0;

		// Point lights ---------------------
		for (auto entity : PointLightView)
		{
			if (CurLightCount < DeferredLightSettings::MaxPointLights)
			{
				PointLight& Light = PointLightView.get<PointLight>(entity);
				Transform& Trans = PointLightView.get<Transform>(entity);

				Light.SetPos(glm::vec4(Trans.GetPos(), 1.0f));
				// Copy the point light info to the buffer
				size_t size = sizeof(PointLight);
				memcpy((m_LightingUBO.DirLightBuffer + (CurLightCount++)), &Light, size);
			}
		}

		// Memcpy to the buffer
		m_LightingUBO.PointLightCount = CurLightCount;
		m_LightingUBO.ViewPos = 
			glm::vec4(m_Camera->GetPosition(), 0.0f) * glm::vec4(-1.0f, 1.0f, -1.0f, 1.0f);
		
		memcpy(
			m_LightingUboBuffers[t_ActiveFrame]->m_MappedMem,
			&m_LightingUBO,
			sizeof(m_LightingUBO));
	}
}   // namespace Fling