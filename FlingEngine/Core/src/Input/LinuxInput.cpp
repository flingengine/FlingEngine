#include "pch.h"

#if FLING_LINUX

#include "Input/LinuxInput.h"

// Oof on this renderer dependency
// #TODO: Figure something else that's better for this
#include "VulkanApp.h"
#include <GLFW/glfw3.h>
#include "ImGuiInputBinding.hpp"

namespace Fling
{
	Input* Input::m_Instance = new LinuxInput();
	Input::KeyMap Input::m_KeyMap;
	Input::KeyDownMap Input::m_KeyDownMap;
	
	void LinuxInput::InitImpl()
	{
		// Any setup for GLFW inputs
		InitKeyMap();
	}

	void LinuxInput::PreUpdateImpl()
	{
#if WITH_IMGUI
		InternalImGui::SetImGuiCallbacks();
#endif
	}

	void LinuxInput::ShutdownImpl()
	{
		// Cleanup any listeners or things like that in the future
	}

	void LinuxInput::InitKeyMap()
	{
		F_LOG_TRACE("Linux Input map!");
		/* The unknown key */
		AddKeyMap(KeyNames::FL_KEY_UNKNOWN, FL_KEYCODE_UNKNOWN);

		/* Printable keys */
		AddKeyMap(KeyNames::FL_KEY_SPACE, FL_KEYCODE_SPACE);
		AddKeyMap(KeyNames::FL_KEY_APOSTROPHE, FL_KEYCODE_APOSTROPHE);          /* ' */
		AddKeyMap(KeyNames::FL_KEY_COMMA, FL_KEYCODE_COMMA);               /* , */
		AddKeyMap(KeyNames::FL_KEY_MINUS, FL_KEYCODE_MINUS);               /* - */
		AddKeyMap(KeyNames::FL_KEY_PERIOD, FL_KEYCODE_PERIOD);              /* . */
		AddKeyMap(KeyNames::FL_KEY_SLASH, FL_KEYCODE_SLASH);               /* / */
		AddKeyMap(KeyNames::FL_KEY_0, FL_KEYCODE_0);
		AddKeyMap(KeyNames::FL_KEY_1, FL_KEYCODE_1);
		AddKeyMap(KeyNames::FL_KEY_2, FL_KEYCODE_2);
		AddKeyMap(KeyNames::FL_KEY_3, FL_KEYCODE_3);
		AddKeyMap(KeyNames::FL_KEY_4, FL_KEYCODE_4);
		AddKeyMap(KeyNames::FL_KEY_5, FL_KEYCODE_5);
		AddKeyMap(KeyNames::FL_KEY_6, FL_KEYCODE_6);
		AddKeyMap(KeyNames::FL_KEY_7, FL_KEYCODE_7);
		AddKeyMap(KeyNames::FL_KEY_8, FL_KEYCODE_8);
		AddKeyMap(KeyNames::FL_KEY_9, FL_KEYCODE_9);
		AddKeyMap(KeyNames::FL_KEY_SEMICOLON, FL_KEYCODE_SEMICOLON);  /* , ); */
		AddKeyMap(KeyNames::FL_KEY_EQUAL, FL_KEYCODE_EQUAL);  /* = */
		AddKeyMap(KeyNames::FL_KEY_A, FL_KEYCODE_A);
		AddKeyMap(KeyNames::FL_KEY_B, FL_KEYCODE_B);
		AddKeyMap(KeyNames::FL_KEY_C, FL_KEYCODE_C);
		AddKeyMap(KeyNames::FL_KEY_D, FL_KEYCODE_D);
		AddKeyMap(KeyNames::FL_KEY_E, FL_KEYCODE_E);
		AddKeyMap(KeyNames::FL_KEY_F, FL_KEYCODE_F);
		AddKeyMap(KeyNames::FL_KEY_G, FL_KEYCODE_G);
		AddKeyMap(KeyNames::FL_KEY_H, FL_KEYCODE_H);
		AddKeyMap(KeyNames::FL_KEY_I, FL_KEYCODE_I);
		AddKeyMap(KeyNames::FL_KEY_J, FL_KEYCODE_J);
		AddKeyMap(KeyNames::FL_KEY_K, FL_KEYCODE_K);
		AddKeyMap(KeyNames::FL_KEY_L, FL_KEYCODE_L);
		AddKeyMap(KeyNames::FL_KEY_M, FL_KEYCODE_M);
		AddKeyMap(KeyNames::FL_KEY_N, FL_KEYCODE_N);
		AddKeyMap(KeyNames::FL_KEY_O, FL_KEYCODE_O);
		AddKeyMap(KeyNames::FL_KEY_P, FL_KEYCODE_P);
		AddKeyMap(KeyNames::FL_KEY_Q, FL_KEYCODE_Q);
		AddKeyMap(KeyNames::FL_KEY_R, FL_KEYCODE_R);
		AddKeyMap(KeyNames::FL_KEY_S, FL_KEYCODE_S);
		AddKeyMap(KeyNames::FL_KEY_T, FL_KEYCODE_T);
		AddKeyMap(KeyNames::FL_KEY_U, FL_KEYCODE_U);
		AddKeyMap(KeyNames::FL_KEY_V, FL_KEYCODE_V);
		AddKeyMap(KeyNames::FL_KEY_W, FL_KEYCODE_W);
		AddKeyMap(KeyNames::FL_KEY_X, FL_KEYCODE_X);
		AddKeyMap(KeyNames::FL_KEY_Y, FL_KEYCODE_Y);
		AddKeyMap(KeyNames::FL_KEY_Z, FL_KEYCODE_Z);
		AddKeyMap(KeyNames::FL_KEY_LEFT_BRACKET, FL_KEYCODE_LEFT_BRACKET);  /* [ */
		AddKeyMap(KeyNames::FL_KEY_BACKSLASH, FL_KEYCODE_BACKSLASH);  /* \ */
		AddKeyMap(KeyNames::FL_KEY_RIGHT_BRACKET, FL_KEYCODE_RIGHT_BRACKET);  /* ] */
		AddKeyMap(KeyNames::FL_KEY_GRAVE_ACCENT, FL_KEYCODE_GRAVE_ACCENT);  /* ` */
		AddKeyMap(KeyNames::FL_KEY_WORLD_1, FL_KEYCODE_WORLD_1); /* non-US #1 */
		AddKeyMap(KeyNames::FL_KEY_WORLD_2, FL_KEYCODE_WORLD_2); /* non-US #2 */

		/* Function keys */
		AddKeyMap(KeyNames::FL_KEY_ESCAPE, FL_KEYCODE_ESCAPE);
		AddKeyMap(KeyNames::FL_KEY_ENTER, FL_KEYCODE_ENTER);
		AddKeyMap(KeyNames::FL_KEY_TAB, FL_KEYCODE_TAB);
		AddKeyMap(KeyNames::FL_KEY_BACKSPACE, FL_KEYCODE_BACKSPACE);
		AddKeyMap(KeyNames::FL_KEY_INSERT, FL_KEYCODE_INSERT);
		AddKeyMap(KeyNames::FL_KEY_DELETE, FL_KEYCODE_DELETE);
		AddKeyMap(KeyNames::FL_KEY_RIGHT, FL_KEYCODE_RIGHT);
		AddKeyMap(KeyNames::FL_KEY_LEFT, FL_KEYCODE_LEFT);
		AddKeyMap(KeyNames::FL_KEY_DOWN, FL_KEYCODE_DOWN);
		AddKeyMap(KeyNames::FL_KEY_UP, FL_KEYCODE_UP);
		AddKeyMap(KeyNames::FL_KEY_PAGE_UP, FL_KEYCODE_PAGE_UP);
		AddKeyMap(KeyNames::FL_KEY_PAGE_DOWN, FL_KEYCODE_PAGE_DOWN);
		AddKeyMap(KeyNames::FL_KEY_HOME, FL_KEYCODE_HOME);
		AddKeyMap(KeyNames::FL_KEY_END, FL_KEYCODE_END);
		AddKeyMap(KeyNames::FL_KEY_CAPS_LOCK, FL_KEYCODE_CAPS_LOCK);
		AddKeyMap(KeyNames::FL_KEY_SCROLL_LOCK, FL_KEYCODE_SCROLL_LOCK);
		AddKeyMap(KeyNames::FL_KEY_NUM_LOCK, FL_KEYCODE_NUM_LOCK);
		AddKeyMap(KeyNames::FL_KEY_PRINT_SCREEN, FL_KEYCODE_PRINT_SCREEN);
		AddKeyMap(KeyNames::FL_KEY_PAUSE, FL_KEYCODE_PAUSE);
		AddKeyMap(KeyNames::FL_KEY_F1, FL_KEYCODE_F1);
		AddKeyMap(KeyNames::FL_KEY_F2, FL_KEYCODE_F2);
		AddKeyMap(KeyNames::FL_KEY_F3, FL_KEYCODE_F3);
		AddKeyMap(KeyNames::FL_KEY_F4, FL_KEYCODE_F4);
		AddKeyMap(KeyNames::FL_KEY_F5, FL_KEYCODE_F5);
		AddKeyMap(KeyNames::FL_KEY_F6, FL_KEYCODE_F6);
		AddKeyMap(KeyNames::FL_KEY_F7, FL_KEYCODE_F7);
		AddKeyMap(KeyNames::FL_KEY_F8, FL_KEYCODE_F8);
		AddKeyMap(KeyNames::FL_KEY_F9, FL_KEYCODE_F9);
		AddKeyMap(KeyNames::FL_KEY_F10, FL_KEYCODE_F10);
		AddKeyMap(KeyNames::FL_KEY_F11, FL_KEYCODE_F11);
		AddKeyMap(KeyNames::FL_KEY_F12, FL_KEYCODE_F12);
		AddKeyMap(KeyNames::FL_KEY_KP_DECIMAL, FL_KEYCODE_KP_DECIMAL);
		AddKeyMap(KeyNames::FL_KEY_KP_DIVIDE, FL_KEYCODE_KP_DIVIDE);
		AddKeyMap(KeyNames::FL_KEY_KP_MULTIPLY, FL_KEYCODE_KP_MULTIPLY);
		AddKeyMap(KeyNames::FL_KEY_KP_SUBTRACT, FL_KEYCODE_KP_SUBTRACT);
		AddKeyMap(KeyNames::FL_KEY_KP_ADD, FL_KEYCODE_KP_ADD);
		AddKeyMap(KeyNames::FL_KEY_KP_ENTER, FL_KEYCODE_KP_ENTER);
		AddKeyMap(KeyNames::FL_KEY_KP_EQUAL, FL_KEYCODE_KP_EQUAL);
		AddKeyMap(KeyNames::FL_KEY_LEFT_SHIFT, FL_KEYCODE_LEFT_SHIFT);
		AddKeyMap(KeyNames::FL_KEY_LEFT_CONTROL, FL_KEYCODE_LEFT_CONTROL);
		AddKeyMap(KeyNames::FL_KEY_LEFT_ALT, FL_KEYCODE_LEFT_ALT);
		AddKeyMap(KeyNames::FL_KEY_LEFT_SUPER, FL_KEYCODE_LEFT_SUPER);
		AddKeyMap(KeyNames::FL_KEY_RIGHT_SHIFT, FL_KEYCODE_RIGHT_SHIFT);
		AddKeyMap(KeyNames::FL_KEY_RIGHT_CONTROL, FL_KEYCODE_RIGHT_CONTROL);
		AddKeyMap(KeyNames::FL_KEY_RIGHT_ALT, FL_KEYCODE_RIGHT_ALT);
		AddKeyMap(KeyNames::FL_KEY_RIGHT_SUPER, FL_KEYCODE_RIGHT_SUPER);
		AddKeyMap(KeyNames::FL_KEY_MENU, FL_KEYCODE_MENU);

		AddKeyMap(KeyNames::FL_MOUSE_BUTTON_1, FL_MOUSE_BUTTON_CODE_1);
		AddKeyMap(KeyNames::FL_MOUSE_BUTTON_2, FL_MOUSE_BUTTON_CODE_2);
		AddKeyMap(KeyNames::FL_MOUSE_BUTTON_3, FL_MOUSE_BUTTON_CODE_3);
		AddKeyMap(KeyNames::FL_MOUSE_BUTTON_4, FL_MOUSE_BUTTON_CODE_4);
		AddKeyMap(KeyNames::FL_MOUSE_BUTTON_5, FL_MOUSE_BUTTON_CODE_5);
		AddKeyMap(KeyNames::FL_MOUSE_BUTTON_6, FL_MOUSE_BUTTON_CODE_6);
		AddKeyMap(KeyNames::FL_MOUSE_BUTTON_7, FL_MOUSE_BUTTON_CODE_7);
		AddKeyMap(KeyNames::FL_MOUSE_BUTTON_8, FL_MOUSE_BUTTON_CODE_8);
	}

