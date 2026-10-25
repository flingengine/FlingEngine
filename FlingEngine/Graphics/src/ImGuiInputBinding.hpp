#pragma once

#include "Platform.h"

#if WITH_IMGUI
#include <imgui.h>
#include "DesktopWindow.h"  // GLFW with GLFW_INCLUDE_VULKAN, before glfw3native.h
#include "VulkanApp.h"

#include "Input/Input.h"
#include <optional>

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

		// Maps a raw GLFW_KEY_* key code to the ImGuiKey enum required by the
		// enum required by the modern (1.87+) io.AddKeyEvent() input API.
		static ImGuiKey ImGui_ImplGlfw_KeyToImGuiKey(int key)
		{
			switch (key)
			{
			case GLFW_KEY_TAB: return ImGuiKey_Tab;
			case GLFW_KEY_LEFT: return ImGuiKey_LeftArrow;
			case GLFW_KEY_RIGHT: return ImGuiKey_RightArrow;
			case GLFW_KEY_UP: return ImGuiKey_UpArrow;
			case GLFW_KEY_DOWN: return ImGuiKey_DownArrow;
			case GLFW_KEY_PAGE_UP: return ImGuiKey_PageUp;
			case GLFW_KEY_PAGE_DOWN: return ImGuiKey_PageDown;
			case GLFW_KEY_HOME: return ImGuiKey_Home;
			case GLFW_KEY_END: return ImGuiKey_End;
			case GLFW_KEY_INSERT: return ImGuiKey_Insert;
			case GLFW_KEY_DELETE: return ImGuiKey_Delete;
			case GLFW_KEY_BACKSPACE: return ImGuiKey_Backspace;
			case GLFW_KEY_SPACE: return ImGuiKey_Space;
			case GLFW_KEY_ENTER: return ImGuiKey_Enter;
			case GLFW_KEY_ESCAPE: return ImGuiKey_Escape;
			case GLFW_KEY_KP_ENTER: return ImGuiKey_KeypadEnter;
			case GLFW_KEY_LEFT_CONTROL: return ImGuiKey_LeftCtrl;
			case GLFW_KEY_RIGHT_CONTROL: return ImGuiKey_RightCtrl;
			case GLFW_KEY_LEFT_SHIFT: return ImGuiKey_LeftShift;
			case GLFW_KEY_RIGHT_SHIFT: return ImGuiKey_RightShift;
			case GLFW_KEY_LEFT_ALT: return ImGuiKey_LeftAlt;
			case GLFW_KEY_RIGHT_ALT: return ImGuiKey_RightAlt;
			case GLFW_KEY_LEFT_SUPER: return ImGuiKey_LeftSuper;
			case GLFW_KEY_RIGHT_SUPER: return ImGuiKey_RightSuper;
			case GLFW_KEY_A: return ImGuiKey_A;
			case GLFW_KEY_C: return ImGuiKey_C;
			case GLFW_KEY_V: return ImGuiKey_V;
			case GLFW_KEY_X: return ImGuiKey_X;
			case GLFW_KEY_Y: return ImGuiKey_Y;
			case GLFW_KEY_Z: return ImGuiKey_Z;
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

		/**
		 * ImGui's own consumer of the input queue. It is not the game's processor, so
		 * Input::SetUICapture does not hide input from ImGui. It holds a weak_ptr to the
		 * queue, so it becomes invalid if Input is shut down.
		 */
		static std::optional<InputMessageProcessor> g_InputProcessor;

		/** Called each frame before ImGui::NewFrame. The mouse comes from the input system. */
		void UpdateImGuiIO(FlingWindow* t_Window)
		{
			ImGuiIO& io = ImGui::GetIO();

			io.DisplaySize = ImVec2(
				static_cast<float>(t_Window->GetWidth()),
				static_cast<float>(t_Window->GetHeight())
			);

			if (!Input::Get().IsInitialized())
			{
				g_InputProcessor.reset();
				return;
			}
			if (!g_InputProcessor || !g_InputProcessor->IsValid())
			{
				g_InputProcessor.emplace(Input::Get().CreateProcessor());
			}

			InputMessageProcessor& Processor = *g_InputProcessor;
			if (Processor.Overran())
			{
				const std::shared_ptr<InputMessageHandler>& Handler = Input::Get().GetHandler();
				Processor.Resync(Handler->GetLatestState(), Handler->GetLatestSequence());
			}
			Processor.ConsumeUntil();

			bool MouseLeft = false;
			bool MouseRight = false;
			for (const std::shared_ptr<InputDevice>& Device : Input::Get().GetDevices())
			{
				if (!Device->GetInfo().HasCapability(InputDeviceCapabilities::Pointer))
				{
					continue;
				}
				const InputDeviceState* State = Processor.GetState().Find(Device->GetId());
				if (State == nullptr)
				{
					continue;
				}
				io.MousePos = ImVec2(State->PointerX, State->PointerY);

				// A press and release inside one frame still gives ImGui one frame of "down".
				const InputKeyState Left = State->GetKeyState(Keys::MouseLeft);
				const InputKeyState Right = State->GetKeyState(Keys::MouseRight);
				MouseLeft = MouseLeft || Left.IsHeld || Left.WasPressed();
				MouseRight = MouseRight || Right.IsHeld || Right.WasPressed();
			}
			io.MouseDown[0] = MouseLeft;
			io.MouseDown[1] = MouseRight;

			Processor.ClearEdges();
		}

		/** Tell the input system which devices ImGui owns this frame. Call after ImGui::NewFrame. */
		void UpdateInputCapture()
		{
			if (Input::Get().IsInitialized())
			{
				const ImGuiIO& io = ImGui::GetIO();
				Input::Get().SetUICapture(io.WantCaptureKeyboard, io.WantCaptureMouse);
			}
		}

		/** Install the ImGui GLFW callbacks once. They chain to the callbacks that were installed before. */
		void SetImGuiCallbacks(FlingWindow* t_Window)
		{
			if (g_InstalledCallbacks)
			{
				return;
			}

			if (!ImGui::GetCurrentContext())
			{
				ImGui::CreateContext();
			}

			ImGuiIO& io = ImGui::GetIO();
			io.BackendFlags |= ImGuiBackendFlags_HasMouseCursors;         // We can honor GetMouseCursor() values (optional)
			io.BackendFlags |= ImGuiBackendFlags_HasSetMousePos;          // We can honor io.WantSetMousePos requests (optional, rarely used)
			io.BackendPlatformName = "imgui_impl_glfw";

			DesktopWindow* DesktopWin = dynamic_cast<DesktopWindow*>(t_Window);

			if (!DesktopWin)
			{
				return;
			}

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