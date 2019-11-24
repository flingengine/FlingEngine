#include "VulkanApp.h"
#include "RenderPipeline.h"
#include "GeometrySubpass.h"
#include "CommandBuffer.h"
#include "Instance.h"
#include "LogicalDevice.h"
#include "PhyscialDevice.h"
#include "SwapChain.h"
#include "FlingWindow.h"
#include "FlingConfig.h"
#include "GraphicsHelpers.h"

namespace Fling
{
	void VulkanApp::Prepare()
	{
		CreateGameWindow(
			FlingConfig::GetInt("Engine", "WindowWidth", FLING_DEFAULT_WINDOW_WIDTH),
			FlingConfig::GetInt("Engine", "WindowHeight", FLING_DEFAULT_WINDOW_WIDTH)
		);

		m_Instance = new Instance();
		assert(m_Instance);

		m_CurrentWindow->CreateSurface(m_Instance->GetRawVkInstance(), &m_Surface);

		m_PhysicalDevice = new PhysicalDevice(m_Instance);
		assert(m_PhysicalDevice);

		m_LogicalDevice = new LogicalDevice(m_Instance, m_PhysicalDevice, m_Surface);
		assert(m_LogicalDevice);

		m_SwapChain = new Swapchain(ChooseSwapExtent(), m_LogicalDevice, m_PhysicalDevice, m_Surface);
		assert(m_SwapChain);

		GraphicsHelpers::CreateCommandPool(&m_CommandPool, 0);
	}

	void VulkanApp::CreateGameWindow(const UINT32 t_width, const UINT32 t_height)
	{
		WindowProps Props = {};
		Props.m_Height = t_width;
		Props.m_Width = t_width;

		// Ensure the window width is valid
		if (t_width > 0 && t_width < 5000 && t_height > 0 && t_height < 5000)
		{
			Props.m_Height = t_width;
			Props.m_Height = t_height;
		}
		else
		{
			F_LOG_ERROR("Window Width of {} or height of {} is invalid! Using default values", t_width, t_height);
			Props.m_Height = FLING_DEFAULT_WINDOW_WIDTH;
			Props.m_Height = FLING_DEFAULT_WINDOW_HEIGHT;
		}

		// Get the window title
		std::string Title = FlingConfig::GetString("Engine", "WindowTitle");

		if (FlingConfig::GetBool("Engine", "DisplayVersionInfoInTitle", true))
		{
			Title += " // Version: " + Version::EngineVersion.ToString();
		}

		if (FlingConfig::GetBool("Engine", "DisplayBuildInfoInTitle", true))
		{
			Title += " // Built from " + (std::string)(GIT_BRANCH) + " @ " + GIT_COMMIT_HASH;
		}

		Props.m_Title = Title;

		m_CurrentWindow = FlingWindow::Create(Props);
	}

	void VulkanApp::BuildRenderPipelines(PipelineFlags t_Conf, entt::registry& t_Reg)
	{
		if (t_Conf & PipelineFlags::DEFERRED)
		{
			F_LOG_TRACE("Bulid DEFERRED render pipeline!");
			std::vector<std::unique_ptr<Subpass>> Subpasses = {};
			
			auto Vert = Shader::Create(HS("Shaders/Deferred/geometry_vert.spv"), m_LogicalDevice);
			auto Frag = Shader::Create(HS("Shaders/Deferred/geometry_frag.spv"), m_LogicalDevice);
			Subpasses.emplace_back(std::make_unique<GeometrySubpass>(m_LogicalDevice, Vert, Frag));
			// #TODO Create Lighting sub pass

			m_RenderPipelines.emplace_back(
				new Fling::RenderPipeline(t_Reg, m_LogicalDevice, m_SwapChain, Subpasses)
			);
		}

		if (t_Conf & PipelineFlags::REFLECTIONS)
		{
			F_LOG_WARN("Bulid REFLECTIONS render pipeline! (NOT YET IMPL)");
		}

		if (t_Conf & PipelineFlags::IMGUI)
		{
#if WITH_IMGUI
			F_LOG_WARN("Bulid IMGUI render pipeline! (NOT YET IMPL)");
#else
			F_LOG_ERROR("IMGUI requested but failed because the CMake flag is not set!");
#endif
		}
	}

	void VulkanApp::Update(float DeltaTime, entt::registry& t_Reg)
	{
		m_CurrentWindow->Update();

		// Get a valid command buffer
		CommandBuffer CmdBuf = { m_LogicalDevice, m_CommandPool };

		CmdBuf.Begin();

		VkViewport viewport{};
		viewport.width = static_cast<float>(m_SwapChain->GetExtents().width);
		viewport.height = static_cast<float>(m_SwapChain->GetExtents().height);
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;
		CmdBuf.SetViewport(0, { viewport });

		VkRect2D scissor{};
		scissor.extent = m_SwapChain->GetExtents();
		CmdBuf.SetScissor(0, { scissor });

		for (const auto& Pipeline : m_RenderPipelines)
		{
			Pipeline->Draw(CmdBuf, t_Reg);
		}

		// Submit frame
		CmdBuf.End();
	}
	
	VkExtent2D VulkanApp::ChooseSwapExtent()
	{
		assert(m_CurrentWindow);

		VkSurfaceCapabilitiesKHR t_Capabilies = {};
		vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_PhysicalDevice->GetVkPhysicalDevice(), m_Surface, &t_Capabilies);

		if (t_Capabilies.currentExtent.width != std::numeric_limits<UINT32>::max())
		{
			return t_Capabilies.currentExtent;
		}
		else
		{
			UINT32 width = m_CurrentWindow->GetWidth();
			UINT32 height = m_CurrentWindow->GetHeight();

			VkExtent2D actualExtent = { width, height };

			actualExtent.width = std::max(t_Capabilies.minImageExtent.width,
				std::min(t_Capabilies.maxImageExtent.width, actualExtent.width));
			actualExtent.height = std::max(t_Capabilies.minImageExtent.height,
				std::min(t_Capabilies.maxImageExtent.height, actualExtent.height));

			return actualExtent;
		}
	}

	void VulkanApp::Init(PipelineFlags t_Conf, entt::registry& t_Reg)
	{
		Prepare();

		// #TODO Build VMA allocator

		BuildRenderPipelines(t_Conf, t_Reg);
	}

	void VulkanApp::Shutdown()
	{
		// Cleanup VMA allocator
		// Cleanup render pipelines
		for (auto& pipeline : m_RenderPipelines)
		{
			if (pipeline)
			{
				delete pipeline;
				pipeline = nullptr;
			}
		}
		m_RenderPipelines.clear();

		if (m_SwapChain)
		{
			delete m_SwapChain;
			m_SwapChain = nullptr;
		}

		vkDestroyCommandPool(m_LogicalDevice->GetVkDevice(), m_CommandPool, nullptr);

		if (m_LogicalDevice)
		{
			delete m_LogicalDevice;
			m_LogicalDevice = nullptr;
		}

		vkDestroySurfaceKHR(m_Instance->GetRawVkInstance(), m_Surface, nullptr);

		if (m_PhysicalDevice)
		{
			delete m_PhysicalDevice;
			m_PhysicalDevice = nullptr;
		}

		if (m_Instance)
		{
			delete m_Instance;
			m_Instance = nullptr;
		}

		// Cleanup Window --------
		if (m_CurrentWindow)
		{
			delete m_CurrentWindow;
			m_CurrentWindow = nullptr;
		}
	}
}	// namespace Fling