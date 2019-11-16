#include "pch.h"
#include "Renderer.h"
#include "ResourceManager.h"
#include "FlingConfig.h"
#include "File.h"
#include "Image.h"
#include "GraphicsHelpers.h"
#include "Components/Transform.h"
#include <random>
#include "CircularBuffer.hpp"

namespace Fling
{
    const int Renderer::MAX_FRAMES_IN_FLIGHT = 2;
	
    void Renderer::Init()
	{
		// You must have the registry set before creating a renderer!
        InitDevices();

        assert(m_Registry);
        ShaderProgramManager::Get().m_Registry = m_Registry;
        ShaderProgramManager::Get().Init();

        // Add entt component callbacks for mesh render etc
        InitComponentData();

		// Initalize all graphics resources
		InitGraphics();
    }

    void Renderer::InitDevices()
    {
        m_Instance = new Instance();
        assert(m_Instance);

        m_CurrentWindow->CreateSurface(m_Instance->GetRawVkInstance(), &m_Surface);

        m_PhysicalDevice = new PhysicalDevice(m_Instance);
        assert(m_PhysicalDevice);

        m_LogicalDevice = new LogicalDevice(m_Instance, m_PhysicalDevice, m_Surface);
        assert(m_LogicalDevice);

        VkExtent2D Extent = ChooseSwapExtent();
        m_SwapChain = new Swapchain(Extent);
        assert(m_SwapChain);
    }

	void Renderer::InitGraphics()
	{
		m_MsaaSampler = new Multisampler(m_PhysicalDevice->GetMaxUsableSampleCount());
		CreateRenderPass();

        GraphicsHelpers::CreateCommandPool(&m_CommandPool, 0);

		// Load default material
		m_DefaultMat = Material::Create("Materials/Default.mat");
		
        CreateGraphicsPipeline();

		m_MsaaSampler->Create(m_SwapChain->GetExtents(), m_SwapChain->GetImageFormat());

        m_DepthBuffer = new DepthBuffer(m_PhysicalDevice->GetMaxUsableSampleCount());
        assert(m_DepthBuffer);

        // Create the camera
        float CamMoveSpeed = FlingConfig::GetFloat("Camera", "MoveSpeed", 10.0f);
        float CamRotSpeed = FlingConfig::GetFloat("Camera", "RotationSpeed", 40.0f);
        m_camera = new FirstPersonCamera(m_CurrentWindow->GetAspectRatio(), CamMoveSpeed, CamRotSpeed);

		m_BRDFLookupTexture = Image::Create("Textures/brdfLUT.png"_hs);
		assert(m_BRDFLookupTexture);

        CreateFrameBuffers();

        assert(m_Registry);
    
        m_CommandBuffers.resize(m_SwapChainFramebuffers.size());
        GraphicsHelpers::CreateCommandBuffers(
            m_CommandBuffers.data(),
            static_cast<UINT32>(m_CommandBuffers.size()),
            m_CommandPool);

        // Load Skybox
		m_Skybox = new Cubemap(
			"Textures/Skybox/posx.jpg"_hs,
			"Textures/Skybox/negx.jpg"_hs,
			"Textures/Skybox/posy.jpg"_hs,
			"Textures/Skybox/negy.jpg"_hs,
			"Textures/Skybox/posz.jpg"_hs,
			"Textures/Skybox/negz.jpg"_hs,
			HS("Shaders/skybox/skybox.vert.spv"),
			HS("Shaders/skybox/skybox.frag.spv"),
			m_RenderPass,
			m_LogicalDevice->GetVkDevice()
		);

		m_Skybox->Init(
            m_camera, 
            m_SwapChain->GetActiveImageIndex(), 
            m_SwapChain->GetImageViewCount(), 
            m_MsaaSampler);

        CreateLightBuffers();

        BindCommadBuffers(*m_Registry);

#if WITH_IMGUI
        // Initialize imgui
        m_flingImgui = new FlingImgui(m_LogicalDevice, m_SwapChain);
        
        m_DrawImgui = FlingConfig::GetBool("Imgui", "display");
        InitImgui();
#endif  // WITH_IMGUI

        CreateSyncObjects();
    }

