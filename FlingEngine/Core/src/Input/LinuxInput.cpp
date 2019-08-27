#include "pch.h"

#ifdef FLING_LINUX

#include "Input/LinuxInput.h"

// Oof on this renderer dependency
// #TODO: Figure something else that's better for this
#include "Renderer.h"
#include <GLFW/glfw3.h>

namespace Fling
{
	Input* Input::m_Instace = new LinuxInput();
	
	Input::KeyMap Input::m_KeyMap;
	
	void LinuxInput::InitImpl()
	{
		// Any setup for GLFW inputs
		InitKeyMap();
	}

	void LinuxInput::ShutdownImpl()
	{
		// Cleanup any listeners or things like that in the future
	}

	void LinuxInput::InitKeyMap()
	{
		F_LOG_TRACE("Linux Input map!");
		// It's gotta happen somewhere...
		AddKeyMap(KeyNames::FL_KEY_W, FL_KEYCODE_W);
		AddKeyMap(KeyNames::FL_KEY_A, FL_KEYCODE_A);
		AddKeyMap(KeyNames::FL_KEY_S, FL_KEYCODE_S);
		AddKeyMap(KeyNames::FL_KEY_D, FL_KEYCODE_D);
	}

	void LinuxInput::PollImpl()
	{
		// #TODO: Poll any input keys I guess
	}
	
	bool LinuxInput::IsKeyDownImpl(const std::string& t_KeyName)
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

	bool LinuxInput::IsKeyHeldImpl(const std::string& t_KeyName)
	{
		return false;
	}

	bool LinuxInput::IsMouseButtonPressedImpl(const std::string& t_KeyName)
	{
		return false;
		//DesktopWindow* Window = static_cast<DesktopWindow*>(Renderer::Get().GetCurrentWindow());
		//if (Window)
		//{
		//	int State = glfwGetMouseButton(Window->GetGlfwWindow(), t_KeyName);
		//	return (State == GLFW_PRESS);
		//}
		//else
		//{
		//	return false;
		//}
	}

} // namespace Fling

#endif	// FLING_LINUX