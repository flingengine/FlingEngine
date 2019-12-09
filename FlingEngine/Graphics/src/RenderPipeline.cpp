#include "RenderPipeline.h"
#include "CommandBuffer.h"
#include "LogicalDevice.h"
#include "GraphicsHelpers.h"
#include "SwapChain.h"
#include "FrameBuffer.h"
#include "MeshRenderer.h"

namespace Fling
{
	RenderPipeline::RenderPipeline(entt::registry& t_Reg, LogicalDevice* t_Dev, Swapchain* t_Swap, std::vector<std::unique_ptr<Subpass>>& t_Subpasses)
		: m_Subpasses ( std::move(t_Subpasses) )
		, m_Device(t_Dev)
		, m_SwapChain(t_Swap)
	{
		assert(m_Device && m_SwapChain);

		// Create the graphics pipelines on each subpass now that we have render passes for them
		for (const std::unique_ptr<Subpass>& pass : m_Subpasses)
		{	
			pass->CreateGraphicsPipeline();	
		}
		F_LOG_TRACE("Render pipeline Graphics Pipelines created...");

		// Build Descriptor sets -------
		CreateDescriptors(t_Reg);

		F_LOG_TRACE("Render pipeline Creation done!");
	}

	RenderPipeline::~RenderPipeline()
	{
		assert(m_Device);

		vkDestroyDescriptorPool(m_Device->GetVkDevice(), m_DescriptorPool, nullptr);

		// Cleanup subpasses
		m_Subpasses.clear();
	}

	void RenderPipeline::Draw(CommandBuffer& t_CmdBuf, VkFramebuffer t_PresentFrameBuf, UINT32 t_ActiveFrameInFlight, entt::registry& t_Reg, float DeltaTime)
	{
		assert(!m_Subpasses.empty() && "Render pipeline should contain at least one sub-pass");

		for (size_t i = 0; i < m_Subpasses.size(); ++i)
		{
			// Build the subpasses for the active frame in flight	
			m_Subpasses[i]->Draw(
				t_CmdBuf, 
				t_PresentFrameBuf,
				t_ActiveFrameInFlight, 
				t_Reg,
				DeltaTime
			);
		}
	}

	void RenderPipeline::GatherPresentDependencies(std::vector<CommandBuffer*>& t_CmdBuffs, std::vector<VkSemaphore>& t_Deps, UINT32 t_ActiveFrameIndex)
	{
		for (const auto& subpass : m_Subpasses)
		{
			subpass->GatherPresentDependencies(t_CmdBuffs, t_Deps, t_ActiveFrameIndex);
		}
	}

	void RenderPipeline::GatherPresentBuffers(std::vector<CommandBuffer*>& t_CmdBuffs, UINT32 t_ActiveFrameIndex)
	{
		for (const auto& subpass : m_Subpasses)
		{
			subpass->GatherPresentBuffers(t_CmdBuffs, t_ActiveFrameIndex);
		}
	}

	void RenderPipeline::CleanUp(entt::registry& t_reg)
	{
		for (const auto& Sub : m_Subpasses)
		{
			Sub->CleanUp(t_reg);
		}
	}

	void RenderPipeline::CreateDescriptors(entt::registry& t_Reg)
	{
		// Create the descriptor pool for us to use -------
		const UINT32 SwapImageCount = static_cast<UINT32>(m_SwapChain->GetImageCount());
		UINT32 DescriptorCount = 1024;

		std::vector<VkDescriptorPoolSize> poolSizes =
		{
			Initializers::DescriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, DescriptorCount),
			Initializers::DescriptorPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, DescriptorCount),
			Initializers::DescriptorPoolSize(VK_DESCRIPTOR_TYPE_SAMPLER, DescriptorCount),
			Initializers::DescriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, DescriptorCount),
			Initializers::DescriptorPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, DescriptorCount)
		};

		VkDescriptorPoolCreateInfo poolInfo = {};
		poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		poolInfo.poolSizeCount = static_cast<UINT32>(poolSizes.size());
		poolInfo.pPoolSizes = poolSizes.data();
		poolInfo.maxSets = SwapImageCount;

		VK_CHECK_RESULT(vkCreateDescriptorPool(m_Device->GetVkDevice(), &poolInfo, nullptr, &m_DescriptorPool));

		// Build all the descriptor SETS in each subpass
		assert(!m_Subpasses.empty() && "Render pipeline should contain at least one sub-pass");

		for (std::unique_ptr<Subpass>& Pass : m_Subpasses)
		{	
			Pass->CreateDescriptorSets(m_DescriptorPool, t_Reg);
		}
	}
}