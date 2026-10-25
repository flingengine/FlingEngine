#pragma once

#include "FlingTypes.h"
#include "Input/InputDeviceTypes.h"
#include <memory>

namespace Fling
{
    class InputMessageHandler;

    /**
     * One input backend: GLFW, XInput, GameInput, the Win32 message pump, or a virtual
     * device. A backend finds devices, makes an InputDevice for each one through the
     * InputDeviceFactory, and calls the InputDevice Push functions for its input.
     *
     * Add one with InputMessageHandler::AddInterface (or Input::AddInterface) at any
     * time, and remove it again with RemoveInterface. The handler owns interfaces with a
     * shared_ptr. Keep a std::weak_ptr to the handler, not a raw pointer.
     */
    class IInputDeviceInterface
    {
    public:
        virtual ~IInputDeviceInterface() = default;

        /** A name that is the same in every session, for example "GLFW". Used to match devices that connect again. */
        virtual const char* GetName() const = 0;

        /**
         * True if code makes this interface's input (playback, tests, scripts), not a
         * physical device. Its devices and their messages are marked as simulated.
         */
        virtual bool IsSimulated() const { return false; }

        /**
         * Called once, when the interface is added. Create the devices that already exist
         * here. Keep t_Handler as a std::weak_ptr: the interface must not keep the handler
         * alive.
         */
        virtual void Init(const std::shared_ptr<InputMessageHandler>& t_Handler) = 0;

        /**
         * Called once, before the interface is destroyed. The handler disconnects every
         * device of this interface after this call.
         */
        virtual void Shutdown() = 0;

        /** True if Poll must be called now. Polled devices (gamepads) use this to poll at their own rate. */
        virtual bool WantsPoll(int64 t_NowNs) const { return true; }

        /** Read input and push it to the devices of this interface. */
        virtual void Poll(int64 t_NowNs) = 0;

        /** A window was created, or this interface was added after the window was created. */
        virtual void OnWindowCreated(WindowHandle t_Window) {}

        /** The window will be destroyed. Remove anything that uses it. */
        virtual void OnWindowDestroyed(WindowHandle t_Window) {}

        /** The ID that the handler gave this interface. Invalid until it is added. */
        InputInterfaceId GetInterfaceId() const { return m_InterfaceId; }

    private:
        friend class InputMessageHandler;

        InputInterfaceId m_InterfaceId;
    };
} // namespace Fling
