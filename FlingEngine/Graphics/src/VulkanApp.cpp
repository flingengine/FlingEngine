#include "VulkanApp.h"
#include "GeometrySubpass.h"
#include "CommandBuffer.h"

namespace Fling
{
	void VulkanApp::Prepare(PipelineFlags t_Conf)
	{
		// Build devices

		// Build VMA allocator

		// Build surface (window)

		// Build swap chain

		BuildRenderPipelines(t_Conf);
	}

	void VulkanApp::BuildRenderPipelines(PipelineFlags t_Conf)
	{
		if (t_Conf & PipelineFlags::DEFERRED)
		{
			F_LOG_TRACE("Bulid DEFERRED render pipeline!");
			std::vector<std::unique_ptr<Subpass>> Subpasses = {};
			
			auto Vert = Shader::Create(HS("Shaders/Deferred/geometry_vert.spv"));
			auto Frag = Shader::Create(HS("Shaders/Deferred/geometry_frag.spv"));
			Subpasses.emplace_back(std::make_unique<GeometrySubpass>(Vert, Frag));
			// #TODO Create Lighting sub pass

			m_RenderPipelines.emplace_back(std::make_unique<Fling::RenderPipeline>(Subpasses));
		}

		if (t_Conf & PipelineFlags::REFLECTIONS)
		{
			F_LOG_WARN("Bulid REFLECTIONS render pipeline! (NOT YET IMPL)");
		}

		if (t_Conf & PipelineFlags::IMGUI)
		{
			F_LOG_WARN("Bulid IMGUI render pipeline! (NOT YET IMPL)");
		}
	}

	void VulkanApp::Update(float DeltaTime, entt::registry& t_Reg)
	{
		// Prepare frame 
			// Wait for the swap chain semaphore

		// Get a valid command buffer
		CommandBuffer CmdBuf = {};

		for (const auto& Pipeline : m_RenderPipelines)
		{
			Pipeline->Draw(CmdBuf, t_Reg);
		}

		// Submit frame
	}
	
	void VulkanApp::Shutdown()
	{
		// Cleanup VMA alloctator
		// Cleanup render pipelines
		// Cleanup swap chain
	}
}	// namespace Fling