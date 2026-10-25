#include "GlfwInputDeviceInterface.h"
#include "Input/InputDevice.h"
#include "Input/InputDeviceFactory.h"
#include "Input/InputMessageHandler.h"
#include "FlingPaths.h"
#include "Logger.h"
#include <GLFW/glfw3.h>
#include <cmath>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>

namespace Fling
{
    namespace
    {
        /** The interface that receives the callbacks. Null when no window is set. */
        std::weak_ptr<GlfwInputDeviceInterface> s_Active;

        // The callbacks that were installed before ours. Ours call them first.
        GLFWkeyfun s_PrevKey = nullptr;
        GLFWcharfun s_PrevChar = nullptr;
        GLFWmousebuttonfun s_PrevMouseButton = nullptr;
        GLFWcursorposfun s_PrevCursorPos = nullptr;
        GLFWscrollfun s_PrevScroll = nullptr;
        GLFWwindowfocusfun s_PrevFocus = nullptr;
        GLFWjoystickfun s_PrevJoystick = nullptr;

        InputKey MapMouseButton(int t_Button)
        {
            switch (t_Button)
            {
            case GLFW_MOUSE_BUTTON_1:
                return Keys::MouseLeft;
            case GLFW_MOUSE_BUTTON_2:
                return Keys::MouseRight;
            case GLFW_MOUSE_BUTTON_3:
                return Keys::MouseMiddle;
            case GLFW_MOUSE_BUTTON_4:
                return Keys::MouseButton4;
            case GLFW_MOUSE_BUTTON_5:
                return Keys::MouseButton5;
            case GLFW_MOUSE_BUTTON_6:
                return Keys::MouseButton6;
            case GLFW_MOUSE_BUTTON_7:
                return Keys::MouseButton7;
            case GLFW_MOUSE_BUTTON_8:
                return Keys::MouseButton8;
            default:
                return Keys::Invalid;
            }
        }

        InputKey MapGamepadButton(int t_Button)
        {
            switch (t_Button)
            {
            case GLFW_GAMEPAD_BUTTON_A:
                return Keys::GamepadFaceBottom;
            case GLFW_GAMEPAD_BUTTON_B:
                return Keys::GamepadFaceRight;
            case GLFW_GAMEPAD_BUTTON_X:
                return Keys::GamepadFaceLeft;
            case GLFW_GAMEPAD_BUTTON_Y:
                return Keys::GamepadFaceTop;
            case GLFW_GAMEPAD_BUTTON_LEFT_BUMPER:
                return Keys::GamepadLeftShoulder;
            case GLFW_GAMEPAD_BUTTON_RIGHT_BUMPER:
                return Keys::GamepadRightShoulder;
            case GLFW_GAMEPAD_BUTTON_BACK:
                return Keys::GamepadBack;
            case GLFW_GAMEPAD_BUTTON_START:
                return Keys::GamepadStart;
            case GLFW_GAMEPAD_BUTTON_GUIDE:
                return Keys::GamepadGuide;
            case GLFW_GAMEPAD_BUTTON_LEFT_THUMB:
                return Keys::GamepadLeftThumb;
            case GLFW_GAMEPAD_BUTTON_RIGHT_THUMB:
                return Keys::GamepadRightThumb;
            case GLFW_GAMEPAD_BUTTON_DPAD_UP:
                return Keys::GamepadDPadUp;
            case GLFW_GAMEPAD_BUTTON_DPAD_RIGHT:
                return Keys::GamepadDPadRight;
            case GLFW_GAMEPAD_BUTTON_DPAD_DOWN:
                return Keys::GamepadDPadDown;
            case GLFW_GAMEPAD_BUTTON_DPAD_LEFT:
                return Keys::GamepadDPadLeft;
            default:
                return Keys::Invalid;
            }
        }

        InputKey MapGamepadAxis(int t_Axis)
        {
            switch (t_Axis)
            {
            case GLFW_GAMEPAD_AXIS_LEFT_X:
                return Keys::GamepadLeftX;
            case GLFW_GAMEPAD_AXIS_LEFT_Y:
                return Keys::GamepadLeftY;
            case GLFW_GAMEPAD_AXIS_RIGHT_X:
                return Keys::GamepadRightX;
            case GLFW_GAMEPAD_AXIS_RIGHT_Y:
                return Keys::GamepadRightY;
            case GLFW_GAMEPAD_AXIS_LEFT_TRIGGER:
                return Keys::GamepadLeftTrigger;
            case GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER:
                return Keys::GamepadRightTrigger;
            default:
                return Keys::Invalid;
            }
        }

