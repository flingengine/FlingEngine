#include "pch.h"
#include "Renderer.h"

namespace Fling
{
	void Renderer::Init()
	{
		InitGraphics();
	}

	void Renderer::InitGraphics()
	{
		CreateGraphicsInstance();
		SetupDebugMessesages();
	}

	void Renderer::CreateGraphicsInstance()
	{
		if( m_enableValidationLayers && !CheckValidationLayerSupport() )
		{
			F_LOG_ERROR( "Validation layers are requested, but not available!" );
			throw std::runtime_error( "Validation layers are requested, but not available!" );
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
		auto extensions = GetRequiredExtensions();
		createInfo.enabledExtensionCount = static_cast<uint32_t>( extensions.size() );
		createInfo.ppEnabledExtensionNames = extensions.data();

		// Determine global validation layer count
		if( m_enableValidationLayers )
		{
			createInfo.enabledLayerCount = static_cast<UINT32>( m_validationLayers.size() );
			createInfo.ppEnabledLayerNames = m_validationLayers.data();
		}
		else 
		{
			createInfo.enabledLayerCount = 0;
		}

		// Create the vulkan instance! Throw exception if it fails
		if( vkCreateInstance( &createInfo, nullptr, &m_instance ) != VK_SUCCESS )
		{
			F_LOG_ERROR( "Failed to create the Vulkan instance!" );
			std::runtime_error( "Failed to create the Vulkan instance!" );
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

		for( const char* layerName : m_validationLayers )
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

	void Renderer::SetupDebugMessesages()
	{
		if( !m_enableValidationLayers ) return;

		VkDebugUtilsMessengerCreateInfoEXT createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
		createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
		createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
		createInfo.pfnUserCallback = DebugCallback;
		createInfo.pUserData = nullptr; // Optional
		
		if( CreateDebugUtilsMessengerEXT( m_instance, &createInfo, nullptr, &m_debugMessenger ) != VK_SUCCESS )
		{
			F_LOG_ERROR( "failed to set up debug messenger!" );
			throw std::runtime_error( "failed to set up debug messenger!" );
		}

	}

	std::vector<const char*> Renderer::GetRequiredExtensions()
	{
		UINT32 glfwExtensionCount = 0;
		const char** glfwExtensions;
		glfwExtensions = glfwGetRequiredInstanceExtensions( &glfwExtensionCount );

		std::vector<const char*> extensions( glfwExtensions, glfwExtensions + glfwExtensionCount );

		if( m_enableValidationLayers ) 
		{
			extensions.push_back( VK_EXT_DEBUG_UTILS_EXTENSION_NAME );
		}

		return extensions;
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
		auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr( instance, "vkCreateDebugUtilsMessengerEXT" );
		if( func != nullptr ) 
		{
			return func( instance, pCreateInfo, pAllocator, pDebugMessenger );
		}
		else {
			return VK_ERROR_EXTENSION_NOT_PRESENT;
		}
	}

	void Renderer::DestroyDebugUtilsMessengerEXT( VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator )
	{
		auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr( instance, "vkDestroyDebugUtilsMessengerEXT" );
		if( func != nullptr ) 
		{
			func( instance, m_debugMessenger, pAllocator );
		}
	}

	void Renderer::CreateGameWindow( const int t_width, const int t_height )
	{
		m_width = t_width;
		m_height = m_height;
		glfwInit();

		glfwWindowHint( GLFW_CLIENT_API, GLFW_NO_API );
		m_window = glfwCreateWindow( m_width, m_height, "Engine Window", nullptr, nullptr );
	}

	void Renderer::Shutdown()
	{
		// Cleanup vulkan ------
		if( m_enableValidationLayers ) 
		{
			DestroyDebugUtilsMessengerEXT( m_instance, m_debugMessenger, nullptr );
		}

		vkDestroyInstance( m_instance, nullptr );

		// Cleanup GLFW --------
		glfwDestroyWindow( m_window );
		glfwTerminate();
	}

}	// namespace Fling