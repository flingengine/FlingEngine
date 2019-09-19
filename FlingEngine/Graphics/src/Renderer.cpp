#include "pch.h"
#include "Renderer.h"
#include "ResourceManager.h"
#include "FlingConfig.h"
#include "File.h"
#include "Image.h"
#include "GraphicsHelpers.h"

namespace Fling
{
    const int Renderer::MAX_FRAMES_IN_FLIGHT = 2;

	void Renderer::Init()
	{
		InitGraphics();
		float CamMoveSpeed = FlingConfig::GetFloat("Camera", "MoveSpeed", 10.0f);
		float CamRotSpeed = FlingConfig::GetFloat("Camera", "RotationSpeed", 40.0f);
		m_camera = std::make_unique<FirstPersonCamera>(m_CurrentWindow->GetAspectRatio(), CamMoveSpeed, CamRotSpeed);
	}

	void Renderer::InitGraphics()
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

        CreateRenderPass();
        CreateDescriptorLayout();
        CreateGraphicsPipeline();

        CreateFrameBuffers();
        CreateCommandPool();
        
		m_DepthBuffer = new DepthBuffer();

		// A test function for loading in images
        CreateTextureImage();

		CreateVertexBuffer();
		CreateIndexBuffer();
        CreateUniformBuffers();
        CreateDescriptorPool();
        CreateDescriptorSets();