        /**
         * Read the USB vendor and product IDs from an SDL-style joystick GUID, for example
         * "030000005e040000120b000017050000" gives 045E:0B12. Each ID is 2 bytes, little
         * endian, at hex characters 8 and 16. GUIDs without IDs in them give 0.
         */
        void ReadIdsFromGuid(const char* t_Guid, uint16& t_OutVendor, uint16& t_OutProduct)
        {
            t_OutVendor = 0;
            t_OutProduct = 0;
            if (t_Guid == nullptr || std::strlen(t_Guid) < 32)
            {
                return;
            }
            // Characters 12 to 15 must be zero, or this GUID holds a name, not IDs.
            if (std::strncmp(t_Guid + 12, "0000", 4) != 0)
            {
                return;
            }
            auto ReadLittleEndian16 = [t_Guid](size_t t_Offset)
            {
                const std::string Low(t_Guid + t_Offset, 2);
                const std::string High(t_Guid + t_Offset + 2, 2);
                return static_cast<uint16>(std::stoul(High + Low, nullptr, 16));
            };
            t_OutVendor = ReadLittleEndian16(8);
            t_OutProduct = ReadLittleEndian16(16);
        }

        /**
         * Remove our callback from a chain. If another callback was installed after ours,
         * it stays, and ours stays below it as a pass-through (s_Active is empty by then).
         */
        template <typename Fn, typename SetFn>
        void RestoreCallback(SetFn t_Set, GLFWwindow* t_Window, Fn t_Ours, Fn& t_Prev)
        {
            Fn Current = t_Set(t_Window, t_Prev);
            if (Current != t_Ours)
            {
                t_Set(t_Window, Current);
            }
            else
            {
                t_Prev = nullptr;
            }
        }
    } // namespace

    GlfwInputDeviceInterface::GlfwInputDeviceInterface() = default;

    GlfwInputDeviceInterface::~GlfwInputDeviceInterface() = default;

