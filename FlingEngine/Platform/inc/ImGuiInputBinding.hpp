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
		static double               g_Time = 0.0;
		static bool                 g_MouseJustPressed[5] = { false, false, false, false, false };
		static GLFWcursor* g_MouseCursors[ImGuiMouseCursor_COUNT] = {};
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

			ImGuiIO& io = ImGui::GetIO();
			if (action == GLFW_PRESS)
				io.KeysDown[key] = true;
			if (action == GLFW_RELEASE)
				io.KeysDown[key] = false;

			// Modifiers are not reliable across systems
			io.KeyCtrl = io.KeysDown[GLFW_KEY_LEFT_CONTROL] || io.KeysDown[GLFW_KEY_RIGHT_CONTROL];
			io.KeyShift = io.KeysDown[GLFW_KEY_LEFT_SHIFT] || io.KeysDown[GLFW_KEY_RIGHT_SHIFT];
			io.KeyAlt = io.KeysDown[GLFW_KEY_LEFT_ALT] || io.KeysDown[GLFW_KEY_RIGHT_ALT];
			io.KeySuper = io.KeysDown[GLFW_KEY_LEFT_SUPER] || io.KeysDown[GLFW_KEY_RIGHT_SUPER];
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

			io.KeyMap[ImGuiKey_Tab] = FL_KEYCODE_TAB;
			io.KeyMap[ImGuiKey_LeftArrow] = FL_KEYCODE_LEFT;
			io.KeyMap[ImGuiKey_RightArrow] = FL_KEYCODE_RIGHT;
			io.KeyMap[ImGuiKey_UpArrow] = FL_KEYCODE_UP;
			io.KeyMap[ImGuiKey_DownArrow] = FL_KEYCODE_DOWN;
			io.KeyMap[ImGuiKey_PageUp] = FL_KEYCODE_PAGE_UP;
			io.KeyMap[ImGuiKey_PageDown] = FL_KEYCODE_PAGE_DOWN;
			io.KeyMap[ImGuiKey_Home] = FL_KEYCODE_HOME;
			io.KeyMap[ImGuiKey_End] = FL_KEYCODE_END;
			io.KeyMap[ImGuiKey_Insert] = FL_KEYCODE_INSERT;
			io.KeyMap[ImGuiKey_Delete] = FL_KEYCODE_DELETE;
			io.KeyMap[ImGuiKey_Backspace] = FL_KEYCODE_BACKSPACE;
			io.KeyMap[ImGuiKey_Space] = FL_KEYCODE_SPACE;
			io.KeyMap[ImGuiKey_Enter] = FL_KEYCODE_ENTER;
			io.KeyMap[ImGuiKey_Escape] = FL_KEYCODE_ESCAPE;
			io.KeyMap[ImGuiKey_KeyPadEnter] = FL_KEYCODE_KP_ENTER;
			io.KeyMap[ImGuiKey_A] = FL_KEYCODE_A;
			io.KeyMap[ImGuiKey_C] = FL_KEYCODE_C;
			io.KeyMap[ImGuiKey_V] = FL_KEYCODE_V;
			io.KeyMap[ImGuiKey_X] = FL_KEYCODE_X;
			io.KeyMap[ImGuiKey_Y] = FL_KEYCODE_Y;
			io.KeyMap[ImGuiKey_Z] = FL_KEYCODE_Z;

			DesktopWindow* DesktopWin = static_cast<DesktopWindow*>(Renderer::Get().GetCurrentWindow());

			assert(DesktopWin);

			io.SetClipboardTextFn = ImGui_ImplGlfw_SetClipboardText;
			io.GetClipboardTextFn = ImGui_ImplGlfw_GetClipboardText;
			io.ClipboardUserData = DesktopWin;

#if FLING_WINDOWS
			io.ImeWindowHandle = (void*)glfwGetWin32Window(DesktopWin->GetGlfwWindow());
#elif FLING_LINUX
			io.ImeWindowHandle = (void*)glfwGetX11Window(DesktopWin->GetGlfwWindow());
#endif
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