	void LinuxInput::PollImpl()
	{
		for(const auto& InputMapping : m_KeyDownMap)
		{
			if(IsKeyDown(InputMapping.first) && InputMapping.second)
			{
				InputMapping.second();
			}
		}

#if WITH_IMGUI
		// Update imgui mouse events and timings
		ImGuiIO& io = ImGui::GetIO();

		DesktopWindow* Window = static_cast<DesktopWindow*>(VulkanApp::Get().GetCurrentWindow());

		io.DisplaySize = ImVec2(
			static_cast<float>(Window->GetWidth()),
			static_cast<float>(Window->GetHeight())
		);

		io.MousePos = ImVec2(Input::GetMousePos().X, Input::GetMousePos().Y);

		io.MouseDown[0] = Input::IsMouseDown(KeyNames::FL_MOUSE_BUTTON_1);
		io.MouseDown[1] = Input::IsMouseDown(KeyNames::FL_MOUSE_BUTTON_2);
#endif
	}
	
	bool LinuxInput::IsKeyDownImpl(const std::string& t_KeyName)
	{
		DesktopWindow* Window = static_cast<DesktopWindow*>(VulkanApp::Get().GetCurrentWindow());
		if (Window)
		{
			// Check the old state of this input!
			Key& CurKey = m_KeyMap.at(t_KeyName);

			int State = glfwGetKey(Window->GetGlfwWindow(), CurKey.GetCode());
			// If the current key was up last time we looked and is 
			if ((State == GLFW_PRESS || State == GLFW_REPEAT) && CurKey.IsUp())
			{
				CurKey.SetState(KeyState::DOWN);
				return true;
			}
			else if (State == GLFW_RELEASE)
			{
				CurKey.SetState(KeyState::UP);
			}
		}

		return false;
	}