    InputKey GlfwInputDeviceInterface::MapKey(int t_GlfwKey)
    {
        switch (t_GlfwKey)
        {
        case GLFW_KEY_SPACE:
            return Keys::Space;
        case GLFW_KEY_APOSTROPHE:
            return Keys::Apostrophe;
        case GLFW_KEY_COMMA:
            return Keys::Comma;
        case GLFW_KEY_MINUS:
            return Keys::Minus;
        case GLFW_KEY_PERIOD:
            return Keys::Period;
        case GLFW_KEY_SLASH:
            return Keys::Slash;
        case GLFW_KEY_0:
            return Keys::Zero;
        case GLFW_KEY_1:
            return Keys::One;
        case GLFW_KEY_2:
            return Keys::Two;
        case GLFW_KEY_3:
            return Keys::Three;
        case GLFW_KEY_4:
            return Keys::Four;
        case GLFW_KEY_5:
            return Keys::Five;
        case GLFW_KEY_6:
            return Keys::Six;
        case GLFW_KEY_7:
            return Keys::Seven;
        case GLFW_KEY_8:
            return Keys::Eight;
        case GLFW_KEY_9:
            return Keys::Nine;
        case GLFW_KEY_SEMICOLON:
            return Keys::Semicolon;
        case GLFW_KEY_EQUAL:
            return Keys::Equal;
        case GLFW_KEY_A:
            return Keys::A;
        case GLFW_KEY_B:
            return Keys::B;
        case GLFW_KEY_C:
            return Keys::C;
        case GLFW_KEY_D:
            return Keys::D;
        case GLFW_KEY_E:
            return Keys::E;
        case GLFW_KEY_F:
            return Keys::F;
        case GLFW_KEY_G:
            return Keys::G;
        case GLFW_KEY_H:
            return Keys::H;
        case GLFW_KEY_I:
            return Keys::I;
        case GLFW_KEY_J:
            return Keys::J;
        case GLFW_KEY_K:
            return Keys::K;
        case GLFW_KEY_L:
            return Keys::L;
        case GLFW_KEY_M:
            return Keys::M;
        case GLFW_KEY_N:
            return Keys::N;
        case GLFW_KEY_O:
            return Keys::O;
        case GLFW_KEY_P:
            return Keys::P;
        case GLFW_KEY_Q:
            return Keys::Q;
        case GLFW_KEY_R:
            return Keys::R;
        case GLFW_KEY_S:
            return Keys::S;
        case GLFW_KEY_T:
            return Keys::T;
        case GLFW_KEY_U:
            return Keys::U;
        case GLFW_KEY_V:
            return Keys::V;
        case GLFW_KEY_W:
            return Keys::W;
        case GLFW_KEY_X:
            return Keys::X;
        case GLFW_KEY_Y:
            return Keys::Y;
        case GLFW_KEY_Z:
            return Keys::Z;
        case GLFW_KEY_LEFT_BRACKET:
            return Keys::LeftBracket;
        case GLFW_KEY_BACKSLASH:
            return Keys::Backslash;
        case GLFW_KEY_RIGHT_BRACKET:
            return Keys::RightBracket;
        case GLFW_KEY_GRAVE_ACCENT:
            return Keys::GraveAccent;
        case GLFW_KEY_WORLD_1:
            return Keys::World1;
        case GLFW_KEY_WORLD_2:
            return Keys::World2;
        case GLFW_KEY_ESCAPE:
            return Keys::Escape;
        case GLFW_KEY_ENTER:
            return Keys::Enter;
        case GLFW_KEY_TAB:
            return Keys::Tab;
        case GLFW_KEY_BACKSPACE:
            return Keys::Backspace;
        case GLFW_KEY_INSERT:
            return Keys::Insert;
        case GLFW_KEY_DELETE:
            return Keys::Delete;
        case GLFW_KEY_RIGHT:
            return Keys::Right;
        case GLFW_KEY_LEFT:
            return Keys::Left;
        case GLFW_KEY_DOWN:
            return Keys::Down;
        case GLFW_KEY_UP:
            return Keys::Up;
        case GLFW_KEY_PAGE_UP:
            return Keys::PageUp;
        case GLFW_KEY_PAGE_DOWN:
            return Keys::PageDown;
        case GLFW_KEY_HOME:
            return Keys::Home;
        case GLFW_KEY_END:
            return Keys::End;
        case GLFW_KEY_CAPS_LOCK:
            return Keys::CapsLock;
        case GLFW_KEY_SCROLL_LOCK:
            return Keys::ScrollLock;
        case GLFW_KEY_NUM_LOCK:
            return Keys::NumLock;
        case GLFW_KEY_PRINT_SCREEN:
            return Keys::PrintScreen;
        case GLFW_KEY_PAUSE:
            return Keys::Pause;
        case GLFW_KEY_F1:
            return Keys::F1;
        case GLFW_KEY_F2:
            return Keys::F2;
        case GLFW_KEY_F3:
            return Keys::F3;
        case GLFW_KEY_F4:
            return Keys::F4;
        case GLFW_KEY_F5:
            return Keys::F5;
        case GLFW_KEY_F6:
            return Keys::F6;
        case GLFW_KEY_F7:
            return Keys::F7;
        case GLFW_KEY_F8:
            return Keys::F8;
        case GLFW_KEY_F9:
            return Keys::F9;
        case GLFW_KEY_F10:
            return Keys::F10;
        case GLFW_KEY_F11:
            return Keys::F11;
        case GLFW_KEY_F12:
            return Keys::F12;
        case GLFW_KEY_F13:
            return Keys::F13;
        case GLFW_KEY_F14:
            return Keys::F14;
        case GLFW_KEY_F15:
            return Keys::F15;
        case GLFW_KEY_F16:
            return Keys::F16;
        case GLFW_KEY_F17:
            return Keys::F17;
        case GLFW_KEY_F18:
            return Keys::F18;
        case GLFW_KEY_F19:
            return Keys::F19;
        case GLFW_KEY_F20:
            return Keys::F20;
        case GLFW_KEY_F21:
            return Keys::F21;
        case GLFW_KEY_F22:
            return Keys::F22;
        case GLFW_KEY_F23:
            return Keys::F23;
        case GLFW_KEY_F24:
            return Keys::F24;
        case GLFW_KEY_F25:
            return Keys::F25;
        case GLFW_KEY_KP_0:
            return Keys::Keypad0;
        case GLFW_KEY_KP_1:
            return Keys::Keypad1;
        case GLFW_KEY_KP_2:
            return Keys::Keypad2;
        case GLFW_KEY_KP_3:
            return Keys::Keypad3;
        case GLFW_KEY_KP_4:
            return Keys::Keypad4;
        case GLFW_KEY_KP_5:
            return Keys::Keypad5;
        case GLFW_KEY_KP_6:
            return Keys::Keypad6;
        case GLFW_KEY_KP_7:
            return Keys::Keypad7;
        case GLFW_KEY_KP_8:
            return Keys::Keypad8;
        case GLFW_KEY_KP_9:
            return Keys::Keypad9;
        case GLFW_KEY_KP_DECIMAL:
            return Keys::KeypadDecimal;
        case GLFW_KEY_KP_DIVIDE:
            return Keys::KeypadDivide;
        case GLFW_KEY_KP_MULTIPLY:
            return Keys::KeypadMultiply;
        case GLFW_KEY_KP_SUBTRACT:
            return Keys::KeypadSubtract;
        case GLFW_KEY_KP_ADD:
            return Keys::KeypadAdd;
        case GLFW_KEY_KP_ENTER:
            return Keys::KeypadEnter;
        case GLFW_KEY_KP_EQUAL:
            return Keys::KeypadEqual;
        case GLFW_KEY_LEFT_SHIFT:
            return Keys::LeftShift;
        case GLFW_KEY_LEFT_CONTROL:
            return Keys::LeftControl;
        case GLFW_KEY_LEFT_ALT:
            return Keys::LeftAlt;
        case GLFW_KEY_LEFT_SUPER:
            return Keys::LeftSuper;
        case GLFW_KEY_RIGHT_SHIFT:
            return Keys::RightShift;
        case GLFW_KEY_RIGHT_CONTROL:
            return Keys::RightControl;
        case GLFW_KEY_RIGHT_ALT:
            return Keys::RightAlt;
        case GLFW_KEY_RIGHT_SUPER:
            return Keys::RightSuper;
        case GLFW_KEY_MENU:
            return Keys::Menu;
        default:
            return Keys::Invalid;
        }
    }

