#pragma once

#include "pch.h"
#include "DesktopWindow.h"

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

		//glfwSetFramebufferSizeCallback(m_Window, &FrameBufferResizeCallback);
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
}   // namespace Fling