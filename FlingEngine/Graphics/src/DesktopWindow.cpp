#include "DesktopWindow.h"
#include "Texture.h"
#include "FlingConfig.h"
#include "VulkanApp.h"
#include "Logger.h"
#include "BuildConfig.h"

#include <algorithm>

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

		glfwSetFramebufferSizeCallback(m_Window, &DesktopWindow::FrameBufferResizeCallback);

		FitToWorkArea(t_Props.m_Width, t_Props.m_Height);

		int FbW = 0, FbH = 0;
		glfwGetFramebufferSize(m_Window, &FbW, &FbH);
		F_LOG_TRACE("Window created: {}x{} framebuffer {}x{}", GetWindowWidth(), GetWindowHeight(), FbW, FbH);
	}

	void DesktopWindow::FitToWorkArea(uint32 t_Width, uint32 t_Height)
	{
		GLFWmonitor* Monitor = glfwGetPrimaryMonitor();
		if (Monitor == nullptr || t_Width == 0 || t_Height == 0)
		{
			return;
		}

		int WorkX = 0, WorkY = 0, WorkW = 0, WorkH = 0;
		glfwGetMonitorWorkarea(Monitor, &WorkX, &WorkY, &WorkW, &WorkH);

		// Wayland reports the work area in pixels but sizes windows in scaled units. The
		// monitor scale is rounded up to a whole number, so use the window's scale, which
		// is the real (possibly fractional) one.
		if (glfwGetPlatform() == GLFW_PLATFORM_WAYLAND)
		{
			float ScaleX = 1.0f, ScaleY = 1.0f;
			glfwGetWindowContentScale(m_Window, &ScaleX, &ScaleY);
			WorkW = static_cast<int>(WorkW / ScaleX);
			WorkH = static_cast<int>(WorkH / ScaleY);
		}
		if (WorkW <= 0 || WorkH <= 0)
		{
			return;
		}

		// Window managers such as GNOME maximize a new window that covers most of the work
		// area, which makes windowed mode look like fullscreen.
		const float MaxFraction = 0.75f;
		const float Fit = std::min({ 1.0f,
			(WorkW * MaxFraction) / static_cast<float>(t_Width),
			(WorkH * MaxFraction) / static_cast<float>(t_Height) });
		if (Fit < 1.0f)
		{
			const int Width = static_cast<int>(t_Width * Fit);
			const int Height = static_cast<int>(t_Height * Fit);
			F_LOG_WARN("Window size {}x{} is too big for the {}x{} work area, using {}x{}",
				t_Width, t_Height, WorkW, WorkH, Width, Height);
			glfwSetWindowSize(m_Window, Width, Height);
		}
	}

	void DesktopWindow::FrameBufferResizeCallback(GLFWwindow* t_Window, int t_Width, int t_Height)
	{
		VulkanApp::Get().OnWindowResized(t_Width, t_Height);
	}

	void DesktopWindow::CreateSurface(void* t_GraphicsInstance, void* t_SurfData)
	{
		if (glfwCreateWindowSurface((VkInstance)t_GraphicsInstance, m_Window, nullptr, (VkSurfaceKHR*)t_SurfData) != VK_SUCCESS)
		{
			F_LOG_FATAL("Failed to created the window surface!");
		}
	}

	void DesktopWindow::WaitForNewWindowSize()
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
		// Nothing to do. GlfwInputDeviceInterface::Poll pumps the GLFW events.
	}

	int DesktopWindow::ShouldClose()
	{
		return glfwWindowShouldClose(m_Window);	
	}

	bool DesktopWindow::IsMinimized() const
	{
		return false;
	}

	uint32 DesktopWindow::GetFramebufferWidth() const
	{
		int width, height;
		glfwGetFramebufferSize(m_Window, &width, &height);

		return width;
	}

	uint32 DesktopWindow::GetFramebufferHeight() const
	{
		int width, height;
		glfwGetFramebufferSize(m_Window, &width, &height);

		return height;
	}

	uint32 DesktopWindow::GetWindowWidth() const
	{
		int width, height;
		glfwGetWindowSize(m_Window, &width, &height);

		return width;
	}

	uint32 DesktopWindow::GetWindowHeight() const
	{
		int width, height;
		glfwGetWindowSize(m_Window, &width, &height);

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

        const int windowedWidth = FlingConfig::GetInt("Windowed", "WindowWidth") ? FlingConfig::GetInt("Windowed", "WindowWidth") : FLING_DEFAULT_WINDOW_WIDTH;
        const int windowedHeight = FlingConfig::GetInt("Windowed", "WindowHeight") ? FlingConfig::GetInt("Windowed", "WindowHeight") : FLING_DEFAULT_WINDOW_HEIGHT;

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
		default:
			break;	
		}
		m_WindowMode = t_WindowMode;

	}

	WindowMode DesktopWindow::GetWindowMode()
	{
		return m_WindowMode;
	}

}   // namespace Fling