    void GlfwInputDeviceInterface::Init(const std::shared_ptr<InputMessageHandler>& t_Handler)
    {
        m_Handler = t_Handler;

        LoadGamepadMappings();

        InputDeviceInfo KeyboardInfo;
        KeyboardInfo.Type = InputDeviceType::Keyboard;
        KeyboardInfo.DisplayName = "Keyboard";
        KeyboardInfo.HardwarePath = "glfw:keyboard";
        KeyboardInfo.Capabilities = InputDeviceCapabilities::Keys;
        m_Keyboard = t_Handler->GetDeviceFactory().CreateDevice(KeyboardInfo, *this)->GetId();

        InputDeviceInfo MouseInfo;
        MouseInfo.Type = InputDeviceType::Mouse;
        MouseInfo.DisplayName = "Mouse";
        MouseInfo.HardwarePath = "glfw:mouse";
        MouseInfo.Capabilities = InputDeviceCapabilities::MouseButtons | InputDeviceCapabilities::Pointer |
                                 InputDeviceCapabilities::RelativeMotion | InputDeviceCapabilities::Wheel;
        m_Mouse = t_Handler->GetDeviceFactory().CreateDevice(MouseInfo, *this)->GetId();

        for (int Jid = GLFW_JOYSTICK_1; Jid < MaxGamepads; ++Jid)
        {
            if (glfwJoystickPresent(Jid))
            {
                TryConnectJoystick(Jid);
            }
        }
    }

    void GlfwInputDeviceInterface::LoadGamepadMappings()
    {
        // Load the file only once. The mappings stay in GLFW until glfwTerminate.
        static bool s_Loaded = false;
        if (s_Loaded)
        {
            return;
        }
        s_Loaded = true;

        const std::string Path = FlingPaths::EngineConfigDir() + "/gamecontrollerdb.txt";
        std::ifstream File(Path);
        if (!File)
        {
            F_LOG_WARN("[Input] No gamepad mapping file at {}. Only GLFW's built-in mappings are used.", Path);
            return;
        }
        std::stringstream Text;
        Text << File.rdbuf();
        if (glfwUpdateGamepadMappings(Text.str().c_str()) == GLFW_TRUE)
        {
            F_LOG_TRACE("[Input] Loaded gamepad mappings from {}", Path);
        }
        else
        {
            F_LOG_WARN("[Input] Could not read the gamepad mappings in {}", Path);
        }
    }