    void Renderer::InitImgui()
    {
#if WITH_IMGUI
        m_flingImgui->Init(
            static_cast<float>(m_CurrentWindow->GetWidth()),
            static_cast<float>(m_CurrentWindow->GetHeight())
        );

        m_flingImgui->InitResources(m_LogicalDevice->GetGraphicsQueue());
#endif  // WITH_IMGUI
    }

    void Renderer::CreateLightBuffers()
    {
        ShaderProgramManager::Get().CreateLightBuffers();
    }

    void Renderer::CreateRenderPass()
    {
		assert(m_MsaaSampler && m_SwapChain);

        // We have a single color buffer for the images in the swap chain
        VkAttachmentDescription ColorAttachment = {};
        ColorAttachment.format = m_SwapChain->GetImageFormat();
        ColorAttachment.samples = m_MsaaSampler->GetSampleCountFlagBits();
        ColorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;       // Clear the frame buffer to black
        ColorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

        ColorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        ColorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

        ColorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        // Change to VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL  for imgui
		// and because multisampled images cannot be presented directly
        ColorAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkAttachmentDescription DepthAttatchment = {};
        DepthAttatchment.format = DepthBuffer::GetDepthBufferFormat();
        DepthAttatchment.samples = m_MsaaSampler->GetSampleCountFlagBits();
        DepthAttatchment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        DepthAttatchment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        DepthAttatchment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        DepthAttatchment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        DepthAttatchment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        DepthAttatchment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		VkAttachmentDescription colorAttachmentResolve = {};
		colorAttachmentResolve.format = m_SwapChain->GetImageFormat();
		colorAttachmentResolve.samples = VK_SAMPLE_COUNT_1_BIT;
		colorAttachmentResolve.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		colorAttachmentResolve.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		colorAttachmentResolve.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		colorAttachmentResolve.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		colorAttachmentResolve.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		colorAttachmentResolve.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        // Subpass -------------------
        VkAttachmentReference ColorAttachmentRef = {};
        ColorAttachmentRef.attachment = 0;
        ColorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkAttachmentReference DepthAttatchmentRef = {};
        DepthAttatchmentRef.attachment = 1;
        DepthAttatchmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		VkAttachmentReference colorAttachmentResolveRef = {};
		colorAttachmentResolveRef.attachment = 2;
		colorAttachmentResolveRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkSubpassDescription Subpass = {};
        Subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;    // You need to be explicit that this is
                                                                        // a graphics subpass because we may support compute passes in the future
        Subpass.colorAttachmentCount = 1;
        Subpass.pColorAttachments = &ColorAttachmentRef;
        Subpass.pDepthStencilAttachment = &DepthAttatchmentRef;
		Subpass.pResolveAttachments = &colorAttachmentResolveRef;

        // Add a subpass dependency
        VkSubpassDependency dependency = {};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.srcAccessMask = 0;
        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        // Create the render pass
        std::array<VkAttachmentDescription, 3> Attachments = { ColorAttachment, DepthAttatchment, colorAttachmentResolve };
        VkRenderPassCreateInfo renderPassInfo = {};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.attachmentCount = static_cast<UINT32>(Attachments.size());
        renderPassInfo.pAttachments = Attachments.data();
        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &Subpass;

        renderPassInfo.dependencyCount = 1;
        renderPassInfo.pDependencies = &dependency;

        if (vkCreateRenderPass(m_LogicalDevice->GetVkDevice(), &renderPassInfo, nullptr, &m_RenderPass) != VK_SUCCESS)
        {
            F_LOG_FATAL("Failed to create render pass!");
        }
    }

    void Renderer::CreateGraphicsPipeline()
    {
        ShaderProgramManager::Get().InitGraphicsPipeline(m_RenderPass, m_MsaaSampler);
    }

