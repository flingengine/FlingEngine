#include "pch.h"
#include "Renderer.h"

namespace Fling
{
	void Renderer::Init()
	{
		InitGraphics();
		InitWindow();
	}

	void Renderer::InitGraphics()
	{
		CreateGraphicsInstance();
	}

	void Renderer::CreateGraphicsInstance()
	{
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
		UINT32 glfwExtensinoCount = 0;
		const char** glfwExtensions;

		glfwExtensions = glfwGetRequiredInstanceExtensions( &glfwExtensinoCount );
		createInfo.enabledExtensionCount = glfwExtensinoCount;
		createInfo.ppEnabledExtensionNames = glfwExtensions;

		// Determine global validation layer count -- more to come
		createInfo.enabledLayerCount = 0;

		// Create the vulkan instance! Throw execption if it fails
		if( vkCreateInstance( &createInfo, nullptr, &m_instance ) != VK_SUCCESS )
		{
			F_LOG_ERROR( "Failed to create the Vulkan instance!" );
			std::runtime_error( "Failed to create the Vulkan instance!" );
		}

		// Find out what extensions are available
		uint32_t extensionCount = 0;
		vkEnumerateInstanceExtensionProperties( nullptr, &extensionCount, nullptr );
		std::vector<VkExtensionProperties> extensions( extensionCount );
		vkEnumerateInstanceExtensionProperties( nullptr, &extensionCount, extensions.data() );
		
		F_LOG_TRACE( "{} Extensions are supported!", extensionCount );
		for( const auto& extension : extensions )
		{
			F_LOG_TRACE( "\t{}", extension.extensionName );
		}
	}

	void Renderer::InitWindow()
	{
		glfwInit();

		glfwWindowHint( GLFW_CLIENT_API, GLFW_NO_API );
		m_window = glfwCreateWindow( 800, 600, "Engine Window", nullptr, nullptr );
	}

	void Renderer::Shutdown()
	{
		// Cleanup vulkan
		vkDestroyInstance( m_instance, nullptr );

		// Cleanup GLFW
		glfwDestroyWindow( m_window );
		glfwTerminate();
	}

}	// namespace Fling