        CreateCommandBuffers();
        CreateSyncObjects();
	}

    void Renderer::CreateRenderPass()
    {
        // We have a single color buffer for the images in the swap chain
        VkAttachmentDescription ColorAttachment = {};
        ColorAttachment.format = m_SwapChain->GetImageFormat();
        ColorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        ColorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;       // Clear the frame buffer to black
        ColorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

        ColorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        ColorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

        ColorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        ColorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        // Subpasses 
        VkAttachmentReference ColorAttachmentRef = {};
        ColorAttachmentRef.attachment = 0;
        ColorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkSubpassDescription Subpass = {};
        Subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;    // You need to be explicit that this is
                                                                        // a graphics subpass because we may support comput passes in the future
        Subpass.colorAttachmentCount = 1;
        Subpass.pColorAttachments = &ColorAttachmentRef;

        // Create the render pass
        VkRenderPassCreateInfo renderPassInfo = {};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.attachmentCount = 1;
        renderPassInfo.pAttachments = &ColorAttachment;
        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &Subpass;

        // Add a subpass dependency
        VkSubpassDependency dependency = {};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.srcAccessMask = 0;
        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        renderPassInfo.dependencyCount = 1;
        renderPassInfo.pDependencies = &dependency;

        if (vkCreateRenderPass(m_LogicalDevice->GetVkDevice(), &renderPassInfo, nullptr, &m_RenderPass) != VK_SUCCESS) 
        {
            F_LOG_FATAL("Failed to create render pass!");
        }
    }

    void Renderer::CreateDescriptorLayout()
    {
        // Create a binding for the UBO
        VkDescriptorSetLayoutBinding UboLayout = {};
        UboLayout.binding = 0;
        UboLayout.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        UboLayout.descriptorCount = 1;
        UboLayout.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        UboLayout.pImmutableSamplers = nullptr;

		// Image sampler layout binding
		VkDescriptorSetLayoutBinding samplerLayoutBinding = {};
		samplerLayoutBinding.binding = 1;
		samplerLayoutBinding.descriptorCount = 1;
		samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		samplerLayoutBinding.pImmutableSamplers = nullptr;
		samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

		std::array<VkDescriptorSetLayoutBinding, 2> bindings = { UboLayout, samplerLayoutBinding };

        VkDescriptorSetLayoutCreateInfo LayoutInfo = {};
        LayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        LayoutInfo.bindingCount = static_cast<UINT32>(bindings.size());
        LayoutInfo.pBindings = bindings.data();

        if(vkCreateDescriptorSetLayout(m_LogicalDevice->GetVkDevice(), &LayoutInfo, nullptr, &m_DescriptorSetLayout) != VK_SUCCESS)
        {
            F_LOG_FATAL("Failed to create descipror set layout!");
        }
    }

    void Renderer::CreateGraphicsPipeline()
    {
        // Load shaders
        std::shared_ptr<File> VertShaderCode = ResourceManager::LoadResource<File>("Shaders/vert.spv"_hs);
        assert(VertShaderCode);

        std::shared_ptr<File> FragShaderCode = ResourceManager::LoadResource<File>("Shaders/frag.spv"_hs);
        assert(FragShaderCode);

        // Create modules
        VkShaderModule VertModule = CreateShaderModule(VertShaderCode);
        VkShaderModule FragModule = CreateShaderModule(FragShaderCode);

        VkPipelineShaderStageCreateInfo VertShaderStageInfo = {};
        VertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        VertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
        VertShaderStageInfo.module = VertModule;
        VertShaderStageInfo.pName = "main";

        VkPipelineShaderStageCreateInfo FragShaderStageInfo = {};
        FragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        FragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        FragShaderStageInfo.module = FragModule;
        FragShaderStageInfo.pName = "main";

        VkPipelineShaderStageCreateInfo ShaderStages[] = { VertShaderStageInfo, FragShaderStageInfo };

        // Vertex input ----------------------
        VkVertexInputBindingDescription BindingDescription = Vertex::GetBindingDescription();
        std::array<VkVertexInputAttributeDescription, 3> AttributeDescriptions = Vertex::GetAttributeDescriptions();

        VkPipelineVertexInputStateCreateInfo VertexInputInfo = {};
        VertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        VertexInputInfo.vertexBindingDescriptionCount = 1;
        VertexInputInfo.pVertexBindingDescriptions = &BindingDescription; 
        VertexInputInfo.vertexAttributeDescriptionCount = static_cast<UINT32>(AttributeDescriptions.size());;
        VertexInputInfo.pVertexAttributeDescriptions = AttributeDescriptions.data();

        // Input Assembly ----------------------
        VkPipelineInputAssemblyStateCreateInfo InputAssembly = {};
        InputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        InputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        InputAssembly.primitiveRestartEnable = VK_FALSE;

        // Viewports and scissors ----------------------
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
        Rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        Rasterizer.depthClampEnable = VK_FALSE;
        Rasterizer.rasterizerDiscardEnable = VK_FALSE;  // Useful for shadow maps, using would require enabling a GPU feature
        Rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
        Rasterizer.lineWidth = 1.0f;

        Rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
        Rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE; // Specify the vertex order! 
        //Rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
        //Rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

        Rasterizer.depthBiasEnable = VK_FALSE;
        Rasterizer.depthBiasConstantFactor = 0.0f;  // Optional
        Rasterizer.depthBiasClamp = 0.0f;           // Optional
        Rasterizer.depthBiasSlopeFactor = 0.0f;     // Optional

        // Multi-sampling ----------------------------------
        // Can be a cheaper way to perform anti-aliasing
        // Using it requires enabling a GPU feature
        VkPipelineMultisampleStateCreateInfo Multisampling = {};
        Multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        Multisampling.sampleShadingEnable = VK_FALSE;
        Multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        Multisampling.minSampleShading = 1.0f; // Optional
        Multisampling.pSampleMask = nullptr; // Optional
        Multisampling.alphaToCoverageEnable = VK_FALSE; // Optional
        Multisampling.alphaToOneEnable = VK_FALSE; // Optional


        // Color blending ----------------------------------
        VkPipelineColorBlendAttachmentState ColorBlendAttachment = {};
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

        VkPipelineColorBlendStateCreateInfo ColorBlending = {};
        ColorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        ColorBlending.logicOpEnable = VK_FALSE;
        ColorBlending.logicOp = VK_LOGIC_OP_COPY;   // Optional
        ColorBlending.attachmentCount = 1;
        ColorBlending.pAttachments = &ColorBlendAttachment;
        ColorBlending.blendConstants[0] = 0.0f;     // Optional
        ColorBlending.blendConstants[1] = 0.0f;     // Optional
        ColorBlending.blendConstants[2] = 0.0f;     // Optional
        ColorBlending.blendConstants[3] = 0.0f;     // Optional

        // Pipeline layout ---------------------
        VkPipelineLayoutCreateInfo PipelineLayoutInfo = {};
        PipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        PipelineLayoutInfo.setLayoutCount = 1;
        PipelineLayoutInfo.pSetLayouts = &m_DescriptorSetLayout;
        PipelineLayoutInfo.pushConstantRangeCount = 0;      // Optional
        PipelineLayoutInfo.pPushConstantRanges = nullptr;   // Optional

        if (vkCreatePipelineLayout(m_LogicalDevice->GetVkDevice(), &PipelineLayoutInfo, nullptr, &m_PipelineLayout) != VK_SUCCESS) 
        {
            F_LOG_FATAL("Failed to create pipeline layout!");
        }

        // Create graphics pipeline ------------------------
        VkGraphicsPipelineCreateInfo pipelineInfo = {};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.stageCount = 2;
        pipelineInfo.pStages = ShaderStages;

        pipelineInfo.pVertexInputState = &VertexInputInfo;
        pipelineInfo.pInputAssemblyState = &InputAssembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &Rasterizer;
        pipelineInfo.pMultisampleState = &Multisampling;
        pipelineInfo.pDepthStencilState = nullptr; // Optional
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

        vkDestroyShaderModule(m_LogicalDevice->GetVkDevice(), FragModule, nullptr);
        vkDestroyShaderModule(m_LogicalDevice->GetVkDevice(), VertModule, nullptr);
    }

    void Renderer::CreateFrameBuffers()
    {
		assert(m_SwapChain);

        m_SwapChainFramebuffers.resize(m_SwapChain->GetImageViewCount());

		const std::vector<VkImageView>& ImageViews = m_SwapChain->GetImageViews();

        // Create the frame buffers based on the image views
        for (size_t i = 0; i < m_SwapChain->GetImageViewCount(); i++)
        {
            VkImageView attachments[] = 
            {
				ImageViews[i]
            };

            VkFramebufferCreateInfo framebufferInfo = {};
            framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebufferInfo.renderPass = m_RenderPass;
            framebufferInfo.attachmentCount = 1;
            framebufferInfo.pAttachments = attachments;
            framebufferInfo.width = m_SwapChain->GetExtents().width;
            framebufferInfo.height = m_SwapChain->GetExtents().height;
            framebufferInfo.layers = 1;

            if (vkCreateFramebuffer(m_LogicalDevice->GetVkDevice(), &framebufferInfo, nullptr, &m_SwapChainFramebuffers[i]) != VK_SUCCESS) 
            {
                F_LOG_FATAL("Failed to create framebuffer!");
            }
        }
    }

    void Renderer::CreateCommandPool()
    {
		UINT32 GraphicsFamily = m_LogicalDevice->GetGraphicsFamily();
        VkCommandPoolCreateInfo poolInfo = {};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.queueFamilyIndex = GraphicsFamily;
        poolInfo.flags = 0; 

        if (vkCreateCommandPool(m_LogicalDevice->GetVkDevice(), &poolInfo, nullptr, &m_CommandPool) != VK_SUCCESS) 
        {
            F_LOG_FATAL("Failed to create command pool!");
        }
    }

    void Renderer::CreateCommandBuffers()
    {
        assert(m_VertexBuffer && m_IndexBuffer);
        
        m_CommandBuffers.resize(m_SwapChainFramebuffers.size());
        // Create the command buffer
        VkCommandBufferAllocateInfo allocInfo = {};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = m_CommandPool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = (UINT32)m_CommandBuffers.size();

        if (vkAllocateCommandBuffers(m_LogicalDevice->GetVkDevice(), &allocInfo, m_CommandBuffers.data()) != VK_SUCCESS) 
        {
            F_LOG_FATAL("Failed to allocate command buffers!");
        }

        // Start command buffer recording
        for (size_t i = 0; i < m_CommandBuffers.size(); i++)
        {
            VkCommandBufferBeginInfo beginInfo = {};
            beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
            beginInfo.pInheritanceInfo = nullptr; 

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

            VkClearValue clearColor = {{ 0.0f, 0.0f, 0.0f, 1.0f }};
            renderPassInfo.clearValueCount = 1;
            renderPassInfo.pClearValues = &clearColor;

            vkCmdBeginRenderPass(m_CommandBuffers[i], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

            vkCmdBindPipeline(m_CommandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, m_GraphicsPipeline);

            VkBuffer VertexBuffers[] = { m_VertexBuffer->GetVkBuffer() };
            VkDeviceSize offsets[] = { 0 };
            vkCmdBindVertexBuffers(m_CommandBuffers[i], 0, 1, VertexBuffers, offsets);
			vkCmdBindIndexBuffer(m_CommandBuffers[i], m_IndexBuffer->GetVkBuffer(), 0, VK_INDEX_TYPE_UINT16);

            vkCmdBindDescriptorSets(m_CommandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, m_PipelineLayout, 0, 1, &m_DescriptorSets[i], 0, nullptr);
			vkCmdDrawIndexed(m_CommandBuffers[i], static_cast<UINT32>(Temp_indices.size()), 1, 0, 0, 0);

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

        VkSemaphoreCreateInfo semaphoreInfo = {};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkFenceCreateInfo fenceInfo = {};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        {
            if (vkCreateSemaphore(m_LogicalDevice->GetVkDevice(), &semaphoreInfo, nullptr, &m_ImageAvailableSemaphores[i]) != VK_SUCCESS ||
                vkCreateSemaphore(m_LogicalDevice->GetVkDevice(), &semaphoreInfo, nullptr, &m_RenderFinishedSemaphores[i]) != VK_SUCCESS ||
                vkCreateFence(m_LogicalDevice->GetVkDevice(), &fenceInfo, nullptr, &m_InFlightFences[i]) != VK_SUCCESS)
            {
                F_LOG_FATAL("Failed to create semaphores or fence!");
            }
        }
    }

    void Renderer::CleanupFrameResources()
    {
        for (size_t i = 0; i < m_SwapChainFramebuffers.size(); i++)
        {
            vkDestroyFramebuffer(m_LogicalDevice->GetVkDevice(), m_SwapChainFramebuffers[i], nullptr);
        }

        vkFreeCommandBuffers(m_LogicalDevice->GetVkDevice(), m_CommandPool, static_cast<UINT32>(m_CommandBuffers.size()), m_CommandBuffers.data());

        vkDestroyPipeline(m_LogicalDevice->GetVkDevice(), m_GraphicsPipeline, nullptr);
        vkDestroyPipelineLayout(m_LogicalDevice->GetVkDevice(), m_PipelineLayout, nullptr);
        vkDestroyRenderPass(m_LogicalDevice->GetVkDevice(), m_RenderPass, nullptr);

		m_SwapChain->Cleanup();

        // Cleanup uniform buffers -------------------------
		for (size_t i = 0; i < m_UniformBuffers.size(); ++i)
		{
			if (m_UniformBuffers[i])
			{
				delete m_UniformBuffers[i];
				m_UniformBuffers[i] = nullptr;
			}
		}
		m_UniformBuffers.clear();

        vkDestroyDescriptorPool(m_LogicalDevice->GetVkDevice(), m_DescriptorPool, nullptr);
    }

    void Renderer::RecreateFrameResources()
    {
		m_CurrentWindow->RecreateSwapChain();
		m_LogicalDevice->WaitForIdle();
        
        CleanupFrameResources();

		m_SwapChain->Recreate(ChooseSwapExtent());

        CreateRenderPass();
        CreateGraphicsPipeline();
        CreateFrameBuffers();

        CreateUniformBuffers();
        CreateDescriptorPool();
        CreateDescriptorSets();
        
        CreateCommandBuffers();
    }

    void Renderer::CreateTextureImage()
    {
        m_TestImage = ResourceManager::LoadResource<Image>("Textures/TestImage.jpg"_hs);
    }

    void Renderer::CreateVertexBuffer()
    {
		VkDeviceSize bufferSize = sizeof(Temp_Vertices[0]) * Temp_Vertices.size();
		
        Buffer StagingBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, Temp_Vertices.data());
        // Create the actual vertex buffer
        m_VertexBuffer = new Buffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

		// Copy the vertex buffer to the GPU memory
		Buffer::CopyBuffer(&StagingBuffer, m_VertexBuffer, bufferSize);
    }

	void Renderer::CreateIndexBuffer()
	{
		VkDeviceSize bufferSize = sizeof(Temp_indices[0]) * Temp_indices.size();
        Buffer StagingBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, Temp_indices.data());
		
        m_IndexBuffer = new Buffer(
            bufferSize, 
            VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, 
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
        );

		// Copy the staging buffer to the index buffer
		Buffer::CopyBuffer(&StagingBuffer, m_IndexBuffer, bufferSize);
	}

    void Renderer::CreateUniformBuffers()
    {
        VkDeviceSize bufferSize = sizeof(UniformBufferObject);

		const std::vector<VkImage>& Images = m_SwapChain->GetImages();

        m_UniformBuffers.resize(Images.size());

        for(size_t i = 0; i < Images.size(); ++i)
        {
			m_UniformBuffers[i] = new Buffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        }
    }

    void Renderer::CreateDescriptorPool()
    {
		std::array<VkDescriptorPoolSize, 2> PoolSizes = {};
		// UBO
		PoolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		PoolSizes[0].descriptorCount = static_cast<uint32_t>(m_SwapChain->GetImageCount());
		// Image smapler
		PoolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		PoolSizes[1].descriptorCount = static_cast<uint32_t>(m_SwapChain->GetImageCount());

        VkDescriptorPoolCreateInfo PoolInfo = {};
        PoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        PoolInfo.poolSizeCount = static_cast<UINT32>(PoolSizes.size());
        PoolInfo.pPoolSizes = PoolSizes.data();
        PoolInfo.maxSets = static_cast<UINT32>(m_SwapChain->GetImageCount());

        if(vkCreateDescriptorPool(m_LogicalDevice->GetVkDevice(), &PoolInfo, nullptr, &m_DescriptorPool) != VK_SUCCESS)
        {
            F_LOG_FATAL("Failed to create discriptor pool!");
        }
    }

    void Renderer::CreateDescriptorSets()
    {
		const std::vector<VkImage>& Images = m_SwapChain->GetImages();

        // Specify what descriptor pool to allocate from and how many
        std::vector<VkDescriptorSetLayout> layouts(Images.size(), m_DescriptorSetLayout);
        VkDescriptorSetAllocateInfo allocInfo = {};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = m_DescriptorPool;
        allocInfo.descriptorSetCount = static_cast<UINT32>(Images.size());
        allocInfo.pSetLayouts = layouts.data();

        m_DescriptorSets.resize(Images.size());
        // Sets will be cleaned up when the descriptor pool is, no need for an explicit free call in cleanup
        if(vkAllocateDescriptorSets(m_LogicalDevice->GetVkDevice(), &allocInfo, m_DescriptorSets.data()) != VK_SUCCESS)
        {
            F_LOG_FATAL("Failed to allocate descriptor sets!");
        }

        // Configure descriptor sets
        for (size_t i = 0; i < Images.size(); ++i)
        {
            VkDescriptorBufferInfo BufferInfo = {};
            BufferInfo.buffer = m_UniformBuffers[i]->GetVkBuffer();
            BufferInfo.offset = 0;
            BufferInfo.range = sizeof(UniformBufferObject);

			VkDescriptorImageInfo imageInfo = {};
			imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			imageInfo.imageView = m_TestImage->GetVkImageView();
			imageInfo.sampler = m_TestImage->GetSampler();

			std::array<VkWriteDescriptorSet, 2> descriptorWrites = {};

			descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			descriptorWrites[0].dstSet = m_DescriptorSets[i];
			descriptorWrites[0].dstBinding = 0;
			descriptorWrites[0].dstArrayElement = 0;
			descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			descriptorWrites[0].descriptorCount = 1;
			descriptorWrites[0].pBufferInfo = &BufferInfo;

			descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			descriptorWrites[1].dstSet = m_DescriptorSets[i];
			descriptorWrites[1].dstBinding = 1;
			descriptorWrites[1].dstArrayElement = 0;
			descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			descriptorWrites[1].descriptorCount = 1;
			descriptorWrites[1].pImageInfo = &imageInfo;

			vkUpdateDescriptorSets(m_LogicalDevice->GetVkDevice(), static_cast<UINT32>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
        }
    }

	// Swapchain support --------------------------------------

    SwapChainSupportDetails Renderer::QuerySwapChainSupport(VkPhysicalDevice t_Device)
    {
        SwapChainSupportDetails Details = {};

        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(t_Device, m_Surface, &Details.Capabilities);

        UINT32 FormatCount = 0;
        vkGetPhysicalDeviceSurfaceFormatsKHR(t_Device, m_Surface, &FormatCount, nullptr);
        if (FormatCount != 0)
        {
            Details.Formats.resize(FormatCount);
            vkGetPhysicalDeviceSurfaceFormatsKHR(t_Device, m_Surface, &FormatCount, Details.Formats.data());
        }

        UINT32 PresentModeCount = 0;
        vkGetPhysicalDeviceSurfacePresentModesKHR(t_Device, m_Surface, &PresentModeCount, nullptr);

        if (PresentModeCount != 0)
        {
            Details.PresentModes.resize(PresentModeCount);
            vkGetPhysicalDeviceSurfacePresentModesKHR(t_Device, m_Surface, &PresentModeCount, Details.PresentModes.data());
        }

        return Details;
    }

    VkSurfaceFormatKHR Renderer::ChooseSwapChainSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& t_AvailableFormats)
    {
        for (const VkSurfaceFormatKHR& Format : t_AvailableFormats)
        {
            if (Format.format == VK_FORMAT_B8G8R8A8_UNORM && Format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            {
                return Format;
            }
        }

        return t_AvailableFormats[0];
    }

    VkPresentModeKHR Renderer::ChooseSwapChainPresentMode(const std::vector<VkPresentModeKHR>& t_AvialableFormats)
    {
        VkPresentModeKHR BestMode = VK_PRESENT_MODE_FIFO_KHR;

        for (const VkPresentModeKHR& Mode : t_AvialableFormats)
        {
            if (Mode == VK_PRESENT_MODE_MAILBOX_KHR)
            {
                return Mode;
            }
            else if (Mode == VK_PRESENT_MODE_IMMEDIATE_KHR) 
            {
                BestMode = Mode;
            }
        }

        return BestMode;
    }

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

    VkShaderModule Renderer::CreateShaderModule(std::shared_ptr<File> t_ShaderCode)
    {
        VkShaderModuleCreateInfo CreateInfo = {};
        CreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        CreateInfo.codeSize = t_ShaderCode->GetFileLength();
        CreateInfo.pCode = reinterpret_cast<const UINT32*>(t_ShaderCode->GetData());

        VkShaderModule ShaderModule;
        if (vkCreateShaderModule(m_LogicalDevice->GetVkDevice(), &CreateInfo, nullptr, &ShaderModule) != VK_SUCCESS)
        {
            F_LOG_FATAL("Failed to create shader module!");
        }

        return ShaderModule;
    }

	void Renderer::CreateGameWindow( const UINT32 t_width, const UINT32 t_height )
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

        if(FlingConfig::GetBool("Engine", "DisplayVersionInfoInTitle", true))
        {
            Title += " // Version: " + Version::EngineVersion.ToString();
        }
		
        if(FlingConfig::GetBool("Engine", "DisplayBuildInfoInTitle", true))
        {
            Title += " // Built from " + (std::string)(GIT_BRANCH) + " @ " + GIT_COMMIT_HASH;
        }

		Props.m_Title = Title;

		m_CurrentWindow = FlingWindow::Create(Props);
	}

	void Renderer::Tick()
	{
		m_CurrentWindow->Update();
	}

    void Renderer::DrawFrame()
    {
        // Wait for the frame to be finished before beginning
        vkWaitForFences(m_LogicalDevice->GetVkDevice(), 1, &m_InFlightFences[CurrentFrameIndex], VK_TRUE, std::numeric_limits<uint64_t>::max());

		VkResult iResult = m_SwapChain->AquireNextImage(m_ImageAvailableSemaphores[CurrentFrameIndex]);
		UINT32  ImageIndex = m_SwapChain->GetActiveImageIndex();

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

        VkSubmitInfo submitInfo = {};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

        VkSemaphore waitSemaphores[] = { m_ImageAvailableSemaphores[CurrentFrameIndex] };
        VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = waitSemaphores;
        submitInfo.pWaitDstStageMask = waitStages;

        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &m_CommandBuffers[ImageIndex];

        VkSemaphore signalSemaphores[] = { m_RenderFinishedSemaphores[CurrentFrameIndex] };
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = signalSemaphores;

        vkResetFences(m_LogicalDevice->GetVkDevice(), 1, &m_InFlightFences[CurrentFrameIndex]);

        if (vkQueueSubmit(m_LogicalDevice->GetGraphicsQueue(), 1, &submitInfo, m_InFlightFences[CurrentFrameIndex]) != VK_SUCCESS)
        {
            F_LOG_FATAL("Failed to submit draw command buffer!");
        }

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
		float TimeSinceStart = Timing::Get().GetTimeSinceStart();
		float DeltaTime = Timing::Get().GetDeltaTime();

		m_camera->Update(DeltaTime);

		UniformBufferObject ubo = {};

		glm::mat4 model = glm::mat4(1.0f);
		//model = glm::translate(model, glm::vec3(0.0f, 0.0f, 1.0f));
		//model = glm::rotate(model, TimeSinceStart * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));

		ubo.Model = model;
		ubo.View = m_camera->GetViewMatrix();
		ubo.Proj = m_camera->GetProjectionMatrix();
		ubo.Proj[1][1] *= -1.0f;
		
		// Copy the ubo to the GPU
		void* data = nullptr;
		vkMapMemory(m_LogicalDevice->GetVkDevice(), m_UniformBuffers[t_CurrentImage]->GetVkDeviceMemory(), 0, sizeof(ubo), 0, &data);
		memcpy(data, &ubo, sizeof(ubo));
		vkUnmapMemory(m_LogicalDevice->GetVkDevice(), m_UniformBuffers[t_CurrentImage]->GetVkDeviceMemory());
    }	

	// Shutdown steps -------------------------------------------

    void Renderer::PrepShutdown()
    {
		m_LogicalDevice->WaitForIdle();
		// You have to free images before before the renderer gets shutdown because they need to the 
		// physical device free their VK resources
		if (m_TestImage)
		{
			m_TestImage.reset();
		}
    }

	void Renderer::Shutdown()
	{
		// Cleanup Vulkan ------
        CleanupFrameResources();

		if (m_SwapChain)
		{
			delete m_SwapChain;
			m_SwapChain = nullptr;
		}

        vkDestroyDescriptorSetLayout(m_LogicalDevice->GetVkDevice(), m_DescriptorSetLayout, nullptr);

        if(m_IndexBuffer)
        {
            delete m_IndexBuffer;
            m_IndexBuffer = nullptr;
        }

        if(m_VertexBuffer)
        {
            delete m_VertexBuffer;
            m_VertexBuffer = nullptr;
        }

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) 
        {
            vkDestroySemaphore(m_LogicalDevice->GetVkDevice(), m_RenderFinishedSemaphores[i], nullptr);
            vkDestroySemaphore(m_LogicalDevice->GetVkDevice(), m_ImageAvailableSemaphores[i], nullptr);
            vkDestroyFence(m_LogicalDevice->GetVkDevice(), m_InFlightFences[i], nullptr);
        }

        vkDestroyCommandPool(m_LogicalDevice->GetVkDevice(), m_CommandPool, nullptr);

        if(m_LogicalDevice)
        {
            delete m_LogicalDevice;
            m_LogicalDevice = nullptr;
        }

        vkDestroySurfaceKHR (m_Instance->GetRawVkInstance(), m_Surface, nullptr);

        if(m_PhysicalDevice)
        {
            delete m_PhysicalDevice;
            m_PhysicalDevice = nullptr;
        }

        if(m_Instance)
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