    void Renderer::CreateFrameBuffers()
    {
        assert(m_SwapChain && m_DepthBuffer && m_MsaaSampler);

        m_SwapChainFramebuffers.resize(m_SwapChain->GetImageViewCount());

        const std::vector<VkImageView>& ImageViews = m_SwapChain->GetImageViews();
        
        // Create the frame buffers based on the image views
        for (size_t i = 0; i < m_SwapChain->GetImageViewCount(); i++)
        {
            std::array<VkImageView, 3> attachments =
            {
				m_MsaaSampler->GetImageView(),
                m_DepthBuffer->GetVkImageView(),
				ImageViews[i]
            };

            VkFramebufferCreateInfo framebufferInfo = {};
            framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebufferInfo.renderPass = m_RenderPass;
            framebufferInfo.attachmentCount = static_cast<UINT32>(attachments.size());
            framebufferInfo.pAttachments = attachments.data();
            framebufferInfo.width = m_SwapChain->GetExtents().width;
            framebufferInfo.height = m_SwapChain->GetExtents().height;
            framebufferInfo.layers = 1;

            if (vkCreateFramebuffer(m_LogicalDevice->GetVkDevice(), &framebufferInfo, nullptr, &m_SwapChainFramebuffers[i]) != VK_SUCCESS)
            {
                F_LOG_FATAL("Failed to create framebuffer!");
            }
        }
    }

