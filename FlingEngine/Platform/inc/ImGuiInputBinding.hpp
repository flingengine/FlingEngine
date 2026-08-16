#pragma once


#if WITH_IMGUI
#include <imgui.h>

#if FLING_WINDOWS

#undef APIENTRY
#define GLFW_EXPOSE_NATIVE_WIN32

#elif FLING_LINUX

#define GLFW_EXPOSE_NATIVE_X11

#endif

#include <GLFW/glfw3native.h>   // for glfwGetWin32Window

namespace Fling
{
	namespace InternalImGui
	{
		// Data
		enum GlfwClientApi
		{
			GlfwClientApi_Unknown,
			GlfwClientApi_OpenGL,
			GlfwClientApi_Vulkan
		};

		static GlfwClientApi        g_ClientApi = GlfwClientApi_Unknown;
		static bool                 g_MouseJustPressed[5] = { false, false, false, false, false };
		static bool                 g_InstalledCallbacks = false;

		// Chain GLFW callbacks: our callbacks will call the user's previously installed callbacks, if any.
		static GLFWmousebuttonfun   g_PrevUserCallbackMousebutton = NULL;
		static GLFWscrollfun        g_PrevUserCallbackScroll = NULL;
		static GLFWkeyfun           g_PrevUserCallbackKey = NULL;
		static GLFWcharfun          g_PrevUserCallbackChar = NULL;

		static const char* ImGui_ImplGlfw_GetClipboardText(void* user_data)
		{
			return glfwGetClipboardString((GLFWwindow*)user_data);
		}

		static void ImGui_ImplGlfw_SetClipboardText(void* user_data, const char* text)
		{
			glfwSetClipboardString((GLFWwindow*)user_data, text);
		}

			// Maps a raw FL_KEYCODE_* key code to the ImGuiKey enum required by the
		// enum required by the modern (1.87+) io.AddKeyEvent() input API.
		static ImGuiKey ImGui_ImplGlfw_KeyToImGuiKey(int key)
		{
			switch (key)
			{
			case FL_KEYCODE_TAB: return ImGuiKey_Tab;
			case FL_KEYCODE_LEFT: return ImGuiKey_LeftArrow;
			case FL_KEYCODE_RIGHT: return ImGuiKey_RightArrow;
			case FL_KEYCODE_UP: return ImGuiKey_UpArrow;
			case FL_KEYCODE_DOWN: return ImGuiKey_DownArrow;
			case FL_KEYCODE_PAGE_UP: return ImGuiKey_PageUp;
			case FL_KEYCODE_PAGE_DOWN: return ImGuiKey_PageDown;
			case FL_KEYCODE_HOME: return ImGuiKey_Home;
			case FL_KEYCODE_END: return ImGuiKey_End;
			case FL_KEYCODE_INSERT: return ImGuiKey_Insert;
			case FL_KEYCODE_DELETE: return ImGuiKey_Delete;
			case FL_KEYCODE_BACKSPACE: return ImGuiKey_Backspace;
			case FL_KEYCODE_SPACE: return ImGuiKey_Space;
			case FL_KEYCODE_ENTER: return ImGuiKey_Enter;
			case FL_KEYCODE_ESCAPE: return ImGuiKey_Escape;
			case FL_KEYCODE_KP_ENTER: return ImGuiKey_KeypadEnter;
			case FL_KEYCODE_LEFT_CONTROL: return ImGuiKey_LeftCtrl;
			case FL_KEYCODE_RIGHT_CONTROL: return ImGuiKey_RightCtrl;
			case FL_KEYCODE_LEFT_SHIFT: return ImGuiKey_LeftShift;
			case FL_KEYCODE_RIGHT_SHIFT: return ImGuiKey_RightShift;
			case FL_KEYCODE_LEFT_ALT: return ImGuiKey_LeftAlt;
			case FL_KEYCODE_RIGHT_ALT: return ImGuiKey_RightAlt;
			case FL_KEYCODE_LEFT_SUPER: return ImGuiKey_LeftSuper;
			case FL_KEYCODE_RIGHT_SUPER: return ImGuiKey_RightSuper;
			case FL_KEYCODE_A: return ImGuiKey_A;
			case FL_KEYCODE_C: return ImGuiKey_C;
			case FL_KEYCODE_V: return ImGuiKey_V;
			case FL_KEYCODE_X: return ImGuiKey_X;
			case FL_KEYCODE_Y: return ImGuiKey_Y;
			case FL_KEYCODE_Z: return ImGuiKey_Z;
			default: return ImGuiKey_None;
			}
		}