    void GlfwInputDeviceInterface::TryConnectJoystick(int t_JoystickId)
    {
        if (glfwJoystickIsGamepad(t_JoystickId))
        {
            ConnectGamepad(t_JoystickId);
            return;
        }
        // Not an error: keyboards and other HID devices can show up as joysticks too.
        const char* Name = glfwGetJoystickName(t_JoystickId);
        const char* Guid = glfwGetJoystickGUID(t_JoystickId);
        F_LOG_TRACE("[Input] Joystick '{}' (GUID {}) has no gamepad mapping, so it is not used. Add a mapping for it "
                    "to Config/gamecontrollerdb.txt.",
                    Name ? Name : "",
                    Guid ? Guid : "");
    }

    void GlfwInputDeviceInterface::Shutdown()
    {
        if (m_Window)
        {
            // Poll is the only place that pumps GLFW events, so the window stops getting events too.
            F_LOG_WARN("The GLFW input interface was removed while its window exists. The window gets no more events.");
            OnWindowDestroyed(WindowHandle{m_Window});
        }
        if (s_Active.lock().get() == this)
        {
            s_Active.reset();
        }
    }

    void GlfwInputDeviceInterface::OnWindowCreated(WindowHandle t_Window)
    {
        if (!t_Window.IsValid())
        {
            return;
        }
        if (m_Window)
        {
            OnWindowDestroyed(WindowHandle{m_Window});
        }

        m_Window = static_cast<GLFWwindow*>(t_Window.Native);
        m_HasLastCursor = false;
        s_Active = weak_from_this();

        // The window user pointer is free, but the callbacks are static and only one window is used.
        s_PrevKey = glfwSetKeyCallback(m_Window, &GlfwInputDeviceInterface::KeyCallback);
        s_PrevChar = glfwSetCharCallback(m_Window, &GlfwInputDeviceInterface::CharCallback);
        s_PrevMouseButton = glfwSetMouseButtonCallback(m_Window, &GlfwInputDeviceInterface::MouseButtonCallback);
        s_PrevCursorPos = glfwSetCursorPosCallback(m_Window, &GlfwInputDeviceInterface::CursorPosCallback);
        s_PrevScroll = glfwSetScrollCallback(m_Window, &GlfwInputDeviceInterface::ScrollCallback);
        s_PrevFocus = glfwSetWindowFocusCallback(m_Window, &GlfwInputDeviceInterface::FocusCallback);
        s_PrevJoystick = glfwSetJoystickCallback(&GlfwInputDeviceInterface::JoystickCallback);

        if (const std::shared_ptr<InputMessageHandler> Handler = m_Handler.lock())
        {
            if (const std::shared_ptr<InputDevice> Keyboard = Handler->GetDeviceFactory().Find(m_Keyboard))
            {
                Keyboard->SetWindow(t_Window);
            }
            if (const std::shared_ptr<InputDevice> Mouse = Handler->GetDeviceFactory().Find(m_Mouse))
            {
                Mouse->SetWindow(t_Window);
            }
        }
    }

    void GlfwInputDeviceInterface::OnWindowDestroyed(WindowHandle t_Window)
    {
        if (!m_Window || t_Window.Native != m_Window)
        {
            return;
        }

        s_Active.reset();

        RestoreCallback(glfwSetKeyCallback, m_Window, &GlfwInputDeviceInterface::KeyCallback, s_PrevKey);
        RestoreCallback(glfwSetCharCallback, m_Window, &GlfwInputDeviceInterface::CharCallback, s_PrevChar);
        RestoreCallback(
            glfwSetMouseButtonCallback, m_Window, &GlfwInputDeviceInterface::MouseButtonCallback, s_PrevMouseButton);
        RestoreCallback(
            glfwSetCursorPosCallback, m_Window, &GlfwInputDeviceInterface::CursorPosCallback, s_PrevCursorPos);
        RestoreCallback(glfwSetScrollCallback, m_Window, &GlfwInputDeviceInterface::ScrollCallback, s_PrevScroll);
        RestoreCallback(glfwSetWindowFocusCallback, m_Window, &GlfwInputDeviceInterface::FocusCallback, s_PrevFocus);

        GLFWjoystickfun CurrentJoystick = glfwSetJoystickCallback(s_PrevJoystick);
        if (CurrentJoystick != &GlfwInputDeviceInterface::JoystickCallback)
        {
            glfwSetJoystickCallback(CurrentJoystick);
        }
        else
        {
            s_PrevJoystick = nullptr;
        }

        if (const std::shared_ptr<InputMessageHandler> Handler = m_Handler.lock())
        {
            if (const std::shared_ptr<InputDevice> Keyboard = Handler->GetDeviceFactory().Find(m_Keyboard))
            {
                Keyboard->SetWindow(WindowHandle{});
            }
            if (const std::shared_ptr<InputDevice> Mouse = Handler->GetDeviceFactory().Find(m_Mouse))
            {
                Mouse->SetWindow(WindowHandle{});
            }
        }

        m_Window = nullptr;
    }