    void Renderer::BindCommadBuffers(entt::registry& t_Reg)
    {
        VkCommandBufferBeginInfo beginInfo = {};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
        beginInfo.pInheritanceInfo = nullptr;

        // Start command buffer recording
        for (size_t i = 0; i < m_CommandBuffers.size(); i++)
        {
            if (vkBeginCommandBuffer(m_CommandBuffers[i], &beginInfo) != VK_SUCCESS)
            {
                F_LOG_FATAL("Failed to begin recording command buffer!");
            }

            VkRenderPassBeginInfo renderPassInfo = {};
            renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            renderPassInfo.renderPass = m_RenderPass;
            renderPassInfo.framebuffer = m_SwapChainFramebuffers[i];

            renderPassInfo.renderArea.offset = { 0, 0 };
            renderPassInfo.renderArea.extent = m_SwapChain->GetExtents();

            // Clear values ---------------------
            std::array<VkClearValue, 2> clearValues = {};
            clearValues[0].color = { 0.0f, 0.0f, 0.0f, 1.0f };
            clearValues[1].depthStencil = { 1.0f, 0 };
            renderPassInfo.clearValueCount = static_cast<UINT32>(clearValues.size());
            renderPassInfo.pClearValues = clearValues.data();

            vkCmdBeginRenderPass(m_CommandBuffers[i], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
            
            VkViewport viewport = Initializers::Viewport(static_cast<float>(m_CurrentWindow->GetWidth()), static_cast<float>(m_CurrentWindow->GetHeight()), 0.0f, 1.0f);
            vkCmdSetViewport(m_CommandBuffers[i], 0, 1, &viewport);

            VkRect2D scissor = Initializers::Rect2D(m_CurrentWindow->GetWidth(), m_CurrentWindow->GetHeight(), 0, 0);
            vkCmdSetScissor(m_CommandBuffers[i], 0, 1, &scissor);

            m_Skybox->BindCmdBuffer(m_CommandBuffers[i]);

            ShaderProgramManager::Get().BindCmdBuffer(m_CommandBuffers[i], i);

            vkCmdEndRenderPass(m_CommandBuffers[i]);

            if (vkEndCommandBuffer(m_CommandBuffers[i]) != VK_SUCCESS)
            {
                F_LOG_FATAL("failed to record command buffer!");
            }
        }
    }

    void Renderer::CreateSyncObjects()
    {
        m_ImageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
        m_RenderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
        m_InFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

        VkFenceCreateInfo fenceInfo = {};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        {
			m_ImageAvailableSemaphores[i] = GraphicsHelpers::CreateSemaphore(m_LogicalDevice->GetVkDevice());
			m_RenderFinishedSemaphores[i] = GraphicsHelpers::CreateSemaphore(m_LogicalDevice->GetVkDevice());
            if (vkCreateFence(m_LogicalDevice->GetVkDevice(), &fenceInfo, nullptr, &m_InFlightFences[i]) != VK_SUCCESS)
            {
                F_LOG_FATAL("Failed to create semaphores or fence!");
            }
        }
    }

    void Renderer::RecreateFrameResources()
    {
        m_CurrentWindow->RecreateSwapChain();
        m_LogicalDevice->WaitForIdle();

        CleanupFrameResources();
#if WITH_IMGUI
        m_flingImgui->Release();
#endif
        m_SwapChain->Recreate(ChooseSwapExtent());

        CreateRenderPass();

        m_DepthBuffer->Create();
		if (m_MsaaSampler)
		{
			m_MsaaSampler->Create(m_SwapChain->GetExtents(), m_SwapChain->GetImageFormat());
		}

        CreateFrameBuffers();

        assert(m_Registry);

        m_CommandBuffers.resize(m_SwapChainFramebuffers.size());
        GraphicsHelpers::CreateCommandBuffers(
            m_CommandBuffers.data(), 
            static_cast<UINT32>(m_CommandBuffers.size()), 
            m_CommandPool);

        BindCommadBuffers(*m_Registry);
        InitImgui();
    }

    // Swapchain support --------------------------------------

    VkExtent2D Renderer::ChooseSwapExtent()
    {
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

    void Renderer::CreateGameWindow(const UINT32 t_width, const UINT32 t_height)
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

    void Renderer::Tick(float DeltaTime)
    {
        m_CurrentWindow->Update();

        m_camera->Update(DeltaTime);
    }

    void Renderer::DrawFrame(entt::registry& t_Reg, float DeltaTime)
    {
        VkResult iResult = m_SwapChain->AquireNextImage(m_ImageAvailableSemaphores[CurrentFrameIndex]);
        UINT32  ImageIndex = m_SwapChain->GetActiveImageIndex();

        vkResetFences(m_LogicalDevice->GetVkDevice(), 1, &m_InFlightFences[CurrentFrameIndex]);

#if WITH_IMGUI
        // Update imgui command buffers
        {
            assert(m_Editor);
            vkResetCommandPool(m_LogicalDevice->GetVkDevice(), m_flingImgui->GetCommandPool(), 0);
            // Prepare the ImGUI buffers to be built
            m_flingImgui->PrepFrameBuild();
#if WITH_EDITOR
            // Draw any ImGUI items here
            m_Editor->Draw(*m_Registry, DeltaTime);
#endif
            // Build the actual ImGUI command buffers
            m_flingImgui->BindCmdBuffers(m_DrawImgui);
        }  
#endif
        // Check if the swap chain needs to be recreated
        if (iResult == VK_ERROR_OUT_OF_DATE_KHR)
        {
            RecreateFrameResources();
            return;
        }
        else if (iResult != VK_SUCCESS && iResult != VK_SUBOPTIMAL_KHR)
        {
            F_LOG_FATAL("Failed to acquire swap chain image!");
        }

        UpdateUniformBuffer(ImageIndex);
        m_Skybox->UpdateUniformBuffer(ImageIndex, m_camera->GetProjectionMatrix(), m_camera->GetViewMatrix());

        VkSubmitInfo submitInfo = {};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

        VkSemaphore waitSemaphores[] = { m_ImageAvailableSemaphores[CurrentFrameIndex] };
        VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = waitSemaphores;
        submitInfo.pWaitDstStageMask = waitStages;

        std::vector<VkCommandBuffer> submitCommandBuffers = {};

        submitCommandBuffers.emplace_back(m_CommandBuffers[ImageIndex]);

#if WITH_IMGUI
        submitCommandBuffers.emplace_back(m_flingImgui->GetCommandBuffer(ImageIndex));
#endif

        submitInfo.commandBufferCount = static_cast<UINT32>(submitCommandBuffers.size());
        submitInfo.pCommandBuffers = submitCommandBuffers.data();

        VkSemaphore signalSemaphores[] = { m_RenderFinishedSemaphores[CurrentFrameIndex] };
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = signalSemaphores;

        if (vkQueueSubmit(m_LogicalDevice->GetGraphicsQueue(), 1, &submitInfo, m_InFlightFences[CurrentFrameIndex]) != VK_SUCCESS)
        {
            F_LOG_FATAL("Failed to submit draw command buffer!");
        }

        vkWaitForFences(m_LogicalDevice->GetVkDevice(), 1, &m_InFlightFences[CurrentFrameIndex], VK_TRUE, std::numeric_limits<uint64_t>::max());

        iResult = m_SwapChain->QueuePresent(m_LogicalDevice->GetPresentQueue(), *signalSemaphores);

        if (iResult == VK_ERROR_OUT_OF_DATE_KHR || iResult == VK_SUBOPTIMAL_KHR || m_FrameBufferResized)
        {
            m_FrameBufferResized = false;
            RecreateFrameResources();
        }
        else if (iResult != VK_SUCCESS)
        {
            F_LOG_FATAL("Failed to present swap chain image!");
        }

        if (m_RebuildCommanfBuffer)
        {
            vkResetCommandPool(m_LogicalDevice->GetVkDevice(), m_CommandPool, 0);
            BindCommadBuffers(t_Reg);
            m_RebuildCommanfBuffer = false;
        }

        CurrentFrameIndex = (CurrentFrameIndex + 1) % MAX_FRAMES_IN_FLIGHT;
    }

	void Renderer::UpdateUniformBuffer(UINT32 t_CurrentImage)
	{
        ShaderProgramManager::Get().UpdateUniformBuffers(t_CurrentImage, m_camera);
    }

    // Shutdown steps -------------------------------------------
    void Renderer::CleanupFrameResources()
    {
        m_DepthBuffer->Cleanup();
        if (m_MsaaSampler)
        {
            m_MsaaSampler->Release();
        }

        for (size_t i = 0; i < m_SwapChainFramebuffers.size(); i++)
        {
            vkDestroyFramebuffer(m_LogicalDevice->GetVkDevice(), m_SwapChainFramebuffers[i], nullptr);
        }

        vkFreeCommandBuffers(m_LogicalDevice->GetVkDevice(), m_CommandPool, static_cast<UINT32>(m_CommandBuffers.size()), m_CommandBuffers.data());

        vkDestroyRenderPass(m_LogicalDevice->GetVkDevice(), m_RenderPass, nullptr);

        m_SwapChain->Cleanup();
    }

    void Renderer::PrepShutdown()
    {
		m_IsQuitting = true;

        m_LogicalDevice->WaitForIdle();

        ShaderProgramManager::Get().PrepShutdown();

         // Delete light buffers
        for (size_t i = 0; i < m_Lighting.m_LightingUBOs.size(); i++)
		{
			if(m_Lighting.m_LightingUBOs[i])
            {
                delete m_Lighting.m_LightingUBOs[i];
                m_Lighting.m_LightingUBOs[i] = nullptr;
            }
		}
        m_Lighting.m_LightingUBOs.clear();

		// release refs to textures
		m_BRDFLookupTexture = nullptr;
    }

    void Renderer::Shutdown()
    {
        // Cleanup Vulkan ------
        CleanupFrameResources();
           
        ShaderProgramManager::Get().Shutdown();

        if (m_camera)
        {
            delete m_camera;
            m_camera = nullptr;
        }

        if (m_Skybox)
        {
            delete m_Skybox;
            m_Skybox = nullptr;
        }
#if WITH_IMGUI
        if (m_flingImgui)
        {
            delete m_flingImgui;
            m_flingImgui = nullptr;
        }
#endif
        if (m_SwapChain)
        {
            delete m_SwapChain;
            m_SwapChain = nullptr;
        }

        if (m_GraphicsPipeline)
        {
            delete m_GraphicsPipeline;
            m_GraphicsPipeline = nullptr;
        }
		
		if (m_DepthBuffer)
		{
			delete m_DepthBuffer;
			m_DepthBuffer = nullptr;
		}

		if (m_MsaaSampler)
		{
			delete m_MsaaSampler;
			m_MsaaSampler = nullptr;
		}

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        {
            vkDestroySemaphore(m_LogicalDevice->GetVkDevice(), m_RenderFinishedSemaphores[i], nullptr);
            vkDestroySemaphore(m_LogicalDevice->GetVkDevice(), m_ImageAvailableSemaphores[i], nullptr);
            vkDestroyFence(m_LogicalDevice->GetVkDevice(), m_InFlightFences[i], nullptr);
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

    // @see https://github.com/skypjack/entt/wiki/Crash-Course:-entity-component-system#observe-changes
	// for more on entt 
    void Renderer::InitComponentData()
    {
        // Add any component callbacks that we may need
        m_Registry->on_construct<MeshRenderer>().connect<&Renderer::MeshRendererAdded>(*this);
		m_Registry->on_destroy<MeshRenderer>().connect<&Renderer::MeshRendererRemoved>(*this);

        m_Registry->on_construct<DirectionalLight>().connect<&Renderer::DirLightAdded>(*this);
		m_Registry->on_construct<PointLight>().connect<&Renderer::PointLightAdded>(*this);

        m_Registry->on_replace<entt::tag<HS("PBR")>>().connect<&Renderer::MeshRendererMaterialChange>(*this);
        m_Registry->on_replace<entt::tag<HS("Reflection")>>().connect<&Renderer::MeshRendererMaterialChange>(*this);

    }

    void Renderer::MeshRendererAdded(entt::entity t_Ent, entt::registry& t_Reg, MeshRenderer& t_MeshRend)
    {
		const std::vector<VkImage>& Images = m_SwapChain->GetImages();
		VkDeviceSize bufferSize = sizeof(UboVS);

		t_MeshRend.m_UniformBuffers.resize(Images.size());
		for (size_t i = 0; i < Images.size(); i++)
		{
			t_MeshRend.m_UniformBuffers[i] = new Buffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
			t_MeshRend.m_UniformBuffers[i]->MapMemory(bufferSize);
		}

		SetFrameBufferHasBeenResized(true);

        // Assign shader program type
		if (!t_MeshRend.m_Material)
		{
			F_LOG_WARN("Mesh renderer has no material! Default being assigned.");
			t_MeshRend.m_Material = m_DefaultMat.get();
		}

        MeshRenderer::AssignShaderProgram(t_MeshRend, t_Reg, t_Ent);
        ShaderProgramManager::Get().CreateDescriptors(t_MeshRend);

        ShaderProgramManager::Get().SortMeshRender();
        m_RebuildCommanfBuffer = true;        
    }

	void Renderer::MeshRendererRemoved(entt::entity t_Ent, entt::registry& t_Reg)
	{
        //Release meshrenderer resources
        MeshRenderer& t_MeshRend = t_Reg.get<MeshRenderer>(t_Ent);
        ShaderProgramManager::Get().ReleaseMeshRenderer(t_MeshRend);

        ShaderProgramManager::Get().SortMeshRender();
        m_RebuildCommanfBuffer = true;
	}

    void Renderer::MeshRendererMaterialChange(entt::entity t_Ent, entt::registry& t_Reg)
    {
        MeshRenderer& m = t_Reg.get<MeshRenderer>(t_Ent);
        ShaderProgramManager::Get().RebuildDescriptors(m);
    }

    void Renderer::DirLightAdded(entt::entity t_Ent, entt::registry& t_Reg, DirectionalLight& t_Light)
    {
        F_LOG_TRACE("Directional Light added!");
        ++m_Lighting.m_CurrentDirLights;

		// Ensure that we have a transform component before adding a light
		if (!t_Reg.has<Transform>(t_Ent))
		{
			t_Reg.assign<Transform>(t_Ent);
		}

        if(m_Lighting.m_CurrentDirLights > Lighting::MaxDirectionalLights)
        {
            F_LOG_WARN("You have enterer more then the max support directional lights of Fling!");
        }
    }

	void Renderer::PointLightAdded(entt::entity t_Ent, entt::registry& t_Reg, PointLight& t_Light)
	{
		F_LOG_TRACE("Point Light added!");
		++m_Lighting.m_CurrentPointLights;

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

		if (m_Lighting.m_CurrentPointLights > Lighting::MaxPointLights)
		{
			F_LOG_WARN("You have enterer more then the max support point lights of Fling!");
		}
	}
}    // namespace FlingR