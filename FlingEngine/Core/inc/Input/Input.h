#pragma once

#include "FlingTypes.h"
#include "Input/IInputDeviceInterface.h"
#include "Input/InputDevice.h"
#include "Input/InputDeviceTypes.h"
#include "Input/InputKey.h"
#include "Input/InputMessageHandler.h"
#include "Input/InputMessageProcessor.h"
#include "Input/InputPlayback.h"
#include "Input/InputRecorder.h"
#include "Misc/SimulationCheck.h"
#include "Singleton.hpp"
#include "Strings/String.h"

#include <memory>
#include <utility>
#include <vector>

namespace Fling
{
    /**
     * The engine's input: it owns the InputMessageHandler (the message queue and the
     * device interfaces), the playback and the recorder. It has no key queries. A fixed
     * system reads the InputState singleton (the tick's cut), a frame system reads
     * PendingInput, and any other consumer (the editor, the debug camera, the UI) makes
     * its own reader with CreateProcessor.
     *
     * Each frame the engine calls, in order: Poll, the fixed ticks, PollLate, the frame
     * systems.
     *
     * Threads (see SetWriterLane): the writer lane polls and owns the handler, the
     * interfaces, the playback and the recorder (Poll, PollLate, SetUICapture, Add and
     * RemoveInterface, GetPlayback and GetRecorder, except the calls below). The tick
     * lane builds tick input with its own TickInputProvider; from there it calls only
     * InputPlayback::ClaimTickCut and NoteTickOverran and InputRecorder::NoteTickCut,
     * which are thread safe.
     *
     * The capture, the devices, the processors and the handler call
     * FLING_SIMULATION_FORBID, so a fixed system that reads them fails in a debug build
     * (SimulationCheck).
     */
    class Input : public Singleton<Input>
    {
    public:
        /** Make the handler, the playback and the recorder. Adds no interfaces. */
        void Init() override;

        /** Remove every interface and destroy the handler. Stop every other thread that reads input first. */
        void Shutdown() override;

        /**
         * The lane of the thread that polls (the writer of the queue). The engine sets
         * Lane::Frame: the main thread is the input writer in every exec mode. Debug builds
         * then check every writer call. None (the default, one
         * thread) checks nothing. Set it before other threads start.
         */
        void SetWriterLane(Lane t_Lane);

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

        std::shared_ptr<IInputDeviceInterface> FindInterface(StringView t_Name) const;

        void NotifyWindowCreated(WindowHandle t_Window);

        void NotifyWindowDestroyed(WindowHandle t_Window);

        /**
         * Advance input playback, poll every interface (this pumps platform events), fire
         * deferred device events, then let the recorder read the new messages. Shipping
         * builds have no playback or recorder.
         *
         * @param t_DeltaSeconds    The frame time. It moves the playback and recording clocks
         */
        void Poll(float t_DeltaSeconds = 0.0f);

        /**
         * The second poll of a frame: after the fixed ticks, right before the frame rate
         * systems. It polls every interface again and fires deferred device events, so the
         * input that came in while the ticks ran is in the queue, pending, when the frame
         * systems read it (PendingInputReader). The next tick consumes it as usual. Playback
         * queues the messages that its next Update would queue (InputPlayback::UpdateLate).
         * The recorder reads them at the next Poll, with that frame's time, so a recording
         * gives them the tick that consumed them. Does not move any clock.
         */
        void PollLate();

        /**
         * Tell the game that UI (for example ImGui) owns the keyboard or the mouse. While a
         * device is captured, the tick and the frame side give neutral input for its keys:
         * not held, no presses, no releases, and no mouse moves (InputSlice::Append).
         * Gamepads are never captured. A processor (the UI's own) still gets every message.
         *
         * A change is a CaptureChanged message in the stream, so the tick and the frame
         * side see it at the same place, and a recording replays it. While input playback
         * runs, the recording owns the capture and this call does nothing. Writer lane.
         */
        void SetUICapture(bool t_Keyboard, bool t_Mouse);

        /** The capture now, at the newest message. Any thread, but not simulation code. */
        bool IsKeyboardCaptured() const;

        bool IsMouseCaptured() const;

        /**
         * Log each input message at this level (see --log-input). Keeps the level for a later
         * Init. Device connects and disconnects are always logged.
         */
        void SetLogLevel(InputLogLevel t_Level);

        InputLogLevel GetLogLevel() const { return m_LogLevel; }

        std::shared_ptr<InputDevice> GetDevice(InputDeviceId t_Id) const;

        /** Every connected device. */
        std::vector<std::shared_ptr<InputDevice>> GetDevices() const;

        Sink<void(const InputDevice&)> OnDeviceConnected() { return m_Handler->OnDeviceConnected(); }

        Sink<void(const InputDevice&)> OnDeviceDisconnected() { return m_Handler->OnDeviceDisconnected(); }

        /** Make a new consumer of the message queue. After Shutdown it reads nothing (IsValid is false). */
        InputMessageProcessor CreateProcessor() const;

#if WITH_INPUT_RECORDING
        /** Records the message stream to a file. Only call this after Init. Not in shipping builds. */
        InputRecorder& GetRecorder() { return *m_Recorder; }

        /** Plays an input file through the "Playback" interface. Only call this after Init. Not in shipping builds. */
        InputPlayback& GetPlayback() { return *m_Playback; }
#endif

        /** The handler, or null before Init. Keep a std::weak_ptr if you store it. */
        const std::shared_ptr<InputMessageHandler>& GetHandler() const
        {
            ForbidInSimulation();
            return m_Handler;
        }

    private:
        std::shared_ptr<InputMessageHandler> m_Handler;

#if WITH_INPUT_RECORDING
        std::unique_ptr<InputRecorder> m_Recorder;

        std::unique_ptr<InputPlayback> m_Playback;
#endif

        /** The simulation check of every reader of the frame's input. */
        static void ForbidInSimulation()
        {
            FLING_SIMULATION_FORBID(
                "A fixed rate system read the frame's input (Input). Read the InputState singleton.");
        }

        InputCapture GetCapture() const { return m_Handler ? m_Handler->GetCapture() : InputCapture::None; }

        Lane m_WriterLane = Lane::None;

        InputLogLevel m_LogLevel = InputLogLevel::Off;
    };
} // namespace Fling
