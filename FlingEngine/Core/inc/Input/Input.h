#pragma once

#include "FlingTypes.h"
#include "Singleton.hpp"
#include "Input/IInputDeviceInterface.h"
#include "Input/InputDevice.h"
#include "Input/InputDeviceTypes.h"
#include "Input/InputKey.h"
#include "Input/InputKeyState.h"
#include "Input/InputMessageHandler.h"
#include "Input/InputMessageProcessor.h"
#include "Input/InputPlayback.h"
#include "Input/InputRecorder.h"
#include <limits>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

namespace Fling
{
    /**
     * The input system for game code. It owns the InputMessageHandler and the game's
     * InputMessageProcessor. The key queries (IsHeld, WasPressed, ...) read the game
     * processor's state, which changes only in BeginTick, so every system in a tick sees
     * the same input.
     *
     * Each frame the engine calls, in order: Poll, BeginTick, the game update, EndTick.
     * Other consumers (render, editor, recorder) make their own processor with
     * CreateProcessor.
     */
    class Input : public Singleton<Input>
    {
    public:
        /** Make the handler and the game processor. Adds no interfaces. */
        void Init() override;

        /** Remove every interface and destroy the handler. */
        void Shutdown() override;

        bool IsInitialized() const { return m_Handler != nullptr; }

        template <typename T, typename... Args>
        InputInterfaceId AddInterface(Args&&... t_Args)
        {
            return AddInterface(std::make_shared<T>(std::forward<Args>(t_Args)...));
        }

        InputInterfaceId AddInterface(std::shared_ptr<IInputDeviceInterface> t_Interface);

        bool RemoveInterface(InputInterfaceId t_Id);

        std::shared_ptr<IInputDeviceInterface> FindInterface(InputInterfaceId t_Id) const;

        /** The interface with this ID as a T, or null if there is none or it is not a T. */
        template <typename T>
        std::shared_ptr<T> FindInterface(InputInterfaceId t_Id) const
        {
            return std::dynamic_pointer_cast<T>(FindInterface(t_Id));
        }

        std::shared_ptr<IInputDeviceInterface> FindInterface(std::string_view t_Name) const;

        void NotifyWindowCreated(WindowHandle t_Window);

        void NotifyWindowDestroyed(WindowHandle t_Window);

        /**
         * Advance input playback, poll every interface (this pumps platform events), fire
         * deferred device events, then let the recorder read the new messages.
         *
         * @param t_DeltaSeconds    The frame time. It moves the playback and recording clocks
         */
        void Poll(float t_DeltaSeconds = 0.0f);

        /**
         * Consume input for the game, up to t_UntilNs. If the game processor fell behind
         * the queue, it is resynced from the newest state first.
         */
        void BeginTick(int64 t_UntilNs = std::numeric_limits<int64>::max());

        /** Clear the game processor's edge counts and mouse deltas. */
        void EndTick();

        /**
         * Tell the game that UI (for example ImGui) owns the keyboard or the mouse. While a
         * device is captured, the key queries below give neutral input for its keys: not
         * held, no presses, no releases, and no mouse delta. Gamepads are never captured.
         * Other processors (the UI's own) still get every message.
         */
        void SetUICapture(bool t_Keyboard, bool t_Mouse);

        bool IsKeyboardCaptured() const { return m_KeyboardCaptured; }

        bool IsMouseCaptured() const { return m_MouseCaptured; }

        /**
         * Log each input message at this level (see --log-input). Keeps the level for a later
         * Init. Device connects and disconnects are always logged.
         */
        void SetLogLevel(InputLogLevel t_Level);

        InputLogLevel GetLogLevel() const { return m_LogLevel; }

        /** True if the key is held on any device. */
        bool IsHeld(InputKey t_Key) const;

        /** True if the key was pressed on any device this tick. */
        bool WasPressed(InputKey t_Key) const;

        /** True if the key was released on any device this tick. */
        bool WasReleased(InputKey t_Key) const;

        /** The state of a key on one device, this tick. */
        InputKeyState GetKeyState(InputDeviceId t_Device, InputKey t_Key) const;

        /** The largest absolute axis value of this key over every device, this tick (with its sign). */
        float GetAxisValue(InputKey t_Key) const;

        /** Mouse movement this tick, summed over every device, in integer counts. */
        void GetMouseDelta(int32& t_OutX, int32& t_OutY) const;

        /**
         * The cursor position of the first connected device with the Pointer capability,
         * in window coordinates. Returns false if there is no such device.
         */
        bool GetPointerPosition(float& t_OutX, float& t_OutY) const;

        std::shared_ptr<InputDevice> GetDevice(InputDeviceId t_Id) const;

        /** Every connected device. */
        std::vector<std::shared_ptr<InputDevice>> GetDevices() const;

        entt::sink<void(const InputDevice&)> OnDeviceConnected() { return m_Handler->OnDeviceConnected(); }

        entt::sink<void(const InputDevice&)> OnDeviceDisconnected() { return m_Handler->OnDeviceDisconnected(); }

        /** Make a new consumer of the message queue. After Shutdown it reads nothing (IsValid is false). */
        InputMessageProcessor CreateProcessor() const;

        const InputMessageProcessor& GetGameProcessor() const { return *m_GameProcessor; }

        /** Records the message stream to a file. Only call this after Init. */
        InputRecorder& GetRecorder() { return *m_Recorder; }

        /** Plays an input file through the "Playback" interface. Only call this after Init. */
        InputPlayback& GetPlayback() { return *m_Playback; }

        /** The handler, or null before Init. Keep a std::weak_ptr if you store it. */
        const std::shared_ptr<InputMessageHandler>& GetHandler() const { return m_Handler; }

    private:
        std::shared_ptr<InputMessageHandler> m_Handler;

        std::optional<InputMessageProcessor> m_GameProcessor;

        std::unique_ptr<InputRecorder> m_Recorder;

        std::unique_ptr<InputPlayback> m_Playback;

        /** True if the key belongs to a device that UI owns now. */
        bool IsCaptured(InputKey t_Key) const;

        bool m_KeyboardCaptured = false;

        bool m_MouseCaptured = false;

        InputLogLevel m_LogLevel = InputLogLevel::Off;
    };
} // namespace Fling
