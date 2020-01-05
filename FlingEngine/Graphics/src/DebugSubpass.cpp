#include "DebugSubpass.h"
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
	DebugSubpass::DebugSubpass(
		const LogicalDevice* t_Dev,
		const Swapchain* t_Swap,
		entt::registry& t_reg,
		VkRenderPass t_GlobalRenderPass,
		FirstPersonCamera* t_Cam,
		std::shared_ptr<Fling::Shader> t_Vert,
		std::shared_ptr<Fling::Shader> t_Frag)
		: Subpass(t_Dev, t_Swap, t_Vert, t_Frag)
		, m_GlobalRenderPass(t_GlobalRenderPass)
		, m_Camera(t_Cam)
	{
		t_reg.on_construct<MeshRenderer>().connect<&DebugSubpass::OnMeshRendererAdded>(*this);

		PrepareAttachments();
	}

	DebugSubpass::~DebugSubpass()
	{
		
	}

	void DebugSubpass::Draw(CommandBuffer& t_CmdBuf, VkFramebuffer t_PresentFrameBuf, UINT32 t_ActiveFrameInFlight, entt::registry& t_reg, float DeltaTime)
	{
		// For every mesh bind it's model and descriptor set info
		auto RenderGroup = t_reg.group<Transform>(entt::get<MeshRenderer, entt::tag<"Debug"_hs>>);

		// Invert the project value to match the proper coordinate space compared to OpenGL
		m_Ubo.Projection = m_Camera->GetProjectionMatrix();
		m_Ubo.Projection[1][1] *= -1.0f;
		VkDeviceSize offsets[1] = { 0 };

		RenderGroup.less([&](entt::entity ent, Transform& t_trans, MeshRenderer& t_MeshRend)
		{
			Fling::Model* Model = t_MeshRend.m_Model;
			if (!Model)
			{
				return;
			}

			// Update the UBO
			Transform::CalculateWorldMatrix(t_trans);
			m_Ubo.Model = t_trans.GetWorldMatrix();

			// Memcpy to the buffer
			Buffer* buf = t_MeshRend.m_UniformBuffer;
			memcpy(
				buf->m_MappedMem,
				&m_Ubo,
				buf->GetSize()
			);

			// If the mesh has no descriptor sets, then build them
			// #TODO Investigate a better way to do this, probably by just moving the 
			// descriptors off of the mesh
			if (t_MeshRend.m_DescriptorSet == VK_NULL_HANDLE)
			{
				CreateMeshDescriptorSet(t_MeshRend);
			}

			// Bind the descriptor set for rendering a mesh using the dynamic offset
			vkCmdBindDescriptorSets(
				t_CmdBuf.GetHandle(),
				VK_PIPELINE_BIND_POINT_GRAPHICS,
				m_GraphicsPipeline->GetPipelineLayout(),
				0,
				1,
				&t_MeshRend.m_DescriptorSet,
				0,
				nullptr);

			vkCmdBindPipeline(t_CmdBuf.GetHandle(), VK_PIPELINE_BIND_POINT_GRAPHICS, m_GraphicsPipeline->GetPipeline());

			VkBuffer vertexBuffers[1] = { Model->GetVertexBuffer()->GetVkBuffer() };
			// Render the mesh
			vkCmdBindVertexBuffers(t_CmdBuf.GetHandle(), 0, 1, vertexBuffers, offsets);
			vkCmdBindIndexBuffer(t_CmdBuf.GetHandle(), Model->GetIndexBuffer()->GetVkBuffer(), 0, Model->GetIndexType());
			vkCmdDrawIndexed(t_CmdBuf.GetHandle(), Model->GetIndexCount(), 1, 0, 0, 0);
		});
	}

	void DebugSubpass::CreateDescriptorSets(VkDescriptorPool t_Pool, entt::registry& t_reg)
	{
		
	}

	void DebugSubpass::CreateMeshDescriptorSet(MeshRenderer& t_MeshRend)
	{
		// Only allocate new descriptor sets if there are none
		// Some may exist if entt decides to re-use the component
		if (t_MeshRend.m_DescriptorSet == VK_NULL_HANDLE)
		{
			VkDescriptorSetLayout layout = m_GraphicsPipeline->GetDescriptorSetLayout();
			VkDescriptorSetAllocateInfo allocInfo = {};
			allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
			// If we have specified a specific pool then use that, otherwise use the one on the mesh
			allocInfo.descriptorPool = m_DescriptorPool;
			allocInfo.descriptorSetCount = 1;
			allocInfo.pSetLayouts = &layout;

			VK_CHECK_RESULT(vkAllocateDescriptorSets(m_Device->GetVkDevice(), &allocInfo, &t_MeshRend.m_DescriptorSet));
		}

		std::vector<VkWriteDescriptorSet> writeDescriptorSets =
		{
			// 0: UBO
			Initializers::WriteDescriptorSetUniform(
				t_MeshRend.m_UniformBuffer,
				t_MeshRend.m_DescriptorSet,
				0
			),
		};

		vkUpdateDescriptorSets(m_Device->GetVkDevice(), static_cast<UINT32>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr);
	}

	void DebugSubpass::PrepareAttachments()
	{
		// Create the descriptor pool for off screen things
		UINT32 DescriptorCount = 100 * m_SwapChain->GetImageViewCount();

		std::vector<VkDescriptorPoolSize> poolSizes =
		{
			Initializers::DescriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 		DescriptorCount),			
		};

		VkDescriptorPoolCreateInfo poolInfo = {};
		poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		poolInfo.poolSizeCount = static_cast<UINT32>(poolSizes.size());
		poolInfo.pPoolSizes = poolSizes.data();
		poolInfo.maxSets = to_u32(100 * m_SwapChain->GetImageViewCount());

		if (vkCreateDescriptorPool(m_Device->GetVkDevice(), &poolInfo, nullptr, &m_DescriptorPool) != VK_SUCCESS)
		{
			F_LOG_FATAL("Failed to create descriptor pool");
		}
	}

	void DebugSubpass::CreateGraphicsPipeline()
	{
		// Draw in wireframe mode
		m_GraphicsPipeline->m_RasterizationState =
			Initializers::PipelineRasterizationStateCreateInfo(
				VK_POLYGON_MODE_FILL,
				VK_CULL_MODE_BACK_BIT,
				VK_FRONT_FACE_COUNTER_CLOCKWISE
			);

		// Create it otherwise with defaults
		m_GraphicsPipeline->CreateGraphicsPipeline(m_GlobalRenderPass, nullptr);
	}

	void DebugSubpass::CleanUp(entt::registry& t_reg)
	{
		if (m_DescriptorPool != VK_NULL_HANDLE)
		{
			vkDestroyDescriptorPool(m_Device->GetVkDevice(), m_DescriptorPool, nullptr);
			m_DescriptorPool = VK_NULL_HANDLE;
		}
	}

	void DebugSubpass::OnMeshRendererAdded(entt::entity t_Ent, entt::registry& t_Reg, MeshRenderer& t_MeshRend)
	{
		// If this mesh renderer material is set to DEBUG, then do this
		if (t_MeshRend.m_Material && t_MeshRend.m_Material->GetType() != Material::Type::Debug)
		{
			return;
		}

		t_Reg.assign<entt::tag<"Debug"_hs >>(t_Ent);

		// Initialize and map the UBO of each mesh renderer
		if (t_MeshRend.m_UniformBuffer == nullptr)
		{
			VkDeviceSize bufferSize = sizeof(DebugUBO);
			t_MeshRend.m_UniformBuffer = new Buffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
			t_MeshRend.m_UniformBuffer->MapMemory(bufferSize);
		}

		CreateMeshDescriptorSet(t_MeshRend);
	}
}   // namespace Fling