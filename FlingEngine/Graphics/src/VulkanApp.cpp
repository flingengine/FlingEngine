#include "VulkanApp.h"
#include "RenderPipeline.h"

#include "GeometrySubpass.h"
#include "OffscreenSubpass.h"
#include "ImGuiSubpass.h"
#include "DebugSubpass.h"

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
#include "BaseEditor.h"

namespace Fling
{
	void VulkanApp::Init(PipelineFlags t_Conf, entt::registry& t_Reg, std::shared_ptr<Fling::BaseEditor> t_Editor)
	{
		Singleton<VulkanApp>::Init();

		Prepare();

		// #TODO Build VMA allocator

		BuildRenderPipelines(t_Conf, t_Reg, t_Editor);

		F_LOG_TRACE("Vulkan App Init!");
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

		GraphicsHelpers::CreateCommandPool(&m_CommandPool, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

		CreateFrameSyncResources();

		// Create the camera
		float CamMoveSpeed = FlingConfig::GetFloat("Camera", "MoveSpeed", 10.0f);
		float CamRotSpeed = FlingConfig::GetFloat("Camera", "RotationSpeed", 40.0f);
		m_Camera = new FirstPersonCamera(m_CurrentWindow->GetAspectRatio(), CamMoveSpeed, CamRotSpeed);

		BuildSwapChainResources();
	}

	void VulkanApp::BuildSwapChainResources()
	{
		// Default clear values
		m_SwapChainClearVals[0].color = { 0.0f, 0.0f, 0.0f, 0.2F };
		m_SwapChainClearVals[1].depthStencil = { 1.0f, ~0U };

		// The command buffers should certainly be empty whenever we are creating them here
		// This is a sanity check for when we are recreating the swap chain
		assert(m_DrawCmdBuffers.size() == 0);

		// Build command buffers (one for each swap chain image)
		for (size_t i = 0; i < m_SwapChain->GetImageViewCount(); ++i)
		{
			m_DrawCmdBuffers.emplace_back(new CommandBuffer(m_LogicalDevice, m_CommandPool));
		}

		// Build the depth stencil
		// The depth buffer can be not-null when we are recreating the swap chain
		if(m_DepthBuffer == nullptr)
		{
			m_DepthBuffer = new DepthBuffer(m_LogicalDevice, VK_SAMPLE_COUNT_1_BIT, m_SwapChain->GetExtents());
		}
		else
		{
			m_DepthBuffer->SetExtents(m_SwapChain->GetExtents());
		}
		assert(m_DepthBuffer);

		BuildGlobalRenderPass();

		BuildSwapChainFrameBuffer();
	}	

	void VulkanApp::BuildGlobalRenderPass()
	{
		assert(m_SwapChain);

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
		dependency.srcStageMask = m_WaitStages;
		dependency.srcAccessMask = 0;
		dependency.dstStageMask = m_WaitStages;
		dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

		std::array<VkAttachmentDescription, 2> attachments = { colorAttachment, depthAttachment };
		VkRenderPassCreateInfo renderPassInfo = {};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		renderPassInfo.attachmentCount = static_cast<uint32>(attachments.size());
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
		for (uint32 i = 0; i < m_SwapChainFrameBuffers.size(); i++)
		{
			VkImageView attachments[2];

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

			// Depth/Stencil attachment is the same for all frame buffers
			attachments[1] = m_DepthBuffer->GetVkImageView();
			VK_CHECK_RESULT(vkCreateFramebuffer(m_LogicalDevice->GetVkDevice(), &frameBufferCreateInfo, nullptr, &m_SwapChainFrameBuffers[i]));
		}
	}

	void VulkanApp::CreateFrameSyncResources()
	{
		assert(m_LogicalDevice);

		m_PresentCompleteSemaphores.resize(VkConfig::MAX_FRAMES_IN_FLIGHT);
		m_RenderFinishedSemaphores.resize(VkConfig::MAX_FRAMES_IN_FLIGHT);
		m_InFlightFences.resize(VkConfig::MAX_FRAMES_IN_FLIGHT);

		VkFenceCreateInfo fenceInfo = {};
		fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

		for (int32 i = 0; i < VkConfig::MAX_FRAMES_IN_FLIGHT; i++)
		{
			m_PresentCompleteSemaphores[i] = GraphicsHelpers::CreateSemaphore(m_LogicalDevice->GetVkDevice());
			m_RenderFinishedSemaphores[i] = GraphicsHelpers::CreateSemaphore(m_LogicalDevice->GetVkDevice());
			if (vkCreateFence(m_LogicalDevice->GetVkDevice(), &fenceInfo, nullptr, &m_InFlightFences[i]) != VK_SUCCESS)
			{
				F_LOG_FATAL("Failed to create fence!");
			}
		}
	}

	void VulkanApp::CreateGameWindow(const uint32 t_width, const uint32 t_height)
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

	void VulkanApp::OnWindowResized(int Width, int Height)
	{
		bNeedsResizing = true;
	}

	void VulkanApp::RecreateFrameResourcesForResize(entt::registry& t_Reg)
	{
		F_LOG_TRACE("Resizing the window!");
		m_CurrentWindow->WaitForNewWindowSize();
		m_LogicalDevice->WaitForIdle();

		CleanupSwapChainResources();

		m_SwapChain->Recreate(ChooseSwapExtent());

		BuildSwapChainResources();

		for(RenderPipeline* Pipeline : m_RenderPipelines)
		{
			Pipeline->OnSwapchainResized(t_Reg);
		}
	}

	void VulkanApp::CleanupSwapChainResources()
	{
		// Global Render pass
		vkDestroyRenderPass(m_LogicalDevice->GetVkDevice(), m_RenderPass, nullptr);

		// Frame buffers
		for (size_t i = 0; i < m_SwapChainFrameBuffers.size(); i++)
		{
			vkDestroyFramebuffer(m_LogicalDevice->GetVkDevice(), m_SwapChainFrameBuffers[i], nullptr);
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

		m_SwapChain->Cleanup();
	}

	void VulkanApp::BuildRenderPipelines(PipelineFlags t_Conf, entt::registry& t_Reg, std::shared_ptr<Fling::BaseEditor> t_Editor)
	{
		if (t_Conf & PipelineFlags::DEFERRED)
		{
			F_LOG_TRACE("Bulid DEFERRED render pipeline!");
			std::vector<std::unique_ptr<Subpass>> Subpasses = {};

			// Offscreen pipeline ------
			// These shaders have vertex input and fill in the buffers that the final pass uses
			std::shared_ptr<Fling::Shader> OffscreenVert = Shader::Create(HS("Shaders/Deferred/mrt_vert.spv"), m_LogicalDevice);
			std::shared_ptr<Fling::Shader> OffscreenFrag = Shader::Create(HS("Shaders/Deferred/mrt_frag.spv"), m_LogicalDevice);
			Subpasses.emplace_back(std::make_unique<OffscreenSubpass>(m_LogicalDevice, m_SwapChain, t_Reg, m_Camera, OffscreenVert, OffscreenFrag));

			// Create geometry pass ------
			// These shaders do not have any vertex input and do the final processing to the screen
			OffscreenSubpass* Offscreen = static_cast<OffscreenSubpass*>(Subpasses[0].get());
			assert(Offscreen);
			FrameBuffer* OffscreenBuf = Offscreen->GetOffscreenFrameBuffer();
			assert(OffscreenBuf);
			std::shared_ptr<Fling::Shader> GeomVert = Shader::Create(HS("Shaders/Deferred/deferred_vert.spv"), m_LogicalDevice);
			std::shared_ptr<Fling::Shader> GeomFrag = Shader::Create(HS("Shaders/Deferred/deferred_frag.spv"), m_LogicalDevice);
			Subpasses.emplace_back(std::make_unique<GeometrySubpass>(m_LogicalDevice, m_SwapChain, t_Reg, m_RenderPass, m_Camera, OffscreenBuf, GeomVert, GeomFrag));

			m_RenderPipelines.emplace_back(
				new Fling::RenderPipeline(t_Reg, m_LogicalDevice, m_SwapChain, Subpasses)
			);
		}

		if (t_Conf & PipelineFlags::REFLECTIONS)
		{
			F_LOG_WARN("Bulid REFLECTIONS render pipeline! (NOT YET IMPL)");
		}

		if(t_Conf & PipelineFlags::CUBEMAP)
		{
			F_LOG_WARN("Build CUBEMAP render pipeline!");
			// Add a cubemap
		}

		/*if (t_Conf & PipelineFlags::DEBUG)
		{
			F_LOG_WARN("Build DEBUG render pipeline!");
			std::vector<std::unique_ptr<Subpass>> Subpasses = {};

			std::shared_ptr<Fling::Shader> DebugVert = Shader::Create(HS("Shaders/Debug/debug_vert.spv"), m_LogicalDevice);
			std::shared_ptr<Fling::Shader> DebugFrag = Shader::Create(HS("Shaders/Debug/debug_frag.spv"), m_LogicalDevice);
			Subpasses.emplace_back(std::make_unique<DebugSubpass>(m_LogicalDevice, m_SwapChain, t_Reg, m_RenderPass, m_Camera, DebugVert, DebugFrag));

			m_RenderPipelines.emplace_back(
				new Fling::RenderPipeline(t_Reg, m_LogicalDevice, m_SwapChain, Subpasses)
			);
		}*/

		if (t_Conf & PipelineFlags::IMGUI)
		{
#if WITH_IMGUI
			F_LOG_TRACE("Bulid IMGUI render pipeline! ");
			std::vector<std::unique_ptr<Subpass>> Subpasses = {};
			std::shared_ptr<Fling::Shader> ImGuiVert = Shader::Create(HS("Shaders/imgui/ui.vert.spv"), m_LogicalDevice);
			std::shared_ptr<Fling::Shader> ImGuiFrag = Shader::Create(HS("Shaders/imgui/ui.frag.spv"), m_LogicalDevice);
			Subpasses.emplace_back(std::make_unique<ImGuiSubpass>(
				m_LogicalDevice, m_SwapChain, t_Reg, m_CurrentWindow, m_RenderPass, t_Editor, ImGuiVert, ImGuiFrag)
			);

			m_RenderPipelines.emplace_back(
				new Fling::RenderPipeline(t_Reg, m_LogicalDevice, m_SwapChain, Subpasses)
			);
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
		uint32  ImageIndex = m_SwapChain->GetActiveImageIndex();

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

		// Fill this with the render pipelines
		std::vector<VkSemaphore> SemaphoresToWaitOn = {};
		std::vector<CommandBuffer*> DependentCmdBufs = {};

		// Vector of command buffers to be sent out with the final swap chain presentation
		// the swap chain draw buffer is always first
		std::vector<CommandBuffer*> FinalSubmissionBufs = {};
		FinalSubmissionBufs.emplace_back(m_DrawCmdBuffers[ImageIndex]);

		//vkResetCommandPool(m_LogicalDevice->GetVkDevice(), m_CommandPool, 0);

		{
			// Get the current drawing command buffer associated with the current swap chain image
			CommandBuffer* CmdBuf = m_DrawCmdBuffers[ImageIndex];
			VkFramebuffer FrameBuf = m_SwapChainFrameBuffers[ImageIndex];
			assert(CmdBuf && FrameBuf != VK_NULL_HANDLE);

			CmdBuf->Begin();

			// Start a render pass using the global render pass settings
			VkRenderPassBeginInfo renderPassBeginInfo = Initializers::RenderPassBeginInfo();
			renderPassBeginInfo.renderPass = m_RenderPass;
			renderPassBeginInfo.framebuffer = FrameBuf;

			renderPassBeginInfo.renderArea.offset = { 0, 0 };
			renderPassBeginInfo.renderArea.extent = m_SwapChain->GetExtents();

			renderPassBeginInfo.clearValueCount = m_SwapChainClearVals.size();
			renderPassBeginInfo.pClearValues = m_SwapChainClearVals.data();

			vkCmdBeginRenderPass(CmdBuf->GetHandle(), &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

			VkExtent2D swapExtents = m_SwapChain->GetExtents();

			VkViewport viewport = Initializers::Viewport(static_cast<float>(swapExtents.width), static_cast<float>(swapExtents.height), 0.0f, 1.0f);

			VkRect2D scissor = Initializers::Rect2D(swapExtents.width, swapExtents.height, /** offsetX */ 0, /** offsetY */ 0);

			CmdBuf->SetViewport(0, { viewport });
			CmdBuf->SetScissor(0, { scissor });

			// Build the command buffers of the render pipelines
			for (RenderPipeline* Pipeline : m_RenderPipelines)
			{		
				Pipeline->Draw(*CmdBuf, FrameBuf, ImageIndex, t_Reg, DeltaTime);
			}

			CmdBuf->EndRenderPass();

			// End command buffer recording
			CmdBuf->End();
		}

		// Gather deps for the current frame only
		for (RenderPipeline* Pipeline : m_RenderPipelines)
		{
			// Gather the dependencies 
			Pipeline->GatherPresentDependencies(DependentCmdBufs, SemaphoresToWaitOn, ImageIndex, CurrentFrameIndex);
			Pipeline->GatherPresentBuffers(FinalSubmissionBufs, ImageIndex);
		}

		// Wait for the color attachment to be done 
		VkPipelineStageFlags waitStages[] = { m_WaitStages };

		// Submit any PIPELINE command buffers for work		
		VkSubmitInfo FinalScreenSubmitInfo = {};
		FinalScreenSubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		FinalScreenSubmitInfo.pWaitDstStageMask = waitStages;

		// If There are dependent semaphores, then wait for them
		// otherwise wait for the present 
		if (!SemaphoresToWaitOn.empty() && !DependentCmdBufs.empty())
		{
			// Submit sub pass command buffers with their waits
			VkSubmitInfo OffscreenSubmission = {};
			OffscreenSubmission.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
			OffscreenSubmission.pWaitDstStageMask = waitStages;
			// Wait on Present complete
			OffscreenSubmission.pWaitSemaphores = &m_PresentCompleteSemaphores[CurrentFrameIndex];
			OffscreenSubmission.waitSemaphoreCount = 1;

			// Signal that the dependent semaphores are done when this is complete
			OffscreenSubmission.pSignalSemaphores = SemaphoresToWaitOn.data();
			OffscreenSubmission.signalSemaphoreCount = (uint32)SemaphoresToWaitOn.size();

			// Mark the draw command buffer at this frame for submission
			std::vector<VkCommandBuffer> submitCommandBuffers = {};
			
			for (CommandBuffer* Buf : DependentCmdBufs)
			{
				submitCommandBuffers.emplace_back(Buf->GetHandle());
			}

			OffscreenSubmission.pCommandBuffers = submitCommandBuffers.data();
			OffscreenSubmission.commandBufferCount = (uint32)submitCommandBuffers.size();
			// Signal off screen semaphore when this is completed
			VK_CHECK_RESULT(vkQueueSubmit(m_LogicalDevice->GetGraphicsQueue(), 1, &OffscreenSubmission, VK_NULL_HANDLE));

			// Wait for dependent semaphores
			FinalScreenSubmitInfo.waitSemaphoreCount = (uint32)SemaphoresToWaitOn.size();
			FinalScreenSubmitInfo.pWaitSemaphores = SemaphoresToWaitOn.data();
		}
		else
		{
			// Track any semaphores that we may need to wait on for the render pipeline
			FinalScreenSubmitInfo.waitSemaphoreCount = 1;
			FinalScreenSubmitInfo.pWaitSemaphores = &m_PresentCompleteSemaphores[CurrentFrameIndex];
		}
		
		// Collect any addition command buffers that we want to submit, but are not dependent on offscreen
		std::vector<VkCommandBuffer> submitCommandBuffers = {};
		for (CommandBuffer* buf : FinalSubmissionBufs)
		{
			submitCommandBuffers.emplace_back(buf->GetHandle());
		}

		FinalScreenSubmitInfo.pCommandBuffers = submitCommandBuffers.data();
		FinalScreenSubmitInfo.commandBufferCount = (uint32)submitCommandBuffers.size();

		// Actually present the swap chain queue. This is always going to be the signal for the final semaphore
		FinalScreenSubmitInfo.signalSemaphoreCount = 1;
		FinalScreenSubmitInfo.pSignalSemaphores = &m_RenderFinishedSemaphores[CurrentFrameIndex];

		VK_CHECK_RESULT(vkQueueSubmit(m_LogicalDevice->GetGraphicsQueue(), 1, &FinalScreenSubmitInfo, m_InFlightFences[CurrentFrameIndex]));
		
		// Finish up the frame by setting the in flight fences to wait -----
		vkWaitForFences(m_LogicalDevice->GetVkDevice(), 1, &m_InFlightFences[CurrentFrameIndex], VK_TRUE, std::numeric_limits<uint64_t>::max());
	
		// Present the swap chain with the renderer finished semaphore
		iResult = m_SwapChain->QueuePresent(m_LogicalDevice->GetPresentQueue(), m_RenderFinishedSemaphores[CurrentFrameIndex]);
		
		// Check if the swap chain is out of date and needs to be rebuilt
		if (iResult == VK_ERROR_OUT_OF_DATE_KHR || iResult == VK_SUBOPTIMAL_KHR || bNeedsResizing)
		{
			// If result is out of date then signal for resize
			bNeedsResizing = false;
			RecreateFrameResourcesForResize(t_Reg);
		}
		else if (iResult != VK_SUCCESS)
		{
			F_LOG_FATAL("Failed to present swap chain image!");
		}

		// Update the current in flight frame index!
		CurrentFrameIndex = (CurrentFrameIndex + 1) % VkConfig::MAX_FRAMES_IN_FLIGHT;
	}
	
	VkExtent2D VulkanApp::ChooseSwapExtent()
	{
		assert(m_CurrentWindow);

		VkSurfaceCapabilitiesKHR t_Capabilies = {};
		vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_PhysicalDevice->GetVkPhysicalDevice(), m_Surface, &t_Capabilies);

		if (t_Capabilies.currentExtent.width != std::numeric_limits<uint32>::max())
		{
			return t_Capabilies.currentExtent;
		}
		else
		{
			uint32 width = m_CurrentWindow->GetWidth();
			uint32 height = m_CurrentWindow->GetHeight();

			VkExtent2D actualExtent = { width, height };

			actualExtent.width = std::max(t_Capabilies.minImageExtent.width,
				std::min(t_Capabilies.maxImageExtent.width, actualExtent.width));
			actualExtent.height = std::max(t_Capabilies.minImageExtent.height,
				std::min(t_Capabilies.maxImageExtent.height, actualExtent.height));

			return actualExtent;
		}
	}

	void VulkanApp::Shutdown(entt::registry& t_Reg)
	{
		Singleton<VulkanApp>::Shutdown();

		// Wait for the device to be ready before shutting down
		m_LogicalDevice->WaitForIdle();

		// Cleanup render pipelines (created in BuildRenderPipelines) -----------------
		for (RenderPipeline* pipeline : m_RenderPipelines)
		{
			if (pipeline)
			{
				pipeline->CleanUp(t_Reg);
				delete pipeline;
				pipeline = nullptr;
			}
		}
		m_RenderPipelines.clear();

		for (size_t i = 0; i < m_SwapChainFrameBuffers.size(); i++)
		{
			vkDestroyFramebuffer(m_LogicalDevice->GetVkDevice(), m_SwapChainFrameBuffers[i], nullptr);
		}

		vkDestroyRenderPass(m_LogicalDevice->GetVkDevice(), m_RenderPass, nullptr);

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

		if (m_DepthBuffer)
		{
			delete m_DepthBuffer;
			m_DepthBuffer = nullptr;
		}

		// #TODO Cleanup VMA allocator -------------

		// Clean up Frame sync resources (created in CreateFrameSyncResources) --------------
		for (size_t i = 0; i < VkConfig::MAX_FRAMES_IN_FLIGHT; i++)
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

		F_LOG_TRACE("Vulkan App Shutdown!");
	}
}	// namespace Fling