#include "pch.h"

#if defined FLING_WINDOWS

#include "Input/WindowsInput.h"

// Oof on this renderer dependency
// #TODO: Figure something else that's better for this
#include "Renderer.h"
#include <GLFW/glfw3.h>

#include "DesktopWindow.h"

namespace Fling
{
	Input* Input::m_Instace = new WindowsInput();
	Input::KeyMap Input::m_KeyMap;

	void WindowsInput::InitImpl()
	{
		// Any specific input binds that may be needed
		InitKeyMap();
	}

	void WindowsInput::ShutdownImpl()
	{
		// Cleanup any listeners or things like that in the future
	}

	void WindowsInput::InitKeyMap()
	{
		F_LOG_TRACE("Windows Input map!");
		// It's gotta happen somewhere...
		AddKeyMap(KeyNames::FL_KEY_W, FL_KEYCODE_W);
		AddKeyMap(KeyNames::FL_KEY_A, FL_KEYCODE_A);
		AddKeyMap(KeyNames::FL_KEY_S, FL_KEYCODE_S);
		AddKeyMap(KeyNames::FL_KEY_D, FL_KEYCODE_D);
	}

	void WindowsInput::Poll()
	{
		// #TODO: Poll any input keys I guess
	}

	bool WindowsInput::IsKeyDownImpl(const std::string& t_KeyName)
	{
		DesktopWindow* Window = static_cast<DesktopWindow*>(Renderer::Get().GetCurrentWindow());
		if (Window)
		{
			// Check the old state of this input!
			Key& CurKey = m_KeyMap.at(t_KeyName);
			
			int State = glfwGetKey(Window->GetGlfwWindow(), CurKey.GetCode());
			// If the current key was up last time we looked and is 
			if((State == GLFW_PRESS || State == GLFW_REPEAT) && CurKey.IsUp())
			{
				CurKey.SetState(KeyState::DOWN);
				return true;
			}
			else
			{
				CurKey.SetState(KeyState::UP);
				return false;
			}
		}

		return false;
	}

	bool WindowsInput::IsKeyHeldImpl(const std::string& t_KeyName)
	{
		return false;
	}

	bool WindowsInput::IsMouseButtonPressedImpl(const std::string& t_KeyName)
	{
		return false;
	}

} // namespace Fling

#endif	// FLING_WINDOWS