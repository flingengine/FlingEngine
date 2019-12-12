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
		FirstPersonCamera* t_Cam,
		std::shared_ptr<Fling::Shader> t_Vert,
		std::shared_ptr<Fling::Shader> t_Frag)
		: Subpass(t_Dev, t_Swap, t_Vert, t_Frag)
		, m_Camera(t_Cam)
	{
		PrepareAttachments();
	}

	DebugSubpass::~DebugSubpass()
	{
		
	}

	void DebugSubpass::Draw(CommandBuffer& t_CmdBuf, VkFramebuffer t_PresentFrameBuf, UINT32 t_ActiveFrameInFlight, entt::registry& t_reg, float DeltaTime)
	{
		
	}

	void DebugSubpass::CreateDescriptorSets(VkDescriptorPool t_Pool, entt::registry& t_reg)
	{
		
	}

	void DebugSubpass::CreateMeshDescriptorSet(MeshRenderer& t_MeshRend, VkDescriptorPool t_Pool, FrameBuffer& t_FrameBuf)
	{
		
	}

	void DebugSubpass::PrepareAttachments()
	{
		
	}

	void DebugSubpass::CreateGraphicsPipeline()
	{
		
	}

	void DebugSubpass::CleanUp(entt::registry& t_reg)
	{
		
	}

	void DebugSubpass::OnMeshRendererAdded(entt::entity t_Ent, entt::registry& t_Reg, MeshRenderer& t_MeshRend)
	{
		
	}
}   // namespace Fling