		void ImGui_ImplGlfw_MouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
		{
			if (g_PrevUserCallbackMousebutton != NULL)
				g_PrevUserCallbackMousebutton(window, button, action, mods);

			if (action == GLFW_PRESS && button >= 0 && button < IM_ARRAYSIZE(g_MouseJustPressed))
				g_MouseJustPressed[button] = true;
		}

		void ImGui_ImplGlfw_ScrollCallback(GLFWwindow* window, double xoffset, double yoffset)
		{
			if (g_PrevUserCallbackScroll != NULL)
				g_PrevUserCallbackScroll(window, xoffset, yoffset);

			ImGuiIO& io = ImGui::GetIO();
			io.MouseWheelH += (float)xoffset;
			io.MouseWheel += (float)yoffset;
		}

		void ImGui_ImplGlfw_CharCallback(GLFWwindow* window, unsigned int c)
		{
			if (g_PrevUserCallbackChar != NULL)
				g_PrevUserCallbackChar(window, c);

			ImGuiIO& io = ImGui::GetIO();
			io.AddInputCharacter(c);
		}

		void ImGui_ImplGlfw_KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
		{
			if (g_PrevUserCallbackKey != NULL)
				g_PrevUserCallbackKey(window, key, scancode, action, mods);

			if (action != GLFW_PRESS && action != GLFW_RELEASE)
				return;

			ImGuiIO& io = ImGui::GetIO();
			io.AddKeyEvent(ImGuiMod_Ctrl, (mods & GLFW_MOD_CONTROL) != 0);
			io.AddKeyEvent(ImGuiMod_Shift, (mods & GLFW_MOD_SHIFT) != 0);
			io.AddKeyEvent(ImGuiMod_Alt, (mods & GLFW_MOD_ALT) != 0);
			io.AddKeyEvent(ImGuiMod_Super, (mods & GLFW_MOD_SUPER) != 0);

			ImGuiKey ImGuiKeyCode = ImGui_ImplGlfw_KeyToImGuiKey(key);
			io.AddKeyEvent(ImGuiKeyCode, action == GLFW_PRESS);
		}

		void SetImGuiCallbacks()
		{
			if (!ImGui::GetCurrentContext())
			{
				ImGui::CreateContext();
			}

			ImGuiIO& io = ImGui::GetIO();
			io.BackendFlags |= ImGuiBackendFlags_HasMouseCursors;         // We can honor GetMouseCursor() values (optional)
			io.BackendFlags |= ImGuiBackendFlags_HasSetMousePos;          // We can honor io.WantSetMousePos requests (optional, rarely used)
			io.BackendPlatformName = "imgui_impl_glfw";

			DesktopWindow* DesktopWin = static_cast<DesktopWindow*>(VulkanApp::Get().GetCurrentWindow());

			assert(DesktopWin);

			io.SetClipboardTextFn = ImGui_ImplGlfw_SetClipboardText;
			io.GetClipboardTextFn = ImGui_ImplGlfw_GetClipboardText;
			io.ClipboardUserData = DesktopWin;

			// Chain GLFW callbacks: our callbacks will call the user's previously installed callbacks, if any.
			g_PrevUserCallbackMousebutton = nullptr;
			g_PrevUserCallbackScroll = nullptr;
			g_PrevUserCallbackKey = nullptr;
			g_PrevUserCallbackChar = nullptr;
			{
				GLFWwindow* Window = DesktopWin->GetGlfwWindow();
				g_InstalledCallbacks = true;
				g_PrevUserCallbackMousebutton = glfwSetMouseButtonCallback(Window, ImGui_ImplGlfw_MouseButtonCallback);
				g_PrevUserCallbackScroll = glfwSetScrollCallback(Window, ImGui_ImplGlfw_ScrollCallback);
				g_PrevUserCallbackKey = glfwSetKeyCallback(Window, ImGui_ImplGlfw_KeyCallback);
				g_PrevUserCallbackChar = glfwSetCharCallback(Window, ImGui_ImplGlfw_CharCallback);
			}
			g_ClientApi = GlfwClientApi_Vulkan;
		}
	}	// namespace InternalImGui
}	// namespace Fling
#endif