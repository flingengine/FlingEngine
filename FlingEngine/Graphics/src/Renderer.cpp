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
		assert(m_Registry);
        InitDevices();

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
		CreateDescriptorLayout();

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

        CreateDescriptorPool();
        CreateDescriptorSets();

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

		m_Skybox->Init(m_camera, m_SwapChain->GetActiveImageIndex(), m_SwapChain->GetImageViewCount(), m_PhysicalDevice->GetMaxUsableSampleCount());

        CreateLightBuffers();

        BuildCommandBuffers(*m_Registry);

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
        const std::vector<VkImage>& Images = m_SwapChain->GetImages();
		VkDeviceSize bufferSize = sizeof(m_LightingUBO);

		m_Lighting.m_LightingUBOs.resize(Images.size());
		for (size_t i = 0; i < Images.size(); i++)
		{
			m_Lighting.m_LightingUBOs[i] = new Buffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
			m_Lighting.m_LightingUBOs[i]->MapMemory(bufferSize);
		}
    }

    void Renderer::UpdateImguiIO()
    {
#if WITH_IMGUI
        // Update imgui mouse events and timings
        ImGuiIO& io = ImGui::GetIO();

        io.DisplaySize = ImVec2(
            static_cast<float>(m_CurrentWindow->GetWidth()),
            static_cast<float>(m_CurrentWindow->GetHeight()));

        io.DeltaTime = Timing::Get().GetDeltaTime();
        io.MousePos = ImVec2(Input::GetMousePos().X, Input::GetMousePos().Y);

        io.MouseDown[0] = Input::IsMouseDown(KeyNames::FL_MOUSE_BUTTON_1);
        io.MouseDown[1] = Input::IsMouseDown(KeyNames::FL_MOUSE_BUTTON_2);
#endif
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

    void Renderer::CreateDescriptorLayout()
    {
		m_DescriptorSetLayout = Shader::CreateSetLayout(m_LogicalDevice->GetVkDevice(), ShaderProgram::Get().GetAllShaders());
    }

    void Renderer::CreateGraphicsPipeline()
    {
        // Shader stage creation!
        const auto& Shaders = ShaderProgram::Get().GetAllShaders();
        std::vector<VkPipelineShaderStageCreateInfo> ShaderStages;

        for(const Shader* shader : Shaders)
        {		
			VkPipelineShaderStageCreateInfo createInfo = {};
			createInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
			createInfo.module = shader->GetShaderModule();
			createInfo.stage = shader->GetStage();
			createInfo.pName = "main";
			createInfo.flags = 0;
			createInfo.pNext = nullptr;
			createInfo.pSpecializationInfo = nullptr;
			ShaderStages.push_back(createInfo);
        }

        // Vertex input ----------------------
        VkVertexInputBindingDescription BindingDescription = Vertex::GetBindingDescription();
        auto AttributeDescriptions = Vertex::GetAttributeDescriptions();

        VkPipelineVertexInputStateCreateInfo VertexInputInfo = {};
        VertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        VertexInputInfo.vertexBindingDescriptionCount = 1;
        VertexInputInfo.pVertexBindingDescriptions = &BindingDescription;
        VertexInputInfo.vertexAttributeDescriptionCount = static_cast<UINT32>(AttributeDescriptions.size());
        VertexInputInfo.pVertexAttributeDescriptions = AttributeDescriptions.data();

        // Input Assembly ----------------------
        VkPipelineInputAssemblyStateCreateInfo InputAssembly = {};
        InputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        InputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        InputAssembly.primitiveRestartEnable = VK_FALSE;

        // View ports and scissors ----------------------
        VkViewport viewport = {};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = (float)m_SwapChain->GetExtents().width;       // These values can differ from the width/height of the window!
        viewport.height = (float)m_SwapChain->GetExtents().height;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;

        VkRect2D scissor = {};
        scissor.offset = { 0, 0 };
        scissor.extent = m_SwapChain->GetExtents();

        VkPipelineViewportStateCreateInfo viewportState = {};
        viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1;
        viewportState.pViewports = &viewport;
        viewportState.scissorCount = 1;
        viewportState.pScissors = &scissor;

        // Rasterizer ----------------------------------
        VkPipelineRasterizationStateCreateInfo Rasterizer = {};
        {
            Rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
            Rasterizer.depthClampEnable = VK_FALSE;
            Rasterizer.rasterizerDiscardEnable = VK_FALSE;  // Useful for shadow maps, using would require enabling a GPU feature
            Rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
            Rasterizer.lineWidth = 1.0f;

            Rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
            Rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE; // Specify the vertex order! 

            Rasterizer.depthBiasEnable = VK_FALSE;
            Rasterizer.depthBiasConstantFactor = 0.0f;  // Optional
            Rasterizer.depthBiasClamp = 0.0f;           // Optional
            Rasterizer.depthBiasSlopeFactor = 0.0f;     // Optional
        }

        // Multi-sampling ----------------------------------
        // Can be a cheaper way to perform anti-aliasing
        // Using it requires enabling a GPU feature
		assert(m_MsaaSampler);
        VkPipelineMultisampleStateCreateInfo Multisampling = {};
        {
            Multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
            Multisampling.sampleShadingEnable = VK_TRUE;
            Multisampling.rasterizationSamples = m_MsaaSampler->GetSampleCountFlagBits();
            Multisampling.minSampleShading = 0.2f; 
            Multisampling.pSampleMask = nullptr; // Optional
            Multisampling.alphaToCoverageEnable = VK_FALSE; // Optional
            Multisampling.alphaToOneEnable = VK_FALSE; // Optional
        }

        // Color blending ----------------------------------
        VkPipelineColorBlendAttachmentState ColorBlendAttachment = {};
        VkPipelineColorBlendStateCreateInfo ColorBlending = {};
        {
            // Attatchment
            ColorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
            ColorBlendAttachment.blendEnable = VK_FALSE;
            ColorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;     // Optional
            ColorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;    // Optional
            ColorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;                // Optional
            ColorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;     // Optional
            ColorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;    // Optional
            ColorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;                // Optional

            ColorBlendAttachment.blendEnable = VK_TRUE;
            ColorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
            ColorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            ColorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
            ColorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
            ColorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
            ColorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

            // Blend
            ColorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
            ColorBlending.logicOpEnable = VK_FALSE;
            ColorBlending.logicOp = VK_LOGIC_OP_COPY;   // Optional
            ColorBlending.attachmentCount = 1;
            ColorBlending.pAttachments = &ColorBlendAttachment;
            ColorBlending.blendConstants[0] = 0.0f;     // Optional
            ColorBlending.blendConstants[1] = 0.0f;     // Optional
            ColorBlending.blendConstants[2] = 0.0f;     // Optional
            ColorBlending.blendConstants[3] = 0.0f;     // Optional
        }

        // Pipeline layout ---------------------
		m_PipelineLayout = Shader::CreatePipelineLayout(m_LogicalDevice->GetVkDevice(), m_DescriptorSetLayout, 0, 0);

		// Depth Stencil
        VkPipelineDepthStencilStateCreateInfo depthStencil = {};
        {
            depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
            depthStencil.depthTestEnable = VK_TRUE;
            depthStencil.depthWriteEnable = VK_TRUE;
            depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
            depthStencil.depthBoundsTestEnable = VK_FALSE;
            depthStencil.minDepthBounds = 0.0f;
            depthStencil.maxDepthBounds = 1.0f;
            depthStencil.stencilTestEnable = VK_FALSE;
            depthStencil.front = {};
            depthStencil.back = {};
        }

        // Create graphics pipeline ------------------------
        VkGraphicsPipelineCreateInfo pipelineInfo = {};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		pipelineInfo.stageCount = (UINT32)ShaderStages.size();
        pipelineInfo.pStages = ShaderStages.data();

        pipelineInfo.pVertexInputState = &VertexInputInfo;
        pipelineInfo.pInputAssemblyState = &InputAssembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &Rasterizer;
        pipelineInfo.pMultisampleState = &Multisampling;
        pipelineInfo.pDepthStencilState = &depthStencil;
        pipelineInfo.pColorBlendState = &ColorBlending;
        pipelineInfo.pDynamicState = nullptr; // Optional

        pipelineInfo.layout = m_PipelineLayout;
        pipelineInfo.renderPass = m_RenderPass;
        pipelineInfo.subpass = 0;

        pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;   // Optional
        pipelineInfo.basePipelineIndex = -1;                // Optional

        if (vkCreateGraphicsPipelines(m_LogicalDevice->GetVkDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_GraphicsPipeline) != VK_SUCCESS)
        {
            F_LOG_FATAL("failed to create graphics pipeline!");
        }
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

    void Renderer::BuildCommandBuffers(entt::registry& t_Reg)
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
            
            VkViewport viewport = Initalizers::Viewport(static_cast<float>(m_CurrentWindow->GetWidth()), static_cast<float>(m_CurrentWindow->GetHeight()), 0.0f, 1.0f);
            vkCmdSetViewport(m_CommandBuffers[i], 0, 1, &viewport);

            VkRect2D scissor = Initalizers::Rect2D(m_CurrentWindow->GetWidth(), m_CurrentWindow->GetHeight(), 0, 0);
            vkCmdSetScissor(m_CommandBuffers[i], 0, 1, &scissor);

            // Skybox -----------------------------
			VkBuffer skyboxVertexBuffers[1] = { m_Skybox->GetVertexBuffer()->GetVkBuffer() };
			VkDeviceSize offsets[1] = { 0 };
			VkDescriptorSet skyboxDescriptorSet[1] = { m_Skybox->GetDescriptorSet() };

			vkCmdBindDescriptorSets(m_CommandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, m_Skybox->GetPipelineLayout(), 0, 1, skyboxDescriptorSet, 0, NULL);
			vkCmdBindVertexBuffers(m_CommandBuffers[i], 0, 1, skyboxVertexBuffers, offsets);
			vkCmdBindIndexBuffer(m_CommandBuffers[i], m_Skybox->GetIndexBuffer()->GetVkBuffer(), 0, m_Skybox->GetIndexType());
			vkCmdBindPipeline(m_CommandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, m_Skybox->GetPipeLine());
			vkCmdDrawIndexed(m_CommandBuffers[i], m_Skybox->GetIndexCount(), 1, 0, 0, 0);
			
			vkCmdBindPipeline(m_CommandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, m_GraphicsPipeline);


            // For each active mesh renderer, bind it's vertex and index buffer
            t_Reg.view<MeshRenderer>().each([&](MeshRenderer& t_MeshRend)
            {
                Fling::Model* Model = t_MeshRend.m_Model;
                if (Model)
                {
                    VkBuffer vertexBuffers[1] = { Model->GetVertexBuffer()->GetVkBuffer() };
                    VkDeviceSize offsets[1] = { 0 };
                    // Bind the descriptor set for rendering a mesh using the dynamic offset
                    vkCmdBindDescriptorSets(m_CommandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, m_PipelineLayout, 0, 1, &t_MeshRend.m_DescriptorSets[i], 0, nullptr);

                    vkCmdBindVertexBuffers(m_CommandBuffers[i], 0, 1, vertexBuffers, offsets);
                    vkCmdBindIndexBuffer(m_CommandBuffers[i], Model->GetIndexBuffer()->GetVkBuffer(), 0, Model->GetIndexType());

                    vkCmdDrawIndexed(m_CommandBuffers[i], Model->GetIndexCount(), 1, 0, 0, 0);
                }
                else
                {
                    F_LOG_WARN("Model is invalid on mesh renderer!");
                }

            });

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

        vkDestroyPipeline(m_LogicalDevice->GetVkDevice(), m_GraphicsPipeline, nullptr);
        vkDestroyPipelineLayout(m_LogicalDevice->GetVkDevice(), m_PipelineLayout, nullptr);
        vkDestroyRenderPass(m_LogicalDevice->GetVkDevice(), m_RenderPass, nullptr);

        m_SwapChain->Cleanup();
		
        vkDestroyDescriptorPool(m_LogicalDevice->GetVkDevice(), m_DescriptorPool, nullptr);
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
        CreateGraphicsPipeline();

        m_DepthBuffer->Create();
		if (m_MsaaSampler)
		{
			m_MsaaSampler->Create(m_SwapChain->GetExtents(), m_SwapChain->GetImageFormat());
		}

        CreateFrameBuffers();

        CreateDescriptorPool();
        CreateDescriptorSets();

        assert(m_Registry);

        m_CommandBuffers.resize(m_SwapChainFramebuffers.size());
        GraphicsHelpers::CreateCommandBuffers(
            m_CommandBuffers.data(), 
            static_cast<UINT32>(m_CommandBuffers.size()), 
            m_CommandPool);

        BuildCommandBuffers(*m_Registry);
        InitImgui();
    }

    void Renderer::CreateDescriptorPool()
    {
        UINT32 SwapImageCount = static_cast<UINT32>(m_SwapChain->GetImageCount());

		UINT32 DescriptorCount = 128;

        std::vector<VkDescriptorPoolSize> PoolSizes =
        {
            Initalizers::DescriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, DescriptorCount),
			Initalizers::DescriptorPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, DescriptorCount),
			Initalizers::DescriptorPoolSize(VK_DESCRIPTOR_TYPE_SAMPLER, DescriptorCount),
            Initalizers::DescriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, DescriptorCount),
			Initalizers::DescriptorPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, DescriptorCount)
        };

        VkDescriptorPoolCreateInfo PoolInfo = {};
        PoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        PoolInfo.poolSizeCount = static_cast<UINT32>(PoolSizes.size());
        PoolInfo.pPoolSizes = PoolSizes.data();
        PoolInfo.maxSets = SwapImageCount;

        m_Registry->view<MeshRenderer>().each([&](MeshRenderer& t_MeshRend)
        {
            if (vkCreateDescriptorPool(m_LogicalDevice->GetVkDevice(), &PoolInfo, nullptr, &t_MeshRend.m_DescriptorPool) != VK_SUCCESS)
            {
                F_LOG_FATAL("Failed to create discriptor pool!");
            }
        });
    }

    void Renderer::CreateDescriptorSets()
    {
        const std::vector<VkImage>& Images = m_SwapChain->GetImages();

        m_Registry->view<MeshRenderer>().each([&](MeshRenderer& t_MeshRend)
        {
            // Specify what descriptor pool to allocate from and how many
            std::vector<VkDescriptorSetLayout> layouts(Images.size(), m_DescriptorSetLayout);
            VkDescriptorSetAllocateInfo allocInfo = {};
            allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
            allocInfo.descriptorPool = t_MeshRend.m_DescriptorPool;
            allocInfo.descriptorSetCount = static_cast<UINT32>(Images.size());
            allocInfo.pSetLayouts = layouts.data();

            t_MeshRend.m_DescriptorSets.resize(Images.size());

            // Sets will be cleaned up when the descriptor pool is, no need for an explicit free call in cleanup
            if (vkAllocateDescriptorSets(m_LogicalDevice->GetVkDevice(), &allocInfo, t_MeshRend.m_DescriptorSets.data()) != VK_SUCCESS)
            {
                F_LOG_FATAL("Failed to allocate descriptor sets!");
            }
        });

		static CircularBuffer<VkDescriptorImageInfo, 4096> ImageInfoBuf = {};
		static CircularBuffer<VkDescriptorBufferInfo, 4096> UBOBuf = {};

        auto AddImageSampler = [&](Image* t_Image, UINT32 t_Binding, VkDescriptorSet t_DstSet, std::vector<VkWriteDescriptorSet>& t_SetArray)
        {
            // Create an image binding
            VkDescriptorImageInfo* ImageInfo = ImageInfoBuf.GetItem();
            ImageInfo->imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            ImageInfo->imageView = t_Image->GetVkImageView();
            ImageInfo->sampler = t_Image->GetSampler();

            // Create sampler information
            VkWriteDescriptorSet ImageSamplerSet = {};
            ImageSamplerSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            ImageSamplerSet.dstSet = t_DstSet;
            ImageSamplerSet.dstBinding = t_Binding;
            ImageSamplerSet.dstArrayElement = 0;
            ImageSamplerSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            ImageSamplerSet.descriptorCount = 1;
            ImageSamplerSet.pImageInfo = ImageInfo;

            // Add to the set array
            t_SetArray.push_back(ImageSamplerSet);
        };

        // Create material description sets for each swap chain image that we have
        for (size_t i = 0; i < Images.size(); ++i)
        {
			std::vector<VkWriteDescriptorSet> descriptorWrites;

			m_Registry->view<MeshRenderer>().each([&](MeshRenderer& t_MeshRend)
			{
				// Binding 0 : Projection/view matrix uniform buffer
				VkDescriptorBufferInfo* BufferInfo = UBOBuf.GetItem();
				BufferInfo->buffer = t_MeshRend.m_UniformBuffers[i]->GetVkBuffer();
				BufferInfo->offset = 0;
				BufferInfo->range = t_MeshRend.m_UniformBuffers[i]->GetSize();
				VkWriteDescriptorSet UniformSet = Initalizers::WriteDescriptorSet(
                    t_MeshRend.m_DescriptorSets[i],
					VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
					0,
					BufferInfo
				);

				descriptorWrites.push_back(UniformSet);

                AddImageSampler(t_MeshRend.m_Material->m_Textures.m_AlbedoTexture, 2, t_MeshRend.m_DescriptorSets[i], descriptorWrites);
                AddImageSampler(t_MeshRend.m_Material->m_Textures.m_NormalTexture, 3, t_MeshRend.m_DescriptorSets[i], descriptorWrites);
                AddImageSampler(t_MeshRend.m_Material->m_Textures.m_MetalTexture, 4, t_MeshRend.m_DescriptorSets[i], descriptorWrites);
                AddImageSampler(t_MeshRend.m_Material->m_Textures.m_RoughnessTexture, 5, t_MeshRend.m_DescriptorSets[i], descriptorWrites);
				AddImageSampler(m_BRDFLookupTexture.get(), 7, t_MeshRend.m_DescriptorSets[i], descriptorWrites);
				// TODO: Ambient Occlusion Map

                // Binding 6 : Fragment shader directional lights
                // A uniform buffer of lights! 
				VkDescriptorBufferInfo* LightBufferInfo = UBOBuf.GetItem();
				LightBufferInfo->buffer = m_Lighting.m_LightingUBOs[i]->GetVkBuffer();
				LightBufferInfo->offset = 0;
				LightBufferInfo->range = m_Lighting.m_LightingUBOs[i]->GetSize();

				VkWriteDescriptorSet LightUniformSet = Initalizers::WriteDescriptorSet(
                    t_MeshRend.m_DescriptorSets[i],
					VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
					6,
					LightBufferInfo
				);

				descriptorWrites.push_back(LightUniformSet);

                vkUpdateDescriptorSets(m_LogicalDevice->GetVkDevice(), static_cast<UINT32>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
			});
        }
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
#if WITH_IMGUI
		UpdateImguiIO();
#endif
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
            m_flingImgui->BuildCommandBuffers(m_DrawImgui);
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


        CurrentFrameIndex = (CurrentFrameIndex + 1) % MAX_FRAMES_IN_FLIGHT;
    }

	void Renderer::UpdateUniformBuffer(UINT32 t_CurrentImage)
	{
		// For each active mesh renderer update it's UBO
		{
            auto view = m_Registry->view<MeshRenderer, Transform>();
            for (auto entity : view)
            {
                MeshRenderer& Mesh = view.get<MeshRenderer>(entity);
                Transform& Trans = view.get<Transform>(entity);

                Transform::CalculateWorldMatrix(Trans);

                // Calculate the world matrix based on the given transform
                UboVS ubo = {};
                ubo.Model = Trans.GetWorldMat();
                ubo.View = m_camera->GetViewMatrix();
                ubo.Projection = m_camera->GetProjectionMatrix();
                ubo.Projection[1][1] *= -1.0f;
				ubo.CamPos = m_camera->GetPosition();
                ubo.ObjPos = Trans.GetPos();

                // Copy the ubo to the GPU
                Buffer* buf = Mesh.m_UniformBuffers[t_CurrentImage];
                memcpy(buf->m_MappedMem, &ubo, buf->GetSize());
            }
        }

        // Copy directional lights to the fragment shader
        {
            auto lightView = m_Registry->view<DirectionalLight>();
            UINT32 CurLightCount = 0;

            for(auto entity : lightView)
            {
                if(CurLightCount < Lighting::MaxDirectionalLights)
                {
                    DirectionalLight& Light = lightView.get(entity);
                    // Copy the dir light info to the buffer
					size_t size = sizeof(DirectionalLight);
                    memcpy((m_LightingUBO.DirLightBuffer + (CurLightCount++)), &Light, size);
                }
            }
            
            m_LightingUBO.DirLightCount = CurLightCount;
        }

		// Copy the point light data to the UBO
		{
			auto lightView = m_Registry->view<PointLight, Transform>();
			UINT32 CurLightCount = 0;

			for (auto entity : lightView)
			{
				if (CurLightCount < Lighting::MaxPointLights)
				{
					PointLight& Light = lightView.get<PointLight>(entity);
					Transform& Trans = lightView.get<Transform>(entity);
					
					Light.SetPos(glm::vec4(Trans.GetPos(), 1.0f));

					// Copy the dir light info to the buffer
					memcpy((m_LightingUBO.PointLightBuffer + (CurLightCount++)), &Light,  sizeof(PointLight));
				}
			}

			m_LightingUBO.PointLightCount = CurLightCount;
		}

		// Memcpy the dir light UBO
		memcpy(m_Lighting.m_LightingUBOs[t_CurrentImage]->m_MappedMem, &m_LightingUBO, sizeof(m_LightingUBO));
    }

    // Shutdown steps -------------------------------------------

    void Renderer::PrepShutdown()
    {
		m_IsQuitting = true;

        m_LogicalDevice->WaitForIdle();
		m_Registry->view<MeshRenderer>().each([&](MeshRenderer& t_MeshRend)
		{
			t_MeshRend.Release();
			vkDestroyDescriptorPool(m_LogicalDevice->GetVkDevice(), t_MeshRend.m_DescriptorPool, nullptr);
        });

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

        vkDestroyDescriptorSetLayout(m_LogicalDevice->GetVkDevice(), m_DescriptorSetLayout, nullptr);

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
        m_Registry->on_construct<DirectionalLight>().connect<&Renderer::DirLightAdded>(*this);
		m_Registry->on_construct<PointLight>().connect<&Renderer::PointLightAdded>(*this);
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
    }

    void Renderer::DirLightAdded(entt::entity t_Ent, entt::registry& t_Reg, DirectionalLight& t_Light)
    {
        F_LOG_TRACE("Directional Light added!");
        ++m_Lighting.m_CurrentDirLights;
        if(m_Lighting.m_CurrentDirLights > Lighting::MaxDirectionalLights)
        {
            F_LOG_WARN("You have enterer more then the max support directional lights of Fling!");
        }
    }

	void Renderer::PointLightAdded(entt::entity t_Ent, entt::registry& t_Reg, PointLight& t_Light)
	{
		F_LOG_TRACE("Point Light added!");
		++m_Lighting.m_CurrentPointLights;

		Transform& t = t_Reg.assign<Transform>(t_Ent);

#if FLING_DEBUG
		// Make a cute little debug mesh on a point light	
		t.SetScale(glm::vec3{ 0.1f });
		t_Reg.assign<MeshRenderer>(t_Ent, "Models/sphere.obj");
#endif	// FLING_DEBUG

		if (m_Lighting.m_CurrentPointLights> Lighting::MaxPointLights)
		{
			F_LOG_WARN("You have enterer more then the max support point lights of Fling!");
		}
	}
}    // namespace FlingR