    void GlfwInputDeviceInterface::Poll(int64 t_NowNs)
    {
        // The only place that pumps GLFW events. The callbacks push the keyboard and mouse input.
        glfwPollEvents();

        for (int Jid = 0; Jid < MaxGamepads; ++Jid)
        {
            GamepadSlot& Slot = m_Gamepads[Jid];
            if (Slot.Device.IsValid())
            {
                PollGamepad(Jid, Slot);
            }
        }
    }

    void
    GlfwInputDeviceInterface::KeyCallback(GLFWwindow* t_Window, int t_Key, int t_Scancode, int t_Action, int t_Mods)
    {
        if (s_PrevKey)
        {
            s_PrevKey(t_Window, t_Key, t_Scancode, t_Action, t_Mods);
        }
        if (const std::shared_ptr<GlfwInputDeviceInterface> Active = s_Active.lock();
            Active && t_Window == Active->m_Window)
        {
            Active->HandleKey(t_Key, t_Scancode, t_Action);
        }
    }

    void GlfwInputDeviceInterface::CharCallback(GLFWwindow* t_Window, unsigned int t_CodePoint)
    {
        if (s_PrevChar)
        {
            s_PrevChar(t_Window, t_CodePoint);
        }
        if (const std::shared_ptr<GlfwInputDeviceInterface> Active = s_Active.lock();
            Active && t_Window == Active->m_Window)
        {
            Active->HandleChar(t_CodePoint);
        }
    }

    void GlfwInputDeviceInterface::MouseButtonCallback(GLFWwindow* t_Window, int t_Button, int t_Action, int t_Mods)
    {
        if (s_PrevMouseButton)
        {
            s_PrevMouseButton(t_Window, t_Button, t_Action, t_Mods);
        }
        if (const std::shared_ptr<GlfwInputDeviceInterface> Active = s_Active.lock();
            Active && t_Window == Active->m_Window)
        {
            Active->HandleMouseButton(t_Button, t_Action);
        }
    }

    void GlfwInputDeviceInterface::CursorPosCallback(GLFWwindow* t_Window, double t_X, double t_Y)
    {
        if (s_PrevCursorPos)
        {
            s_PrevCursorPos(t_Window, t_X, t_Y);
        }
        if (const std::shared_ptr<GlfwInputDeviceInterface> Active = s_Active.lock();
            Active && t_Window == Active->m_Window)
        {
            Active->HandleCursorPos(t_X, t_Y);
        }
    }

    void GlfwInputDeviceInterface::ScrollCallback(GLFWwindow* t_Window, double t_X, double t_Y)
    {
        if (s_PrevScroll)
        {
            s_PrevScroll(t_Window, t_X, t_Y);
        }
        if (const std::shared_ptr<GlfwInputDeviceInterface> Active = s_Active.lock();
            Active && t_Window == Active->m_Window)
        {
            Active->HandleScroll(t_X, t_Y);
        }
    }

    void GlfwInputDeviceInterface::FocusCallback(GLFWwindow* t_Window, int t_Focused)
    {
        if (s_PrevFocus)
        {
            s_PrevFocus(t_Window, t_Focused);
        }
        if (const std::shared_ptr<GlfwInputDeviceInterface> Active = s_Active.lock();
            Active && t_Window == Active->m_Window)
        {
            Active->HandleFocus(t_Focused != 0);
        }
    }

