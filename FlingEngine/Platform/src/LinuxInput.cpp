#include "pch.h"
#include "LinuxInput.h"

#ifdef FLING_LINUX
#include "Renderer.h"

#include <GLFW/glfw3.h>

namespace Fling
{
	Input* Input::m_Instace = new LinuxInput();

	void LinuxInput::InitImpl()
	{
		// Any setup for GLFW inputs
	}

	void LinuxInput::ShutdownImpl()
	{
		// Cleanup any listeners or things like that in the future
	}

	bool LinuxInput::IsKeyDownImpl(int t_Keycode)
	{
		DesktopWindow* Window = static_cast<DesktopWindow*>(Renderer::Get().GetCurrentWindow());
		if (Window)
		{
			// Check the old state of this input! 
			int State = glfwGetKey(Window->GetGlfwWindow(), t_Keycode);
			return (State == GLFW_PRESS);
		}
		else
		{
			return false;
		}
	}

	bool LinuxInput::IsKeyHeldImpl(int t_Keycode)
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

	bool LinuxInput::IsMouseButtonPressedImpl(int t_Button)
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

#endif	// FLING_LINUX