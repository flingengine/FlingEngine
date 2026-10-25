#pragma once

#include "FlingTypes.h"
#include "NonCopyable.hpp"
#include "Input/InputDeviceTypes.h"
#include "Input/InputKey.h"
#include "Input/InputKeyState.h"
#include <memory>

namespace Fling
{
    class InputMessageHandler;

    /**
     * One human interface device: one keyboard, one mouse, one gamepad, one touch
     * screen. InputDeviceFactory makes it and gives it a unique ID.
     *
     * The interface that owns the device calls the Push functions. Each one makes an
     * InputMessage and sends it to the InputMessageHandler. A Push on a device that is
     * not connected does nothing. A time of 0 means "now" (InputNowNs).
     *
     * The factory owns devices with a shared_ptr, so code can keep a device. A device
     * keeps only a weak_ptr to its handler: after the handler is destroyed, the Push
     * functions do nothing.
     */
    class InputDevice : public NonCopyable
    {
    public:
        InputDevice(InputDeviceId t_Id,
                    const InputDeviceInfo& t_Info,
                    InputInterfaceId t_Owner,
                    std::weak_ptr<InputMessageHandler> t_Handler);

        InputDeviceId GetId() const { return m_Id; }

        const InputDeviceInfo& GetInfo() const { return m_Info; }

        /** The interface that owns this device. Invalid while the device is disconnected. */
        InputInterfaceId GetOwner() const { return m_Owner; }

        bool IsConnected() const { return m_Connected; }

        /** The window this device sends input for, if any (mouse and keyboard). */
        WindowHandle GetWindow() const { return m_Window; }

        void SetWindow(WindowHandle t_Window) { m_Window = t_Window; }

        /** The newest state of a key on this device, before any consumer reads it. */
        InputKeyState GetLatestKeyState(InputKey t_Key) const;

        void PushKey(InputKey t_Key, bool t_IsDown, uint32 t_Scancode = 0, int64 t_TimeNs = 0);

        void PushRepeat(InputKey t_Key, uint32 t_Scancode = 0, int64 t_TimeNs = 0);

        void PushChar(uint32 t_CodePoint, int64 t_TimeNs = 0);

        void PushAxis(InputKey t_Key, float t_Value, int64 t_TimeNs = 0);

        /**
         * @param t_DeltaX      Relative movement in integer counts
         * @param t_DeltaY      Relative movement in integer counts
         * @param t_PositionX   Cursor position in window coordinates
         * @param t_PositionY   Cursor position in window coordinates
         */
        void PushMouseMove(int32 t_DeltaX, int32 t_DeltaY, float t_PositionX, float t_PositionY, int64 t_TimeNs = 0);

        void PushWheel(float t_WheelX, float t_WheelY, int64 t_TimeNs = 0);

        void PushFocus(bool t_HasFocus, int64 t_TimeNs = 0);

    private:
        friend class InputDeviceFactory;

        InputDeviceId m_Id;

        InputDeviceInfo m_Info;

        InputInterfaceId m_Owner;

        std::weak_ptr<InputMessageHandler> m_Handler;

        WindowHandle m_Window;

        bool m_Connected = false;
    };
} // namespace Fling
