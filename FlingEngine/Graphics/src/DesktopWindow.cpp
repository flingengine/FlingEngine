#include "pch.h"
#include "DesktopWindow.h"
#include "Texture.h"
#include "FlingConfig.h"

namespace Fling
{
	static bool bGlfwInitalized = false;

	FlingWindow* FlingWindow::Create(const WindowProps& t_Props)
	{
		return new DesktopWindow(t_Props);
	}

	DesktopWindow::DesktopWindow(const WindowProps& t_Props)
	{
		// Initialize GLFW if we have to
		if (!bGlfwInitalized)
		{
			glfwInit();
			glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
			bGlfwInitalized = true;
		}

		m_Window = glfwCreateWindow(t_Props.m_Width, t_Props.m_Height, t_Props.m_Title.c_str(), nullptr, nullptr);

		glfwSetFramebufferSizeCallback(m_Window, &FrameBufferResizeCallback);
	}

	void DesktopWindow::FrameBufferResizeCallback(GLFWwindow* t_Window, int t_Width, int t_Height)
	{
	}

	void DesktopWindow::CreateSurface(void* t_GraphicsInstance, void* t_SurfData)
	{
		if (glfwCreateWindowSurface((VkInstance)t_GraphicsInstance, m_Window, nullptr, (VkSurfaceKHR*)t_SurfData) != VK_SUCCESS)
		{
			F_LOG_FATAL("Failed to created the window surface!");
		}
	}

	void DesktopWindow::RecreateSwapChain()
	{
		// If the window is minimized then wait for it to come to the foreground before displaying it again
		// #TODO: Handle minimizing windows asynchronously or in a way that the renderer doesn't block
		int width = 0, height = 0;
		while (width == 0 || height == 0)
		{
			glfwGetFramebufferSize(m_Window, &width, &height);
			glfwWaitEvents();
		}
	}

	DesktopWindow::~DesktopWindow()
	{
		glfwDestroyWindow(m_Window);
		glfwTerminate();
	}

	void DesktopWindow::Update()
	{
		glfwPollEvents();
	}

	int DesktopWindow::ShouldClose()
	{
		return glfwWindowShouldClose(m_Window);	
	}

	bool DesktopWindow::IsMinimized() const
	{
		return false;
	}

	UINT32 DesktopWindow::GetWidth() const
	{
		int width, height;
		glfwGetFramebufferSize(m_Window, &width, &height);

		return width;
	}

	UINT32 DesktopWindow::GetHeight() const
	{
		int width, height;
		glfwGetFramebufferSize(m_Window, &width, &height);

		return height;
	}

	float DesktopWindow::GetAspectRatio() const
	{
		int width, height;
		glfwGetFramebufferSize(m_Window, &width, &height);

		return static_cast<float>(width) / static_cast<float>(height);
	}

	void DesktopWindow::SetMouseVisible(bool t_IsVisible)
	{
		m_IsMouseVisible = t_IsVisible;
		if (t_IsVisible)
		{
			glfwSetInputMode(m_Window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
		}
		else
		{
			glfwSetInputMode(m_Window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
		}
	}

	bool DesktopWindow::GetMouseVisible()
	{
		return m_IsMouseVisible;
	}

	void DesktopWindow::SetWindowIcon(Guid t_ID)
	{
		// Load an image 
		std::shared_ptr<Texture> Icon = Texture::Create(t_ID);
		assert(Icon);

		// Set the Pixel data for this image
		GLFWimage GLFW_Image;
		GLFW_Image.height = Icon->GetHeight();
		GLFW_Image.width = Icon->GetWidth();
		GLFW_Image.pixels = Icon->GetPixelData();

		// Set it via GLFW
		glfwSetWindowIcon(m_Window, 1, &GLFW_Image);
	}

	void DesktopWindow::SetWindowMode(WindowMode t_WindowMode)
	{
		//early return if nothing would change
        if (t_WindowMode == m_WindowMode)
        {
            return;
        }

        int windowedWidth = FlingConfig::GetInt("Windowed", "WindowWidth") ? FlingConfig::GetInt("Windowed", "WindowWidth") : FLING_DEFAULT_WINDOW_WIDTH;
        int windowedHeight = FlingConfig::GetInt("Windowed", "WindowHeight") ? FlingConfig::GetInt("Windowed", "WindowHeight") : FLING_DEFAULT_WINDOW_HEIGHT;

		const GLFWvidmode* mode = glfwGetVideoMode(glfwGetPrimaryMonitor());

		switch (t_WindowMode)
		{
		case WindowMode::Fullscreen:
			glfwSetWindowAttrib(m_Window, GLFW_DECORATED, GLFW_TRUE);
			glfwSetWindowMonitor(m_Window, glfwGetPrimaryMonitor(), 0, 0, mode->width, mode->height, 0);
			break;
		case WindowMode::Windowed:
			glfwSetWindowAttrib(m_Window, GLFW_DECORATED, GLFW_TRUE);
			glfwSetWindowMonitor(m_Window, nullptr, 100, 100, windowedWidth, windowedHeight, 0);
            break;
        case WindowMode::BorderlessWindowed:
            glfwSetWindowAttrib(m_Window, GLFW_DECORATED, GLFW_FALSE);
            glfwSetWindowMonitor(m_Window, nullptr, 100, 100, windowedWidth, windowedHeight, 0);
			break;
		}
		m_WindowMode = t_WindowMode;

	}

	WindowMode DesktopWindow::GetWindowMode()
	{
		return m_WindowMode;
	}

}   // namespace Fling