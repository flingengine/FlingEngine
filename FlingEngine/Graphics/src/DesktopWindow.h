#pragma once

#include "FlingWindow.h"

// FlingVulkan.h sets VK_NO_PROTOTYPES before glfw3.h includes vulkan.h.
#include "FlingVulkan.h"

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

		/** Tick the window. GLFW events are pumped by GlfwInputDeviceInterface::Poll, not here. */
		virtual void Update() override;

		/** BLocks until we get a new window size from GLFW */
		virtual void WaitForNewWindowSize() override;

		/**
		* Check if this window should close
		* @return	0 if the window should not close, non-zero if it should.
		*/
		virtual int ShouldClose() override;

		/** Is this window currently minimized? */
		virtual bool IsMinimized() const override final;

		virtual uint32 GetFramebufferWidth() const override;

		virtual uint32 GetFramebufferHeight() const override;

		virtual uint32 GetWindowWidth() const override;

		virtual uint32 GetWindowHeight() const override;

		virtual float GetAspectRatio() const override;

		virtual void SetMouseVisible(bool t_IsVisible) override;

		virtual bool GetMouseVisible() override;

		/** 
		* Set this window's icon.
		* @param t_ID	the GUID of the window icon
		*/
		void SetWindowIcon(Guid t_ID) override;

		/** get the current GLFW window */
		GLFWwindow* GetGlfwWindow() const { return m_Window; }

		virtual void SetWindowMode(WindowMode t_WindowMode) override;
		virtual WindowMode GetWindowMode() override;
	private:

		/** Shrink the window, keeping its aspect ratio, if it would cover most of the monitor's work area */
		void FitToWorkArea(uint32 t_Width, uint32 t_Height);


		GLFWwindow* m_Window = nullptr;

		static void FrameBufferResizeCallback(GLFWwindow* t_Window, int t_Width, int t_Height);
	};
}   // namespace Fling