#pragma once

#include "Input/IInputDeviceInterface.h"
#include "Input/InputDeviceTypes.h"
#include "Input/InputKey.h"
#include <array>
#include <cstdint>
#include <memory>

struct GLFWwindow;

namespace Fling
{
    class InputDevice;
    class InputMessageHandler;

    /**
     * Gets keyboard, mouse, and gamepad input from GLFW. The window callbacks are chained:
     * each one calls the callback that was installed before it, so ImGui's GLFW binding
     * and this interface both see every event, in any install order.
     *
     * Only one GLFW window is supported. Poll calls glfwPollEvents, so this is the only
     * place that pumps GLFW events.
     *
     * The handler owns this interface with a shared_ptr. The GLFW callbacks reach it
     * through a weak_ptr, so a callback after the interface is gone does nothing.
     */
    class GlfwInputDeviceInterface : public IInputDeviceInterface,
                                     public std::enable_shared_from_this<GlfwInputDeviceInterface>
    {
    public:
        GlfwInputDeviceInterface();

        ~GlfwInputDeviceInterface() override;

        const char* GetName() const override { return "GLFW"; }

        void Init(const std::shared_ptr<InputMessageHandler>& t_Handler) override;

        void Shutdown() override;

        void Poll(int64 t_NowNs) override;

        void OnWindowCreated(WindowHandle t_Window) override;

        void OnWindowDestroyed(WindowHandle t_Window) override;

        /** Map a GLFW key code to an InputKey. Returns an invalid key for an unknown code. */
        static InputKey MapKey(int t_GlfwKey);

    private:
        /** The newest gamepad values that were pushed. */
        struct GamepadSlot
        {
            InputDeviceId Device;
            bool HasPrevious = false;
            std::array<uint8_t, 15> Buttons{};
            std::array<float, 6> Axes{};
        };

        static constexpr int MaxGamepads = 16;

        static void KeyCallback(GLFWwindow* t_Window, int t_Key, int t_Scancode, int t_Action, int t_Mods);
        static void CharCallback(GLFWwindow* t_Window, unsigned int t_CodePoint);
        static void MouseButtonCallback(GLFWwindow* t_Window, int t_Button, int t_Action, int t_Mods);
        static void CursorPosCallback(GLFWwindow* t_Window, double t_X, double t_Y);
        static void ScrollCallback(GLFWwindow* t_Window, double t_X, double t_Y);
        static void FocusCallback(GLFWwindow* t_Window, int t_Focused);
        static void JoystickCallback(int t_JoystickId, int t_Event);

        void HandleKey(int t_Key, int t_Scancode, int t_Action);
        void HandleChar(unsigned int t_CodePoint);
        void HandleMouseButton(int t_Button, int t_Action);
        void HandleCursorPos(double t_X, double t_Y);
        void HandleScroll(double t_X, double t_Y);
        void HandleFocus(bool t_HasFocus);
        void HandleJoystick(int t_JoystickId, bool t_Connected);

        /**
         * Load more gamepad mappings from Config/gamecontrollerdb.txt (SDL_GameControllerDB
         * format). GLFW has built-in mappings, but new controllers and firmware versions
         * are often missing from them. Without a mapping, GLFW does not report a joystick
         * as a gamepad.
         */
        static void LoadGamepadMappings();

        /** Make a gamepad device if GLFW has a gamepad mapping for this joystick. Log it if not. */
        void TryConnectJoystick(int t_JoystickId);

        void ConnectGamepad(int t_JoystickId);
        void DisconnectGamepad(int t_JoystickId);
        void PollGamepad(int t_JoystickId, GamepadSlot& t_Slot);

        std::weak_ptr<InputMessageHandler> m_Handler;

        InputDeviceId m_Keyboard;

        InputDeviceId m_Mouse;

        GLFWwindow* m_Window = nullptr;

        bool m_HasLastCursor = false;

        double m_LastCursorX = 0.0;

        double m_LastCursorY = 0.0;

        std::array<GamepadSlot, MaxGamepads> m_Gamepads;
    };
} // namespace Fling