	bool LinuxInput::IsKeyHelpImpl(const std::string& t_KeyName)
	{
		DesktopWindow* Window = static_cast<DesktopWindow*>(VulkanApp::Get().GetCurrentWindow());
		if (Window)
		{
			// Check the old state of this input!
			Key& CurKey = m_KeyMap.at(t_KeyName);

			int State = glfwGetKey(Window->GetGlfwWindow(), CurKey.GetCode());
			// If the current key was up last time we looked and is 
			if ((State == GLFW_PRESS || State == GLFW_REPEAT))
			{
				CurKey.SetState(KeyState::DOWN);
				return true;
			}
			else if (State == GLFW_RELEASE)
			{
				CurKey.SetState(KeyState::UP);
			}
		}

		return false;
	}

	bool LinuxInput::IsMouseButtonPressedImpl(const std::string& t_KeyName)
	{
		DesktopWindow* Window = static_cast<DesktopWindow*>(VulkanApp::Get().GetCurrentWindow());
		if (Window)
		{
			Key& CurKey = m_KeyMap.at(t_KeyName);

			int State = glfwGetMouseButton(Window->GetGlfwWindow(), CurKey.GetCode());
			// If the current key was up last time we looked and is 
			if ((State == GLFW_PRESS || State == GLFW_REPEAT) && CurKey.IsUp())
			{
				CurKey.SetState(KeyState::DOWN);
				return true;
			}
			else if (State == GLFW_RELEASE)
			{
				CurKey.SetState(KeyState::UP);
			}
		}

		return false;
	}

	bool LinuxInput::IsMouseDownImpl(const std::string& t_KeyName)
	{
		DesktopWindow* Window = static_cast<DesktopWindow*>(VulkanApp::Get().GetCurrentWindow());
		if (Window)
		{
			Key& CurKey = m_KeyMap.at(t_KeyName);

			int State = glfwGetMouseButton(Window->GetGlfwWindow(), CurKey.GetCode());
			// If the current key was up last time we looked and is 
			return (State == GLFW_PRESS || State == GLFW_REPEAT);
		}
		return false;
	}

	MousePos LinuxInput::GetMousePosImpl()
	{
		MousePos CurPos = {};

		// #TODO Get rid of this WINDOW DEPENDECNY!!!!
		DesktopWindow* Window = static_cast<DesktopWindow*>(VulkanApp::Get().GetCurrentWindow());
		if (Window)
		{
			double xPos = 0.0;
			double yPos = 0.0;
			glfwGetCursorPos(Window->GetGlfwWindow(), &xPos, &yPos);
			CurPos.X = static_cast<float>(xPos);
			CurPos.Y = static_cast<float>(yPos);
		}

		return CurPos;
	}

} // namespace Fling

#endif	// FLING_LINUX