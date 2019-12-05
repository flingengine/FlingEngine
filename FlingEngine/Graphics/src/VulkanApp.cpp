#include "VulkanApp.h"
#include "RenderPipeline.h"

#include "GeometrySubpass.h"
#include "OffscreenSubpass.h"

#include "CommandBuffer.h"
#include "Instance.h"
#include "LogicalDevice.h"
#include "PhyscialDevice.h"
#include "SwapChain.h"
#include "FlingWindow.h"
#include "FlingConfig.h"
#include "FirstPersonCamera.h"
#include "GraphicsHelpers.h"
#include "DepthBuffer.h"

namespace Fling
{
	const INT32 VulkanApp::MAX_FRAMES_IN_FLIGHT = 2;

	void VulkanApp::Init(PipelineFlags t_Conf, entt::registry& t_Reg)
	{
		Singleton<VulkanApp>::Init();

		Prepare();

		// #TODO Build VMA allocator

		BuildRenderPipelines(t_Conf, t_Reg);
	}

	CommandBuffer* VulkanApp::RequestCommandBuffer()
	{
		return new CommandBuffer(m_LogicalDevice, m_CommandPool);
	}

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

		CreateFrameSyncResources();

		// Create the camera
		float CamMoveSpeed = FlingConfig::GetFloat("Camera", "MoveSpeed", 10.0f);
		float CamRotSpeed = FlingConfig::GetFloat("Camera", "RotationSpeed", 40.0f);
		m_Camera = new FirstPersonCamera(m_CurrentWindow->GetAspectRatio(), CamMoveSpeed, CamRotSpeed);

