#include "pch.h"
#include "WindowsInput.h"

#if defined FLING_WINDOWS

// Oof on this renderer dependency
// #TODO: Figure something else that's better for this
#include "Renderer.h"
#include <GLFW/glfw3.h>

#include "DesktopWindow.h"

namespace Fling
{
	Input* Input::m_Instace = new WindowsInput();

	void WindowsInput::InitImpl()
	{
		// Any specific input binds that may be needed
	}

	void WindowsInput::ShutdownImpl()
	{
		// Cleanup any listeners or things like that in the future
	}

	bool WindowsInput::IsKeyDownImpl(int t_Keycode)
	{
		DesktopWindow* Window = static_cast<DesktopWindow*>(Renderer::Get().GetCurrentWindow());
		if (Window)
		{
			int State = glfwGetKey(Window->GetGlfwWindow(), t_Keycode);
			return (State == GLFW_PRESS);
		}
		else
		{
			return false;
		}
	}

	bool WindowsInput::IsKeyHeldImpl(int t_Keycode)
	{
		DesktopWindow* Window = static_cast<DesktopWindow*>(Renderer::Get().GetCurrentWindow());
		if (Window)
		{
			int State = glfwGetKey(Window->GetGlfwWindow(), t_Keycode);
			return (State == GLFW_PRESS || State == GLFW_REPEAT);
		}
		else
		{
			return false;
		}
	}

	bool WindowsInput::IsMouseButtonPressedImpl(int t_Button)
	{
		DesktopWindow* Window = static_cast<DesktopWindow*>(Renderer::Get().GetCurrentWindow());
		if (Window)
		{
			int State = glfwGetMouseButton(Window->GetGlfwWindow(), t_Button);
			return (State == GLFW_PRESS);
		}
		else
		{
			return false;
		}
	}

} // namespace Fling

#endif	// FLING_WINDOWS