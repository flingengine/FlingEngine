#pragma once

#include "FlingTypes.h"
#include "Input/InputDeviceTypes.h"
#include "Input/InputKey.h"
#include "Input/InputKeyState.h"
#include "Input/InputMessage.h"
#include <unordered_map>
#include <vector>

namespace Fling
{
    /** The accumulated state of one device, as one consumer sees it. */
    struct InputDeviceState
    {
        std::unordered_map<InputKey, InputKeyState> Keys;

        /** Mouse movement summed since the edges were last cleared, in integer counts. */
        int32 MouseDeltaX = 0;
        int32 MouseDeltaY = 0;

        /** The last cursor position in window coordinates. */
        float PointerX = 0.0f;
        float PointerY = 0.0f;

        /** Wheel scroll summed since the edges were last cleared. */
        float WheelX = 0.0f;
        float WheelY = 0.0f;

        bool Connected = false;

        /** The state of a key. A key with no messages gives a default state. */
        InputKeyState GetKeyState(InputKey t_Key) const;
    };

    /**
     * Applies InputMessages to get the state of every device. Each InputMessageProcessor
     * has one, and InputMessageHandler has one for the newest state.
     */
    class InputDeviceStates
    {
    public:
        /**
         * Apply one message.
         *   KeyDown:  if not held, set held and add a press. Value = 1
         *   KeyUp:    if held, clear held and add a release. Value = 0
         *   KeyRepeat: add a repeat
         *   Axis:     set Value
         *   MouseMove: add the delta, set the pointer position
         *   MouseWheel: add the wheel values
         *   DeviceConnected / DeviceDisconnected: set Connected
         * Other types change nothing. Every message makes an entry for its device.
         */
        void Apply(const InputMessage& t_Message);

        /** Clear the edge counts of every key, and the summed mouse and wheel values. */
        void ClearEdges();

        /** The state of a device, or null if no message for it was applied. */
        const InputDeviceState* Find(InputDeviceId t_Device) const;

        /** The state of a key on a device. Gives a default state if there is none. */
        InputKeyState GetKeyState(InputDeviceId t_Device, InputKey t_Key) const;

        const std::unordered_map<InputDeviceId, InputDeviceState>& GetDevices() const { return m_Devices; }

        /**
         * Make the messages that change the held keys and the connected state of this
         * object to match t_Target: a KeyUp for each key held here and not in t_Target, a
         * KeyDown for each key held in t_Target and not here, Axis messages for axis
         * values that are different, and connect or disconnect messages. Each message has
         * t_TimeNs and Sequence 0. Used to recover after a consumer falls behind.
         */
        std::vector<InputMessage> MakeTransitionMessages(const InputDeviceStates& t_Target, int64 t_TimeNs) const;

        void Reset() { m_Devices.clear(); }

    private:
        std::unordered_map<InputDeviceId, InputDeviceState> m_Devices;
    };
} // namespace Fling