    void GlfwInputDeviceInterface::JoystickCallback(int t_JoystickId, int t_Event)
    {
        if (s_PrevJoystick)
        {
            s_PrevJoystick(t_JoystickId, t_Event);
        }
        if (const std::shared_ptr<GlfwInputDeviceInterface> Active = s_Active.lock())
        {
            Active->HandleJoystick(t_JoystickId, t_Event == GLFW_CONNECTED);
        }
    }

    void GlfwInputDeviceInterface::HandleKey(int t_Key, int t_Scancode, int t_Action)
    {
        const std::shared_ptr<InputMessageHandler> Handler = m_Handler.lock();
        if (!Handler)
        {
            return;
        }
        const InputKey Key = MapKey(t_Key);
        if (!Key.IsValid())
        {
            return;
        }
        const std::shared_ptr<InputDevice> Keyboard = Handler->GetDeviceFactory().Find(m_Keyboard);
        if (!Keyboard)
        {
            return;
        }

        const uint32 Scancode = static_cast<uint32>(t_Scancode);
        if (t_Action == GLFW_PRESS)
        {
            Keyboard->PushKey(Key, true, Scancode);
        }
        else if (t_Action == GLFW_RELEASE)
        {
            Keyboard->PushKey(Key, false, Scancode);
        }
        else if (t_Action == GLFW_REPEAT)
        {
            Keyboard->PushRepeat(Key, Scancode);
        }
    }

    void GlfwInputDeviceInterface::HandleChar(unsigned int t_CodePoint)
    {
        const std::shared_ptr<InputMessageHandler> Handler = m_Handler.lock();
        if (!Handler)
        {
            return;
        }
        if (const std::shared_ptr<InputDevice> Keyboard = Handler->GetDeviceFactory().Find(m_Keyboard))
        {
            Keyboard->PushChar(t_CodePoint);
        }
    }

    void GlfwInputDeviceInterface::HandleMouseButton(int t_Button, int t_Action)
    {
        const std::shared_ptr<InputMessageHandler> Handler = m_Handler.lock();
        if (!Handler)
        {
            return;
        }
        const InputKey Key = MapMouseButton(t_Button);
        const std::shared_ptr<InputDevice> Mouse = Handler->GetDeviceFactory().Find(m_Mouse);
        if (!Key.IsValid() || !Mouse)
        {
            return;
        }
        if (t_Action == GLFW_PRESS)
        {
            Mouse->PushKey(Key, true);
        }
        else if (t_Action == GLFW_RELEASE)
        {
            Mouse->PushKey(Key, false);
        }
    }

    void GlfwInputDeviceInterface::HandleCursorPos(double t_X, double t_Y)
    {
        const std::shared_ptr<InputMessageHandler> Handler = m_Handler.lock();
        if (!Handler)
        {
            return;
        }
        const std::shared_ptr<InputDevice> Mouse = Handler->GetDeviceFactory().Find(m_Mouse);
        if (!Mouse)
        {
            return;
        }

        int32 DeltaX = 0;
        int32 DeltaY = 0;
        if (m_HasLastCursor)
        {
            DeltaX = static_cast<int32>(std::lround(t_X - m_LastCursorX));
            DeltaY = static_cast<int32>(std::lround(t_Y - m_LastCursorY));
        }
        m_HasLastCursor = true;
        m_LastCursorX = t_X;
        m_LastCursorY = t_Y;

        Mouse->PushMouseMove(DeltaX, DeltaY, static_cast<float>(t_X), static_cast<float>(t_Y));
    }

    void GlfwInputDeviceInterface::HandleScroll(double t_X, double t_Y)
    {
        const std::shared_ptr<InputMessageHandler> Handler = m_Handler.lock();
        if (!Handler)
        {
            return;
        }
        if (const std::shared_ptr<InputDevice> Mouse = Handler->GetDeviceFactory().Find(m_Mouse))
        {
            Mouse->PushWheel(static_cast<float>(t_X), static_cast<float>(t_Y));
        }
    }

    void GlfwInputDeviceInterface::HandleFocus(bool t_HasFocus)
    {
        const std::shared_ptr<InputMessageHandler> Handler = m_Handler.lock();
        if (!Handler)
        {
            return;
        }
        if (!t_HasFocus)
        {
            // The cursor can move while the window is not focused, so the next delta starts again
            m_HasLastCursor = false;
        }
        if (const std::shared_ptr<InputDevice> Keyboard = Handler->GetDeviceFactory().Find(m_Keyboard))
        {
            Keyboard->PushFocus(t_HasFocus);
        }
        if (const std::shared_ptr<InputDevice> Mouse = Handler->GetDeviceFactory().Find(m_Mouse))
        {
            Mouse->PushFocus(t_HasFocus);
        }
    }

