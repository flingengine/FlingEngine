#include "Input/GlfwInput.h"
#include "DesktopWindow.h"
#include "GlfwInputDeviceInterface.h"

namespace Fling
{
    std::shared_ptr<IInputDeviceInterface> CreateGlfwInputDeviceInterface()
    {
        // make_shared, so the interface can give the GLFW callbacks a weak_ptr to itself.
        return std::make_shared<GlfwInputDeviceInterface>();
    }

    WindowHandle GetInputWindowHandle(FlingWindow* t_Window)
    {
        DesktopWindow* Desktop = dynamic_cast<DesktopWindow*>(t_Window);
        if (!Desktop)
        {
            return WindowHandle{};
        }
        return WindowHandle{Desktop->GetGlfwWindow()};
    }
} // namespace Fling