		BuildSwapChainResources();
	}


	void VulkanApp::BuildSwapChainResources()
	{
		// Build command buffers (one for each swap chain image)
		for (size_t i = 0; i < m_SwapChain->GetImageViewCount(); ++i)
		{
			m_DrawCmdBuffers.emplace_back(new CommandBuffer(m_LogicalDevice, m_CommandPool));
		}

		// Build the depth stencil
		m_DepthBuffer = new DepthBuffer(m_LogicalDevice, VK_SAMPLE_COUNT_1_BIT, m_SwapChain->GetExtents());
		assert(m_DepthBuffer);

		BuildGlobalRenderPass();

		BuildSwapChainFrameBuffer();
	}

	void VulkanApp::BuildGlobalRenderPass()
	{
		assert(m_SwapChain);

		//std::array<VkAttachmentDescription, 2> attachments = {};
		//// Color attachment
		//attachments[0].format = m_SwapChain->GetImageFormat();
		//attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
		//attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		//attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		//attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		//attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		//attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		//attachments[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		//// Depth attachment
		//attachments[1].format = DepthBuffer::GetDepthBufferFormat();
		//attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
		//attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		//attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		//attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		//attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		//attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		//attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		//VkAttachmentReference colorReference = {};
		//colorReference.attachment = 0;
		//colorReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		//VkAttachmentReference depthReference = {};
		//depthReference.attachment = 1;
		//depthReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		//VkSubpassDescription subpassDescription = {};
		//subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		//subpassDescription.colorAttachmentCount = 1;
		//subpassDescription.pColorAttachments = &colorReference;
		//subpassDescription.pDepthStencilAttachment = &depthReference;
		//subpassDescription.inputAttachmentCount = 0;
		//subpassDescription.pInputAttachments = nullptr;
		//subpassDescription.preserveAttachmentCount = 0;
		//subpassDescription.pPreserveAttachments = nullptr;
		//subpassDescription.pResolveAttachments = nullptr;

		//// Subpass dependencies for layout transitions
		//std::array<VkSubpassDependency, 2> dependencies;

		//dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
		//dependencies[0].dstSubpass = 0;
		//dependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
		//dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		//dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
		//dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		//dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

		//dependencies[1].srcSubpass = 0;
		//dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
		//dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		//dependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
		//dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		//dependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
		//dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

		//VkRenderPassCreateInfo renderPassInfo = {};
		//renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		//renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
		//renderPassInfo.pAttachments = attachments.data();
		//renderPassInfo.subpassCount = 1;
		//renderPassInfo.pSubpasses = &subpassDescription;
		//renderPassInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
		//renderPassInfo.pDependencies = dependencies.data();

		//VK_CHECK_RESULT(vkCreateRenderPass(m_LogicalDevice->GetVkDevice(), &renderPassInfo, nullptr, &m_RenderPass));	



		VkAttachmentDescription colorAttachment = {};
		colorAttachment.format = m_SwapChain->GetImageFormat();
		colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
		colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

		VkAttachmentDescription depthAttachment = {};
		depthAttachment.format = DepthBuffer::GetDepthBufferFormat();
		depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
		depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		VkAttachmentReference colorAttachmentRef = {};
		colorAttachmentRef.attachment = 0;
		colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VkAttachmentReference depthAttachmentRef = {};
		depthAttachmentRef.attachment = 1;
		depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		VkSubpassDescription subpass = {};
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &colorAttachmentRef;
		subpass.pDepthStencilAttachment = &depthAttachmentRef;

		VkSubpassDependency dependency = {};
		dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
		dependency.dstSubpass = 0;
		dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependency.srcAccessMask = 0;
		dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

		std::array<VkAttachmentDescription, 2> attachments = { colorAttachment, depthAttachment };
		VkRenderPassCreateInfo renderPassInfo = {};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
		renderPassInfo.pAttachments = attachments.data();
		renderPassInfo.subpassCount = 1;
		renderPassInfo.pSubpasses = &subpass;
		renderPassInfo.dependencyCount = 1;
		renderPassInfo.pDependencies = &dependency;

		VK_CHECK_RESULT(vkCreateRenderPass(m_LogicalDevice->GetVkDevice(), &renderPassInfo, nullptr, &m_RenderPass));

	}

	void VulkanApp::BuildSwapChainFrameBuffer()
	{
		assert(m_DepthBuffer);

		const std::vector<VkImageView>& ImageViews = m_SwapChain->GetImageViews();

		// Create frame buffers for every swap chain image
		m_SwapChainFrameBuffers.resize(m_SwapChain->GetImageCount());
		for (uint32_t i = 0; i < m_SwapChainFrameBuffers.size(); i++)
		{
			VkImageView attachments[2];

			// Depth/Stencil attachment is the same for all frame buffers
			attachments[1] = m_DepthBuffer->GetVkImageView();

			VkFramebufferCreateInfo frameBufferCreateInfo = {};
			frameBufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			frameBufferCreateInfo.pNext = nullptr;
			frameBufferCreateInfo.renderPass = m_RenderPass;
			frameBufferCreateInfo.attachmentCount = 2;
			frameBufferCreateInfo.pAttachments = attachments;
			frameBufferCreateInfo.width = m_SwapChain->GetExtents().width;
			frameBufferCreateInfo.height = m_SwapChain->GetExtents().height;
			frameBufferCreateInfo.layers = 1;

			attachments[0] = ImageViews[i];
			VK_CHECK_RESULT(vkCreateFramebuffer(m_LogicalDevice->GetVkDevice(), &frameBufferCreateInfo, nullptr, &m_SwapChainFrameBuffers[i]));
		}
	}

	void VulkanApp::CreateFrameSyncResources()
	{
		assert(m_LogicalDevice);

		m_PresentCompleteSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
		m_RenderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
		m_InFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

		VkFenceCreateInfo fenceInfo = {};
		fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

		for (INT32 i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
		{
			m_PresentCompleteSemaphores[i] = GraphicsHelpers::CreateSemaphore(m_LogicalDevice->GetVkDevice());
			m_RenderFinishedSemaphores[i] = GraphicsHelpers::CreateSemaphore(m_LogicalDevice->GetVkDevice());
			if (vkCreateFence(m_LogicalDevice->GetVkDevice(), &fenceInfo, nullptr, &m_InFlightFences[i]) != VK_SUCCESS)
			{
				F_LOG_FATAL("Failed to create fence!");
			}
		}
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

			// Offscreen pipeline ------
			// These shaders have vertex input and fill in the buffers that the final pass uses
			std::shared_ptr<Fling::Shader> OffscreenVert = Shader::Create(HS("Shaders/Deferred/mrt_vert.spv"), m_LogicalDevice);
			std::shared_ptr<Fling::Shader> OffscreenFrag = Shader::Create(HS("Shaders/Deferred/mrt_frag.spv"), m_LogicalDevice);
			Subpasses.emplace_back(std::make_unique<OffscreenSubpass>(m_LogicalDevice, m_SwapChain, t_Reg, OffscreenVert, OffscreenFrag));

			// Create geometry pass ------
			// These shaders do not have any vertex input and do the final processing to the screen
			OffscreenSubpass* Offscreen = static_cast<OffscreenSubpass*>(Subpasses[0].get());
			assert(Offscreen);
			FrameBuffer* OffscreenBuf = Offscreen->GetOffscreenFrameBuffer();
			assert(OffscreenBuf);
			std::shared_ptr<Fling::Shader> GeomVert = Shader::Create(HS("Shaders/Deferred/deferred_vert.spv"), m_LogicalDevice);
			std::shared_ptr<Fling::Shader> GeomFrag = Shader::Create(HS("Shaders/Deferred/deferred_frag.spv"), m_LogicalDevice);
			Subpasses.emplace_back(std::make_unique<GeometrySubpass>(m_LogicalDevice, m_SwapChain, t_Reg,  OffscreenBuf, GeomVert, GeomFrag));

			m_RenderPipelines.emplace_back(
				new Fling::RenderPipeline(t_Reg, m_LogicalDevice, m_SwapChain, Subpasses)
			);
		}

		if (t_Conf & PipelineFlags::REFLECTIONS)
		{
			F_LOG_WARN("Bulid REFLECTIONS render pipeline! (NOT YET IMPL)");
		}

		// #TODO Debug config (Unlit, wireframe, etc) 

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
		// Prepare the frame for submission by waiting for the swap chain
		m_CurrentWindow->Update();
		m_Camera->Update(DeltaTime);

		// Aquire the active image index
		VkResult iResult = m_SwapChain->AquireNextImage(m_PresentCompleteSemaphores[CurrentFrameIndex]);
		UINT32  ImageIndex = m_SwapChain->GetActiveImageIndex();

		vkResetFences(m_LogicalDevice->GetVkDevice(), 1, &m_InFlightFences[CurrentFrameIndex]);

		if (iResult == VK_ERROR_OUT_OF_DATE_KHR)
		{
			F_LOG_WARN("Swap chain out of date! ");
			return;
		}
		else if (iResult != VK_SUCCESS && iResult != VK_SUBOPTIMAL_KHR)
		{
			F_LOG_FATAL("Failed to acquire swap chain image!");
		}

		// Build all the command buffer for the swap chain
		//for(size_t i = 0; i < m_DrawCmdBuffers.size(); ++i)
		{
			// Get the current drawing command buffer associated with the current swap chain image
			assert(m_DrawCmdBuffers[ImageIndex]);

			for (RenderPipeline* Pipeline : m_RenderPipelines)
			{
				Pipeline->Draw(*m_DrawCmdBuffers[ImageIndex], ImageIndex, t_Reg);
			}
		}

		// Wait for the color attachment to be done 
		VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

		std::vector<VkSemaphore> SemaphoresToWaitOn;

		// For each render pass
			// Submit it
			


		// Submit sub pass command buffers with their waits
		VkSubmitInfo OffscreenSubmission = {};
		OffscreenSubmission.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		OffscreenSubmission.pWaitDstStageMask = waitStages;
		// Wait on Present complete
		OffscreenSubmission.pWaitSemaphores = &m_PresentCompleteSemaphores[CurrentFrameIndex];
		OffscreenSubmission.commandBufferCount = 1;
		//OffscreenSubmission.pCommandBuffers = 
		// Offscreen command buffer pointer

		// Signal offscreen semaphore when this is completed


		// Submit any PIPELINE command buffers for work		
		VkSubmitInfo FinalScreenSubmitInfo = {};
		FinalScreenSubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

		// Then for the final submission...
		for (RenderPipeline* pipe : m_RenderPipelines)
		{
			// Give it a ref to the semaphores to wait on
				// Inside the render pipeline, loop through all the subpass
					// Let a subpass add to that vector of dependent semaphores
		}
		// Wait for offscreen or any other semaphore from a render pipeline

		// Signal render complete

		// Track any semaphores that we may need to wait on for the render pipeline
		FinalScreenSubmitInfo.waitSemaphoreCount = 1;
		FinalScreenSubmitInfo.pWaitSemaphores = &m_PresentCompleteSemaphores[CurrentFrameIndex];

		FinalScreenSubmitInfo.pWaitDstStageMask = waitStages;

		// Mark the draw command buffer at this frame for submission
		std::vector<VkCommandBuffer> submitCommandBuffers = {};
		submitCommandBuffers.emplace_back(m_DrawCmdBuffers[ImageIndex]->GetHandle());

		// Actually present the swap chain queue. This is always going to be the signal for the final semaphore
		FinalScreenSubmitInfo.signalSemaphoreCount = 1;
		FinalScreenSubmitInfo.pSignalSemaphores = &m_RenderFinishedSemaphores[CurrentFrameIndex];

		VK_CHECK_RESULT(vkQueueSubmit(m_LogicalDevice->GetGraphicsQueue(), 1, &FinalScreenSubmitInfo, m_InFlightFences[CurrentFrameIndex]));
		
		// Finish up the frame by setting the in flight fences to wait -----
		vkWaitForFences(m_LogicalDevice->GetVkDevice(), 1, &m_InFlightFences[CurrentFrameIndex], VK_TRUE, std::numeric_limits<uint64_t>::max());
	
		// Present the swap chain with the renderer finished semaphore
		iResult = m_SwapChain->QueuePresent(m_LogicalDevice->GetPresentQueue(), m_RenderFinishedSemaphores[CurrentFrameIndex]);
		
		// Check if the swap chain is out of date and needs to be rebuilt
		if (iResult == VK_ERROR_OUT_OF_DATE_KHR || iResult == VK_SUBOPTIMAL_KHR)
		{
			// #TODO If result is out of date then signal for resize
		}
		else if (iResult != VK_SUCCESS)
		{
			F_LOG_FATAL("Failed to present swap chain image!");
		}

		// Update the current in flight frame index!
		CurrentFrameIndex = (CurrentFrameIndex + 1) % MAX_FRAMES_IN_FLIGHT;
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

	void VulkanApp::Shutdown()
	{
		// Cleanup render pipelines (created in BuildRenderPipelines) -----------------
		for (RenderPipeline* pipeline : m_RenderPipelines)
		{
			if (pipeline)
			{
				delete pipeline;
				pipeline = nullptr;
			}
		}
		m_RenderPipelines.clear();

		// Camera cleanup ------------
		if (m_Camera)
		{
			delete m_Camera;
			m_Camera = nullptr;
		}

		// Destroy swap chain (created in Prepare) -----------
		if (m_SwapChain)
		{
			delete m_SwapChain;
			m_SwapChain = nullptr;
		}

		// #TODO Cleanup VMA allocator -------------

		// Clean up Frame sync resources (created in CreateFrameSyncResources) --------------
		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
		{
			vkDestroySemaphore(m_LogicalDevice->GetVkDevice(), m_RenderFinishedSemaphores[i], nullptr);
			vkDestroySemaphore(m_LogicalDevice->GetVkDevice(), m_PresentCompleteSemaphores[i], nullptr);
			vkDestroyFence(m_LogicalDevice->GetVkDevice(), m_InFlightFences[i], nullptr);
		}

		// Clean up command buffers and command pool -------------
		for (CommandBuffer* CmdBuf : m_DrawCmdBuffers)
		{
			if (CmdBuf)
			{
				delete CmdBuf;
				CmdBuf = nullptr;
			}
		}
		m_DrawCmdBuffers.clear();

		vkDestroyCommandPool(m_LogicalDevice->GetVkDevice(), m_CommandPool, nullptr);

		// Clean up devices and surface (created in Prepare) --------------
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