    void GlfwInputDeviceInterface::HandleJoystick(int t_JoystickId, bool t_Connected)
    {
        if (t_JoystickId < 0 || t_JoystickId >= MaxGamepads)
        {
            return;
        }
        if (t_Connected)
        {
            TryConnectJoystick(t_JoystickId);
        }
        else
        {
            DisconnectGamepad(t_JoystickId);
        }
    }

    void GlfwInputDeviceInterface::ConnectGamepad(int t_JoystickId)
    {
        const std::shared_ptr<InputMessageHandler> Handler = m_Handler.lock();
        if (!Handler || t_JoystickId < 0 || t_JoystickId >= MaxGamepads)
        {
            return;
        }

        const char* Guid = glfwGetJoystickGUID(t_JoystickId);
        const char* Name = glfwGetGamepadName(t_JoystickId);

        InputDeviceInfo Info;
        Info.Type = InputDeviceType::Gamepad;
        Info.DisplayName = Name ? Name : "Gamepad";
        Info.HardwarePath = std::string("glfw:") + (Guid ? Guid : "");
        ReadIdsFromGuid(Guid, Info.VendorId, Info.ProductId);
        Info.Capabilities =
            InputDeviceCapabilities::Buttons | InputDeviceCapabilities::Thumbsticks | InputDeviceCapabilities::Triggers;

        GamepadSlot& Slot = m_Gamepads[t_JoystickId];
        Slot = GamepadSlot{};
        Slot.Device = Handler->GetDeviceFactory().CreateDevice(Info, *this)->GetId();
    }

    void GlfwInputDeviceInterface::DisconnectGamepad(int t_JoystickId)
    {
        GamepadSlot& Slot = m_Gamepads[t_JoystickId];
        const std::shared_ptr<InputMessageHandler> Handler = m_Handler.lock();
        if (Handler && Slot.Device.IsValid())
        {
            Handler->GetDeviceFactory().DisconnectDevice(Slot.Device);
        }
        Slot = GamepadSlot{};
    }

    void GlfwInputDeviceInterface::PollGamepad(int t_JoystickId, GamepadSlot& t_Slot)
    {
        const std::shared_ptr<InputMessageHandler> Handler = m_Handler.lock();
        const std::shared_ptr<InputDevice> Device = Handler ? Handler->GetDeviceFactory().Find(t_Slot.Device) : nullptr;
        if (!Device)
        {
            return;
        }

        GLFWgamepadstate State;
        if (!glfwGetGamepadState(t_JoystickId, &State))
        {
            return;
        }

        for (int Button = 0; Button <= GLFW_GAMEPAD_BUTTON_LAST; ++Button)
        {
            const uint8_t Value = State.buttons[Button] == GLFW_PRESS ? 1 : 0;
            const InputKey Key = MapGamepadButton(Button);
            // On the first poll only pressed buttons need a message
            const bool Changed = t_Slot.HasPrevious ? t_Slot.Buttons[Button] != Value : Value != 0;
            if (Key.IsValid() && Changed)
            {
                Device->PushKey(Key, Value != 0);
            }
            t_Slot.Buttons[Button] = Value;
        }

        for (int Axis = 0; Axis <= GLFW_GAMEPAD_AXIS_LAST; ++Axis)
        {
            float Value = State.axes[Axis];
            if (Axis == GLFW_GAMEPAD_AXIS_LEFT_TRIGGER || Axis == GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER)
            {
                // GLFW triggers are -1 (released) to 1 (pressed)
                Value = (Value + 1.0f) * 0.5f;
            }
            const InputKey Key = MapGamepadAxis(Axis);
            const bool Changed = t_Slot.HasPrevious ? t_Slot.Axes[Axis] != Value : Value != 0.0f;
            if (Key.IsValid() && Changed)
            {
                Device->PushAxis(Key, Value);
            }
            t_Slot.Axes[Axis] = Value;
        }

        t_Slot.HasPrevious = true;
    }
} // namespace Fling
