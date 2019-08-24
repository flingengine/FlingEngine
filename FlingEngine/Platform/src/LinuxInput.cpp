#include "pch.h"
#include "LinuxInput.h"

#ifdef FLING_LINUX
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
		// #TODO: Get the window
		int State = glfwGetKey(nullptr, t_Keycode);
		return (State == GLFW_PRESS || State == GLFW_REPEAT);
	}

	bool LinuxInput::IsMouseButtonPressedImpl(int t_Button)
	{
		int State = glfwGetMouseButton(nullptr, t_Button);
		return (State == GLFW_PRESS);
	}

} // namespace Fling

#endif	// FLING_LINUX