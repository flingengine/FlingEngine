#include "pch.h"
#include "Renderer.h"

namespace Fling
{
	void Renderer::Init()
	{
		InitGraphics();
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
                Indecies.GraphicsFamily = Family.queueFlags & VK_QUEUE_GRAPHICS_BIT ? i : 0;
                
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
		CreateGraphicsInstance();
		SetupDebugMessesages();
        CreateSurface();
		PickPhysicalDevice();
        CreateLogicalDevice();
        CreateSwapChain();
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
		appInfo.applicationVersion = VK_MAKE_VERSION( 0, 1, 0 );
		appInfo.pEngineName = "Fling Engine";
		appInfo.engineVersion = VK_MAKE_VERSION( 0, 1, 0 );
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
			for( const auto& layerProperties : availableLayers )
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

        // Set the enabled extenstions
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
        if (glfwCreateWindowSurface(m_Instance, m_Window, nullptr, &m_Surface) != VK_SUCCESS)
        {
            F_LOG_FATAL("Failed to created the window surface!");
        }
    }

    void Renderer::CreateSwapChain()
    {
        SwapChainSupportDetails SwapChainSupport = QuerySwapChainSupport(m_PhysicalDevice);

        m_SwapChainFormat = ChooseSwapChainSurfaceFormat(SwapChainSupport.Formats);
        VkPresentModeKHR PresentMode = ChooseSwapChainPresentMode(SwapChainSupport.PresentModes);
        m_SwapChainExtents = ChooseSwapExtent(SwapChainSupport.Capabilities);

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
        CreateInfo.imageFormat = m_SwapChainFormat.format;
        CreateInfo.imageColorSpace = m_SwapChainFormat.colorSpace;
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
        if (t_Capabilies.currentExtent.width != std::numeric_limits<uint32_t>::max())
        {
            return t_Capabilies.currentExtent;
        }
        else 
        {
            VkExtent2D actualExtent = { m_WindowWidth, m_WindowHeight };

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

	void Renderer::CreateGameWindow( const UINT32 t_width, const UINT32 t_height )
	{
		m_WindowWidth = t_width;
		m_WindowHeight = m_WindowHeight;
		glfwInit();

		glfwWindowHint( GLFW_CLIENT_API, GLFW_NO_API );
		m_Window = glfwCreateWindow( m_WindowWidth, m_WindowHeight, "Engine Window", nullptr, nullptr );
	}

	void Renderer::Shutdown()
	{
		// Cleanup Vulkan ------
		if( m_EnableValidationLayers ) 
		{
			DestroyDebugUtilsMessengerEXT( m_Instance, m_DebugMessenger, nullptr );
		}

        vkDestroySwapchainKHR(m_Device, m_SwapChain, nullptr);
        vkDestroyDevice( m_Device, nullptr );
        vkDestroySurfaceKHR(m_Instance, m_Surface, nullptr);
		vkDestroyInstance( m_Instance, nullptr );

		// Cleanup GLFW --------
		glfwDestroyWindow( m_Window );
		glfwTerminate();
	}

}	// namespace Fling