#pragma once

#include "FlingWindow.h"

#ifndef GLFW_INCLUDE_VULKAN
#	define GLFW_INCLUDE_VULKAN
#endif
#include <GLFW/glfw3.h>

namespace Fling
{
	/**
	* Base class that represents a window to the Fling Engine using GLFW
	*/
	class DesktopWindow : public FlingWindow
	{
	public:

		DesktopWindow(const WindowProps& t_Props);
		virtual ~DesktopWindow();

		virtual void CreateSurface(void* t_GraphicsInstance, void* t_SurfData) override;

		/** Tick the window (poll input from GLFW) */
		virtual void Update() override;

		/** Recreate the swap chain based on current window size with GLFW */
		virtual void RecreateSwapChain() override;

		/**
		* Check if this window should close
		* @return	0 if the window should not close, non-zero if it should.
		*/
		virtual int ShouldClose() override;

		/** Is this window currently minimized? */
		virtual bool IsMinimized() const override;

		virtual UINT32 GetWidth() const override;

		virtual UINT32 GetHeight() const override;

		virtual float GetAspectRatio() const override;

		/** get the current GLFW window */
		GLFWwindow* GetGlfwWindow() const { return m_Window; }



	private:

		GLFWwindow* m_Window = nullptr;

		static void FrameBufferResizeCallback(GLFWwindow* t_Window, int t_Width, int t_Height);

	};
}   // namespace Fling