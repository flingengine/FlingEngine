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

		m_camera = std::make_unique<FirstPersonCamera>(m_CurrentWindow->GetAspectRatio());
	}

    UINT16 Renderer::GetDeviceRating( VkPhysicalDevice t_Device )
    {
        UINT16 Score = 0;

        VkPhysicalDeviceProperties DeviceProperties;
        VkPhysicalDeviceFeatures DeviceFeatures;
        vkGetPhysicalDeviceProperties( t_Device, &DeviceProperties );
        vkGetPhysicalDeviceFeatures( t_Device, &DeviceFeatures );

        // Necessary application features, if the device doesn't have one then return 0
        if( !DeviceFeatures.geometryShader )
        {
            return 0;
        }

        // Ensure that this devices supports all necessary extensions
        if (!CheckDeviceExtensionSupport(t_Device))
        {
            return 0;
        }

        // This device must have swap chain support
        SwapChainSupportDetails SwapChainSupport = QuerySwapChainSupport(t_Device);
        if (SwapChainSupport.Formats.empty() || SwapChainSupport.PresentModes.empty())
        {
            return 0;
        }

        // Favor discrete GPU's
        if( DeviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU )
        {
            Score += 500;
        }

        // Favor complete queue sets
        QueueFamilyIndices QueueFamily = FindQueueFamilies( t_Device );
        if( QueueFamily.IsComplete() )
        {
            Score += 500;
        }

        return Score;
    }

    QueueFamilyIndices Renderer::FindQueueFamilies( VkPhysicalDevice const t_Device )
    {
        QueueFamilyIndices Indecies = {};

        UINT32 QueueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties( t_Device, &QueueFamilyCount, nullptr );

        std::vector<VkQueueFamilyProperties> QueueFamilies( QueueFamilyCount );
        vkGetPhysicalDeviceQueueFamilyProperties( t_Device, &QueueFamilyCount, QueueFamilies.data() );
        // Set the family flags we are interested in
        int i = 0;
        for( const VkQueueFamilyProperties& Family : QueueFamilies )
        {
            if (Family.queueCount > 0)
            {
                Indecies.GraphicsFamily = ((Family.queueFlags & VK_QUEUE_GRAPHICS_BIT) ? i : 0);
                
                VkBool32 PresentSupport = false;
                vkGetPhysicalDeviceSurfaceSupportKHR(t_Device, i, m_Surface, &PresentSupport);

                Indecies.PresentFamily = PresentSupport ? i : 0;
            }
                    
            if( Indecies.IsComplete() )
            {
                break;
            }
            i++;
        }

        return Indecies;
    }

    void Renderer::InitGraphics()
	{
		ReadConfig();

		CreateGraphicsInstance();
		SetupDebugMessesages();
        CreateSurface();
		PickPhysicalDevice();
        CreateLogicalDevice();
        CreateSwapChain();
        CreateImageViews();
        CreateRenderPass();
        CreateDescriptorLayout();
        CreateGraphicsPipeline();
        CreateFrameBuffers();
        CreateCommandPool();
        
        CreateTextureImage();

		CreateVertexBuffer();
		CreateIndexBuffer();
        CreateUniformBuffers();
        CreateDescriptorPool();
        CreateDescriptorSets();

        CreateCommandBuffers();
        CreateSyncObjects();
	}

	void Renderer::ReadConfig()
	{
		m_EnableValidationLayers = FlingConfig::GetBool("Vulkan", "EnableValidationLayers");
		F_LOG_TRACE("[Renderer] m_EnableValidationLayers is {}", m_EnableValidationLayers);
	}

	void Renderer::CreateGraphicsInstance()
	{
		if( m_EnableValidationLayers && !CheckValidationLayerSupport() )
		{
            F_LOG_FATAL( "Validation layers are requested, but not available!" );
		}

		// Basic app data that we can modify 
		VkApplicationInfo appInfo = {};
		// Most structs in Vulkan require you to specify the sType and 
		// the pNext values 
		appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		appInfo.pApplicationName = "Engine Bay 0.1";
		appInfo.applicationVersion = VK_MAKE_VERSION(Version::EngineVersion.Major, Version::EngineVersion.Minor, Version::EngineVersion.Patch);
		appInfo.pEngineName = "Fling Engine";
		appInfo.engineVersion = VK_MAKE_VERSION(Version::EngineVersion.Major, Version::EngineVersion.Minor, Version::EngineVersion.Patch);
		appInfo.apiVersion = VK_API_VERSION_1_0;

		// Instance creation info, similar to how DX11 worked
		VkInstanceCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		createInfo.pApplicationInfo = &appInfo;

		// Because we are using GLFW as an extension interface for window creation
		// Vulkan needs to know how many extensions are available
		std::vector<const char*> extensions = GetRequiredExtensions();
		createInfo.enabledExtensionCount = static_cast<uint32_t>( extensions.size() );
		createInfo.ppEnabledExtensionNames = extensions.data();

		// Determine global validation layer count
		if( m_EnableValidationLayers )
		{
			createInfo.enabledLayerCount = static_cast<UINT32>( m_ValidationLayers.size() );
			createInfo.ppEnabledLayerNames = m_ValidationLayers.data();
		}
		else 
		{
			createInfo.enabledLayerCount = 0;
			createInfo.pNext = nullptr;
		}

		// Create the vulkan instance! Throw exception if it fails
		if( vkCreateInstance( &createInfo, nullptr, &m_Instance ) != VK_SUCCESS )
		{
            F_LOG_FATAL( "Failed to create the Vulkan instance!" );
		}

		// Find out what extensions are available
		uint32_t extensionCount = 0;
		vkEnumerateInstanceExtensionProperties( nullptr, &extensionCount, nullptr );
		std::vector<VkExtensionProperties> vk_extensions( extensionCount );
		vkEnumerateInstanceExtensionProperties( nullptr, &extensionCount, vk_extensions.data() );

		F_LOG_TRACE( "{} Extensions are supported!", extensionCount );
		for( const auto& extension : vk_extensions )
		{
			F_LOG_TRACE( "\t{}", extension.extensionName );
		}
	}

	bool Renderer::CheckValidationLayerSupport()
	{
		// Get the list of available validation layers
		// Validation layers are ways for us to choose what types of debugging
		// features we want from the Vulkan SDK
		UINT32 layerCount = 0;
		vkEnumerateInstanceLayerProperties( &layerCount, nullptr );

		std::vector<VkLayerProperties> availableLayers( layerCount );
		vkEnumerateInstanceLayerProperties( &layerCount, availableLayers.data() );

		for( const char* layerName : m_ValidationLayers )
		{
			bool layerFound = false;
			for( const VkLayerProperties& layerProperties : availableLayers )
			{
				if( strcmp( layerName, layerProperties.layerName ) == 0 )
				{
					layerFound = true;
					break;
				}
			}
			if( !layerFound )
			{
				return false;
			}
		}

		return true;
	}

	void Renderer::PickPhysicalDevice()
	{
        // Enumerate available devices
		UINT32 DeviceCount = 0;
		vkEnumeratePhysicalDevices( m_Instance, &DeviceCount, nullptr );

		if( DeviceCount == 0 )
		{
            F_LOG_FATAL( "Failed to find GPU's with Vulkan support!" );
		}

        std::vector<VkPhysicalDevice> Devices( DeviceCount );
        vkEnumeratePhysicalDevices( m_Instance, &DeviceCount, Devices.data() );
        
        // Find the best device for this application
        UINT16 MaxRating = 0;
        for( const VkPhysicalDevice& Device : Devices )
        {
            UINT16 Rating = GetDeviceRating( Device );
            if( Rating > 0 && Rating > MaxRating )
            {
                MaxRating = Rating;
                m_PhysicalDevice = Device;
            }
        }

        if( m_PhysicalDevice == VK_NULL_HANDLE )
        {
            F_LOG_FATAL( "Failed to find a suitable GPU!" );
        }
	}

    void Renderer::CreateLogicalDevice()
    {
        // Queue creation
        QueueFamilyIndices Indecies = FindQueueFamilies( m_PhysicalDevice );
  
        std::vector<VkDeviceQueueCreateInfo> QueueCreateInfos;
        std::set<UINT32> UniqueQueueFamilies = { Indecies.GraphicsFamily, Indecies.PresentFamily };

        // Generate the CreatinInfo for each queue family 
        float Priority = 1.0f;
        for (const UINT32 fam : UniqueQueueFamilies)
        {
            VkDeviceQueueCreateInfo QueueCreateInfo = {};
            QueueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            QueueCreateInfo.queueFamilyIndex = fam;
            QueueCreateInfo.queueCount = 1;
            QueueCreateInfo.pQueuePriorities = &Priority;
            QueueCreateInfos.push_back(QueueCreateInfo);
        }

        VkPhysicalDeviceFeatures DevicesFeatures = {};

        // Device creation 
        VkDeviceCreateInfo CreateInfo = {};
        CreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        CreateInfo.queueCreateInfoCount = static_cast<UINT32>(QueueCreateInfos.size());
        CreateInfo.pQueueCreateInfos = QueueCreateInfos.data();
        CreateInfo.pEnabledFeatures = &DevicesFeatures;

        // Set the enabled extensions
        CreateInfo.enabledExtensionCount = static_cast<UINT32>(m_DeviceExtensions.size());
        CreateInfo.ppEnabledExtensionNames = m_DeviceExtensions.data();

        if( m_EnableValidationLayers ) 
        {
            CreateInfo.enabledLayerCount = static_cast<UINT32>( m_ValidationLayers.size() );
            CreateInfo.ppEnabledLayerNames = m_ValidationLayers.data();
        }
        else 
        {
            CreateInfo.enabledLayerCount = 0;
        }

        if( vkCreateDevice( m_PhysicalDevice, &CreateInfo, nullptr, &m_Device ) != VK_SUCCESS ) 
        {
            F_LOG_FATAL( "failed to create logical Device!" );
        }

        vkGetDeviceQueue( m_Device, Indecies.GraphicsFamily, 0, &m_GraphicsQueue );
        vkGetDeviceQueue(m_Device, Indecies.PresentFamily, 0, &m_PresentQueue);
    }

	void Renderer::SetupDebugMessesages()
	{
		if( !m_EnableValidationLayers ) { return; }

		VkDebugUtilsMessengerCreateInfoEXT createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
		createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
		createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
		createInfo.pfnUserCallback = DebugCallback;
		createInfo.pUserData = nullptr; // Optional
		
		if( CreateDebugUtilsMessengerEXT( m_Instance, &createInfo, nullptr, &m_DebugMessenger ) != VK_SUCCESS )
		{
            F_LOG_FATAL( "Failed to set up debug messenger!" );
		}
	}

    void Renderer::CreateSurface()
    {
		m_CurrentWindow->CreateSurface(m_Instance, &m_Surface);
    }

    void Renderer::CreateSwapChain()
    {
        SwapChainSupportDetails SwapChainSupport = QuerySwapChainSupport(m_PhysicalDevice);

        VkSurfaceFormatKHR SwapChainSurfaceFormat = ChooseSwapChainSurfaceFormat(SwapChainSupport.Formats);
        VkPresentModeKHR PresentMode = ChooseSwapChainPresentMode(SwapChainSupport.PresentModes);
        m_SwapChainExtents = ChooseSwapExtent(SwapChainSupport.Capabilities);
        m_SwapChainImageFormat = SwapChainSurfaceFormat.format;

        // Use one more than the minimum image count so that we don't have to wait for the 
        // driver to finish some internal things before we start sending another image
        UINT32 ImageCount = SwapChainSupport.Capabilities.minImageCount + 1;

        // Check that we don't exceed the max image count
        if (SwapChainSupport.Capabilities.maxImageCount > 0 && ImageCount > SwapChainSupport.Capabilities.maxImageCount)
        {
            ImageCount = SwapChainSupport.Capabilities.maxImageCount;
        }

        VkSwapchainCreateInfoKHR CreateInfo = {};
        CreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        CreateInfo.surface = m_Surface;
        CreateInfo.minImageCount = ImageCount;
        CreateInfo.imageFormat = SwapChainSurfaceFormat.format;
        CreateInfo.imageColorSpace = SwapChainSurfaceFormat.colorSpace;
        CreateInfo.imageExtent = m_SwapChainExtents;
        CreateInfo.imageArrayLayers = 1;
        CreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

        // Specify the handling of multiple queue families
        QueueFamilyIndices Indices = FindQueueFamilies(m_PhysicalDevice);
        UINT32 queueFamilyIndices[] = { Indices.GraphicsFamily, Indices.PresentFamily };

        if (Indices.GraphicsFamily != Indices.PresentFamily) 
        {
            CreateInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            CreateInfo.queueFamilyIndexCount = 2;
            CreateInfo.pQueueFamilyIndices = queueFamilyIndices;
        }
        else 
        {
            CreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
            CreateInfo.queueFamilyIndexCount = 0; // Optional
            CreateInfo.pQueueFamilyIndices = nullptr; // Optional
        }

        // Transparency settings of this swap chain
        CreateInfo.preTransform = SwapChainSupport.Capabilities.currentTransform;
        CreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        CreateInfo.presentMode = PresentMode;
        CreateInfo.clipped = VK_TRUE;
        CreateInfo.oldSwapchain = VK_NULL_HANDLE;

        if (vkCreateSwapchainKHR(m_Device, &CreateInfo, nullptr, &m_SwapChain) != VK_SUCCESS)
        {
            F_LOG_FATAL("Failed to create swap chain!");
        }

        // Get handles to the swap chain images
        vkGetSwapchainImagesKHR(m_Device, m_SwapChain, &ImageCount, nullptr);
        m_SwapChainImages.resize(ImageCount);
        vkGetSwapchainImagesKHR(m_Device, m_SwapChain, &ImageCount, m_SwapChainImages.data());
    }

    void Renderer::CreateImageViews()
    {
        m_SwapChainImageViews.resize(m_SwapChainImages.size());
        for (size_t i = 0; i < m_SwapChainImages.size(); i++)
        {
            VkImageViewCreateInfo createInfo = {};
            createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            createInfo.image = m_SwapChainImages[i];

            createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;    // use 3D for cubemaps
            createInfo.format = m_SwapChainImageFormat;

            // Map all color channels to their defaults
            createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

            createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            createInfo.subresourceRange.baseMipLevel = 0;
            createInfo.subresourceRange.levelCount = 1;
            createInfo.subresourceRange.baseArrayLayer = 0;
            createInfo.subresourceRange.layerCount = 1;

            if (vkCreateImageView(m_Device, &createInfo, nullptr, &m_SwapChainImageViews[i]) != VK_SUCCESS)
            {
                F_LOG_FATAL("Failed to create image views!");
            }
        }
    }

    void Renderer::CreateRenderPass()
    {
        // We have a single color buffer for the images in the swap chain
        VkAttachmentDescription ColorAttachment = {};
        ColorAttachment.format = m_SwapChainImageFormat;
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

        if (vkCreateRenderPass(m_Device, &renderPassInfo, nullptr, &m_RenderPass) != VK_SUCCESS) 
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

        VkDescriptorSetLayoutCreateInfo LayoutInfo = {};
        LayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        LayoutInfo.bindingCount = 1;
        LayoutInfo.pBindings = &UboLayout;

        if(vkCreateDescriptorSetLayout(m_Device, &LayoutInfo, nullptr, &m_DescriptorSetLayout) != VK_SUCCESS)
        {
            F_LOG_FATAL("Failed to create descipror set layout!");
        }
    }

    void Renderer::CreateGraphicsPipeline()
    {
        // Load shaders
        // #TODO Create a way to re-compile shaders in-engine at runtime
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
        std::array<VkVertexInputAttributeDescription, 2> AttributeDescriptions = Vertex::GetAttributeDescriptions();

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
        viewport.width = (float)m_SwapChainExtents.width;       // These values can differ from the width/height of the window!
        viewport.height = (float)m_SwapChainExtents.height;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;

        VkRect2D scissor = {};
        scissor.offset = { 0, 0 };
        scissor.extent = m_SwapChainExtents;

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

        if (vkCreatePipelineLayout(m_Device, &PipelineLayoutInfo, nullptr, &m_PipelineLayout) != VK_SUCCESS) 
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

        if (vkCreateGraphicsPipelines(m_Device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_GraphicsPipeline) != VK_SUCCESS)
        {
            F_LOG_FATAL("failed to create graphics pipeline!");
        }

        vkDestroyShaderModule(m_Device, FragModule, nullptr);
        vkDestroyShaderModule(m_Device, VertModule, nullptr);
    }

    void Renderer::CreateFrameBuffers()
    {
        m_SwapChainFramebuffers.resize(m_SwapChainImageViews.size());

        // Create the frame buffers basedon the image views
        for (size_t i = 0; i < m_SwapChainImageViews.size(); i++) 
        {
            VkImageView attachments[] = 
            {
                m_SwapChainImageViews[i]
            };

            VkFramebufferCreateInfo framebufferInfo = {};
            framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebufferInfo.renderPass = m_RenderPass;
            framebufferInfo.attachmentCount = 1;
            framebufferInfo.pAttachments = attachments;
            framebufferInfo.width = m_SwapChainExtents.width;
            framebufferInfo.height = m_SwapChainExtents.height;
            framebufferInfo.layers = 1;

            if (vkCreateFramebuffer(m_Device, &framebufferInfo, nullptr, &m_SwapChainFramebuffers[i]) != VK_SUCCESS) 
            {
                F_LOG_FATAL("Failed to create framebuffer!");
            }
        }
    }

    void Renderer::CreateCommandPool()
    {
        QueueFamilyIndices queueFamilyIndices = FindQueueFamilies(m_PhysicalDevice);

        VkCommandPoolCreateInfo poolInfo = {};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.queueFamilyIndex = queueFamilyIndices.GraphicsFamily;
        poolInfo.flags = 0; 

        if (vkCreateCommandPool(m_Device, &poolInfo, nullptr, &m_CommandPool) != VK_SUCCESS) 
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

        if (vkAllocateCommandBuffers(m_Device, &allocInfo, m_CommandBuffers.data()) != VK_SUCCESS) 
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
            renderPassInfo.renderArea.extent = m_SwapChainExtents;

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
            if (vkCreateSemaphore(m_Device, &semaphoreInfo, nullptr, &m_ImageAvailableSemaphores[i]) != VK_SUCCESS ||
                vkCreateSemaphore(m_Device, &semaphoreInfo, nullptr, &m_RenderFinishedSemaphores[i]) != VK_SUCCESS ||
                vkCreateFence(m_Device, &fenceInfo, nullptr, &m_InFlightFences[i]) != VK_SUCCESS)
            {
                F_LOG_FATAL("Failed to create semaphores or fence!");
            }
        }
    }

    void Renderer::CleanUpSwapChain()
    {
        for (size_t i = 0; i < m_SwapChainFramebuffers.size(); i++) 
        {
            vkDestroyFramebuffer(m_Device, m_SwapChainFramebuffers[i], nullptr);
        }

        vkFreeCommandBuffers(m_Device, m_CommandPool, static_cast<UINT32>(m_CommandBuffers.size()), m_CommandBuffers.data());

        vkDestroyPipeline(m_Device, m_GraphicsPipeline, nullptr);
        vkDestroyPipelineLayout(m_Device, m_PipelineLayout, nullptr);
        vkDestroyRenderPass(m_Device, m_RenderPass, nullptr);

        for (size_t i = 0; i < m_SwapChainImageViews.size(); i++) 
        {
            vkDestroyImageView(m_Device, m_SwapChainImageViews[i], nullptr);
        }

        vkDestroySwapchainKHR(m_Device, m_SwapChain, nullptr);

        // Cleanup uniform buffers -------------------------
        for (size_t i = 0; i < m_SwapChainImages.size(); i++)
        {
            vkDestroyBuffer(m_Device, m_UniformBuffers[i], nullptr);
            vkFreeMemory(m_Device, m_UniformBuffersMemory[i], nullptr);
        }

        vkDestroyDescriptorPool(m_Device, m_DescriptorPool, nullptr);
    }

    void Renderer::RecreateSwapChain()
    {
		m_CurrentWindow->RecreateSwapChain();

        vkDeviceWaitIdle(m_Device);

        CleanUpSwapChain();

        CreateSwapChain();
        CreateImageViews();
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
        //std::shared_ptr<Image> TestImage = ResourceManager::LoadResource<Image>("Textures/TestImage.jpg"_hs, m_Device, m_PhysicalDevice);

    }

    void Renderer::CreateVertexBuffer()
    {
        F_LOG_TRACE("Create vertex buffer!");
		VkDeviceSize bufferSize = sizeof(Temp_Vertices[0]) * Temp_Vertices.size();
		
        Buffer StagingBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, Temp_Vertices.data());
        F_LOG_TRACE("Create staging buffer : Complete");
        // Create the actual vertex buffer
        m_VertexBuffer = new Buffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		//GraphicsHelpers::CreateBuffer(m_Device, m_PhysicalDevice,bufferSize,
		//	VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
		//	VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		//	m_VertexBuffer,
		//	m_VertexBufferMemory);

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

        m_UniformBuffers.resize(m_SwapChainImages.size());
        m_UniformBuffersMemory.resize(m_SwapChainImages.size());

        for(size_t i = 0; i < m_SwapChainImages.size(); ++i)
        {
            GraphicsHelpers::CreateBuffer(m_Device, m_PhysicalDevice,
                bufferSize,
                VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, 
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                m_UniformBuffers[i],
                m_UniformBuffersMemory[i]
            );
        }
    }

    void Renderer::CreateDescriptorPool()
    {
        VkDescriptorPoolSize PoolSize = {};
        PoolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        PoolSize.descriptorCount = static_cast<UINT32>(m_SwapChainImages.size());

        VkDescriptorPoolCreateInfo PoolInfo = {};
        PoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        PoolInfo.poolSizeCount = 1;
        PoolInfo.pPoolSizes = &PoolSize;

        PoolInfo.maxSets = static_cast<UINT32>(m_SwapChainImages.size());

        if(vkCreateDescriptorPool(m_Device, &PoolInfo, nullptr, &m_DescriptorPool) != VK_SUCCESS)
        {
            F_LOG_FATAL("Failed to create discriptor pool!");
        }
    }

    void Renderer::CreateDescriptorSets()
    {
        // Specify what descriptor pool to allocate from and how many
        std::vector<VkDescriptorSetLayout> layouts(m_SwapChainImages.size(), m_DescriptorSetLayout);
        VkDescriptorSetAllocateInfo allocInfo = {};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = m_DescriptorPool;
        allocInfo.descriptorSetCount = static_cast<UINT32>(m_SwapChainImages.size());
        allocInfo.pSetLayouts = layouts.data();

        m_DescriptorSets.resize(m_SwapChainImages.size());
        // Sets will be cleaned up when the descriptor pool is, no need for an explict free call in cleanup
        if(vkAllocateDescriptorSets(m_Device, &allocInfo, m_DescriptorSets.data()) != VK_SUCCESS)
        {
            F_LOG_FATAL("Failed to allocate descriptor sets!");
        }

        // Configure descriptor sets
        for (size_t i = 0; i < m_SwapChainImages.size(); ++i) 
        {
            VkDescriptorBufferInfo BufferInfo = {};
            BufferInfo.buffer = m_UniformBuffers[i];
            BufferInfo.offset = 0;
            BufferInfo.range = sizeof(UniformBufferObject);

            VkWriteDescriptorSet descriptorWrite = {};
            descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrite.dstSet = m_DescriptorSets[i];
            descriptorWrite.dstBinding = 0;
            descriptorWrite.dstArrayElement = 0;

            descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            descriptorWrite.descriptorCount = 1;
            descriptorWrite.pBufferInfo = &BufferInfo;
            descriptorWrite.pImageInfo = nullptr;
            descriptorWrite.pTexelBufferView = nullptr;

            vkUpdateDescriptorSets(m_Device, 1, &descriptorWrite, 0, nullptr);
        }
    }

	void Renderer::CopyBuffer(VkBuffer t_SrcBuffer, VkBuffer t_DstBuffer, VkDeviceSize t_Size)
	{
		VkCommandBuffer commandBuffer = GraphicsHelpers::BeginSingleTimeCommands(m_Device, m_CommandPool);

		VkBufferCopy copyRegion = {};
		copyRegion.srcOffset = 0; // Optional
		copyRegion.dstOffset = 0; // Optional
		copyRegion.size = t_Size;
		vkCmdCopyBuffer(commandBuffer, t_SrcBuffer, t_DstBuffer, 1, &copyRegion);

		GraphicsHelpers::EndSingleTimeCommands(commandBuffer);
	}

	void Renderer::TransitionImageLayout(VkImage t_Image, VkFormat t_Format, VkImageLayout t_oldLayout, VkImageLayout t_NewLayout)
	{
		VkCommandBuffer commandBuffer = GraphicsHelpers::BeginSingleTimeCommands(m_Device, m_CommandPool);

		VkImageMemoryBarrier barrier = {};
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.oldLayout = t_oldLayout;
		barrier.newLayout = t_NewLayout;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

		barrier.image = t_Image;
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		barrier.subresourceRange.baseMipLevel = 0;
		barrier.subresourceRange.levelCount = 1;
		barrier.subresourceRange.baseArrayLayer = 0;
		barrier.subresourceRange.layerCount = 1;

		barrier.srcAccessMask = 0; // TODO
		barrier.dstAccessMask = 0; // TODO

		vkCmdPipelineBarrier(
			commandBuffer,
			0 /* TODO */, 0 /* TODO */,
			0,
			0, nullptr,
			0, nullptr,
			1, &barrier
		);

		GraphicsHelpers::EndSingleTimeCommands(commandBuffer);
	}

	void Renderer::CopyBufferToImage(VkBuffer t_Buffer, VkImage t_Image, UINT32 t_Width, UINT32 t_Height)
	{
		VkCommandBuffer commandBuffer = GraphicsHelpers::BeginSingleTimeCommands(m_Device, m_CommandPool);

		VkBufferImageCopy region = {};
		region.bufferOffset = 0;
		region.bufferRowLength = 0;
		region.bufferImageHeight = 0;

		region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		region.imageSubresource.mipLevel = 0;
		region.imageSubresource.baseArrayLayer = 0;
		region.imageSubresource.layerCount = 1;

		region.imageOffset = { 0, 0, 0 };
		region.imageExtent = {
			t_Width,
			t_Height,
			1
		};

		vkCmdCopyBufferToImage(
			commandBuffer,
			t_Buffer,
			t_Image,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			1,
			&region
		);

		GraphicsHelpers::EndSingleTimeCommands(commandBuffer);
	}

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

    VkExtent2D Renderer::ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& t_Capabilies)
    {
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

    std::vector<const char*> Renderer::GetRequiredExtensions()
	{
		UINT32 glfwExtensionCount = 0;
		const char** glfwExtensions;
		glfwExtensions = glfwGetRequiredInstanceExtensions( &glfwExtensionCount );

		std::vector<const char*> extensions( glfwExtensions, glfwExtensions + glfwExtensionCount );

		if( m_EnableValidationLayers ) 
		{
			extensions.push_back( VK_EXT_DEBUG_UTILS_EXTENSION_NAME );
		}

		return extensions;
	}

    bool Renderer::CheckDeviceExtensionSupport(VkPhysicalDevice t_Device)
    {
        UINT32 ExtensionCount = 0;

        // Determine how many extensions are available
        vkEnumerateDeviceExtensionProperties(t_Device, nullptr, &ExtensionCount, nullptr);
        std::vector<VkExtensionProperties> AvailableExtensions(ExtensionCount);
        vkEnumerateDeviceExtensionProperties(t_Device, nullptr, &ExtensionCount, AvailableExtensions.data());

        std::set<std::string> RequiredExtensions(m_DeviceExtensions.begin(), m_DeviceExtensions.end());

        for (const VkExtensionProperties& Extension : AvailableExtensions)
        {
            RequiredExtensions.erase(Extension.extensionName);
        }

        return RequiredExtensions.empty();
    }

    VkShaderModule Renderer::CreateShaderModule(std::shared_ptr<File> t_ShaderCode)
    {
        VkShaderModuleCreateInfo CreateInfo = {};
        CreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        CreateInfo.codeSize = t_ShaderCode->GetFileLength();
        CreateInfo.pCode = reinterpret_cast<const UINT32*>(t_ShaderCode->GetData());

        VkShaderModule ShaderModule;
        if (vkCreateShaderModule(m_Device, &CreateInfo, nullptr, &ShaderModule) != VK_SUCCESS)
        {
            F_LOG_FATAL("Failed to create shader module!");
        }

        return ShaderModule;
    }

    VKAPI_ATTR VkBool32 VKAPI_CALL Renderer::DebugCallback(
		VkDebugUtilsMessageSeverityFlagBitsEXT t_messageSeverity, 
		VkDebugUtilsMessageTypeFlagsEXT t_messageType, 
		const VkDebugUtilsMessengerCallbackDataEXT* t_CallbackData, 
		void* t_UserData )
	{
		switch( t_messageSeverity )
		{
		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
			F_LOG_WARN( "Validation layer: {}", t_CallbackData->pMessage );
			break;
		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_FLAG_BITS_MAX_ENUM_EXT:
			F_LOG_ERROR( "Validation layer error: {}", t_CallbackData->pMessage );
			break;
		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
		default:
			F_LOG_TRACE( "Validation layer: {}", t_CallbackData->pMessage );
			break;
		}

		return VK_FALSE;
	}

	VkResult Renderer::CreateDebugUtilsMessengerEXT( VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger )
	{
		PFN_vkCreateDebugUtilsMessengerEXT func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr( instance, "vkCreateDebugUtilsMessengerEXT" );
		if( func != nullptr ) 
		{
			return func( instance, pCreateInfo, pAllocator, pDebugMessenger );
		}
		else 
		{
			return VK_ERROR_EXTENSION_NOT_PRESENT;
		}
	}

	void Renderer::DestroyDebugUtilsMessengerEXT( VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator )
	{
		PFN_vkDestroyDebugUtilsMessengerEXT func = 
			(PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr( instance, "vkDestroyDebugUtilsMessengerEXT" );
		if( func != nullptr ) 
		{
			func( instance, m_DebugMessenger, pAllocator );
		}
	}

    void Renderer::FrameBufferResizeCallback(FlingWindow* t_Window, int t_Width, int t_Height)
    {
        Renderer::Get().m_FrameBufferResized = true;
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

		// #TODO: Add a resize callback to the window!

		//m_Window = glfwCreateWindow( m_WindowWidth, m_WindowHeight, Title.c_str(), nullptr, nullptr );
		//DesktopWindow* Window = static_cast<DesktopWindow*>(m_CurrentWindow);
		
        //glfwSetFramebufferSizeCallback(m_Window, &FrameBufferResizeCallback);
	}

	void Renderer::Tick()
	{
		m_CurrentWindow->Update();
	}

    void Renderer::DrawFrame()
    {
        // Wait for the frame to be finished before beginning
        vkWaitForFences(m_Device, 1, &m_InFlightFences[CurrentFrameIndex], VK_TRUE, std::numeric_limits<uint64_t>::max());

        UINT32 ImageIndex;
        VkResult iResult = vkAcquireNextImageKHR(m_Device, m_SwapChain, std::numeric_limits<uint64_t>::max(), m_ImageAvailableSemaphores[CurrentFrameIndex], VK_NULL_HANDLE, &ImageIndex);

        // Check if the swap chain needs to be recreated
        if (iResult == VK_ERROR_OUT_OF_DATE_KHR)
        {
            RecreateSwapChain();
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

        vkResetFences(m_Device, 1, &m_InFlightFences[CurrentFrameIndex]);

        if (vkQueueSubmit(m_GraphicsQueue, 1, &submitInfo, m_InFlightFences[CurrentFrameIndex]) != VK_SUCCESS)
        {
            F_LOG_FATAL("Failed to submit draw command buffer!");
        }

        // Presentation
        VkPresentInfoKHR presentInfo = {};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = signalSemaphores;

        VkSwapchainKHR swapChains[] = { m_SwapChain };
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = swapChains;
        presentInfo.pImageIndices = &ImageIndex;
        presentInfo.pResults = nullptr;

        iResult = vkQueuePresentKHR(m_PresentQueue, &presentInfo);

        if (iResult == VK_ERROR_OUT_OF_DATE_KHR || iResult == VK_SUBOPTIMAL_KHR || m_FrameBufferResized)
        {
            m_FrameBufferResized = false;
            RecreateSwapChain();
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
		model = glm::translate(model, glm::vec3(0.0f, 0.0f, 1.0f));
		model = glm::rotate(model, TimeSinceStart * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));

		ubo.Model = model;
		ubo.View = m_camera->GetViewMatrix();
		ubo.Proj = m_camera->GetProjectionMatrix();
		ubo.Proj[1][1] *= -1.0f;
		
		// Copy the ubo to the GPU
		void* data = nullptr;
		vkMapMemory(m_Device, m_UniformBuffersMemory[t_CurrentImage], 0, sizeof(ubo), 0, &data);
		memcpy(data, &ubo, sizeof(ubo));
		vkUnmapMemory(m_Device, m_UniformBuffersMemory[t_CurrentImage]);
    }	

    void Renderer::PrepShutdown()
    {
        vkDeviceWaitIdle(m_Device);
    }

	void Renderer::Shutdown()
	{
		// Cleanup Vulkan ------
        CleanUpSwapChain();

        vkDestroyDescriptorSetLayout(m_Device, m_DescriptorSetLayout, nullptr);

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

        //vkDestroyBuffer(m_Device, m_VertexBuffer, nullptr);
        //vkFreeMemory(m_Device, m_VertexBufferMemory, nullptr);

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) 
        {
            vkDestroySemaphore(m_Device, m_RenderFinishedSemaphores[i], nullptr);
            vkDestroySemaphore(m_Device, m_ImageAvailableSemaphores[i], nullptr);
            vkDestroyFence(m_Device, m_InFlightFences[i], nullptr);
        }

        vkDestroyCommandPool(m_Device, m_CommandPool, nullptr);

        vkDestroyDevice(m_Device, nullptr);

        // Debug messenger
        if (m_EnableValidationLayers)
        {
            DestroyDebugUtilsMessengerEXT(m_Instance, m_DebugMessenger, nullptr);
        }

        vkDestroySurfaceKHR(m_Instance, m_Surface, nullptr);
		vkDestroyInstance( m_Instance, nullptr );

		// Cleanup Window --------
		if (m_CurrentWindow)
		{
			delete m_CurrentWindow;
			m_CurrentWindow = nullptr;
		}

		//glfwDestroyWindow(m_Window);
		//glfwTerminate();
	}

}	// namespace Fling