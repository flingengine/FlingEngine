#include "pch.h"
#include "Renderer.h"

namespace Fling
{
	void Renderer::Init()
	{
		InitWindow();
	}

	void Renderer::InitWindow()
	{
		glfwInit();

		glfwWindowHint( GLFW_CLIENT_API, GLFW_NO_API );
		m_window = glfwCreateWindow( 800, 600, "Engine Window", nullptr, nullptr );

		uint32_t extensionCount = 0;
		vkEnumerateInstanceExtensionProperties( nullptr, &extensionCount, nullptr );

		F_LOG_TRACE( "{} Extensions are supported!", extensionCount );
	}

	void Renderer::Shutdown()
	{
		if( m_window )
		{
			glfwDestroyWindow( m_window );
		}

		glfwTerminate();
	}
}	// namespace Fling