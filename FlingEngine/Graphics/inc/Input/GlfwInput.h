#pragma once

#include "Input/IInputDeviceInterface.h"
#include "Input/InputDeviceTypes.h"
#include <memory>

namespace Fling
{
    class FlingWindow;

    /**
     * Make the input interface that gets keyboard, mouse, and gamepad input from GLFW.
     * Add it with Input::AddInterface after the window exists. It is the only place that
     * pumps GLFW events (glfwPollEvents is in its Poll).
     */
    std::shared_ptr<IInputDeviceInterface> CreateGlfwInputDeviceInterface();

    /**
     * The native handle of a window, for Input::NotifyWindowCreated.
     * For a GLFW window, Native is the GLFWwindow*. Returns an invalid handle if the window is null.
     */
    WindowHandle GetInputWindowHandle(FlingWindow* t_Window);
} // namespace Fling
