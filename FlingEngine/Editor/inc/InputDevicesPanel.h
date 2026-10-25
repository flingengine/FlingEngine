#pragma once

#include "FileBrowserPopup.h"
#include "FlingEditorPanel.h"
#include "Input/InputRecording.h"

#include <entt/entity/registry.hpp>

namespace Fling
{
    class InputDevice;
    enum class InputRecorderState : uint8;
    enum class InputPlaybackState : uint8;

    /**
     * Debug window for the input system. It lists every input interface and the devices
     * it owns, every device with its metadata and live state, and has a button that
     * changes the input log level. A "Recording + Playback" section starts and stops
     * input recording and playback, picks the playback file, and shows a log of the
     * messages that are recorded or replayed.
     */
    class InputDevicesPanel final : public FlingEditorPanel
    {
    public:
        InputDevicesPanel() { m_ShowWindow = false; }

        /** Draws the "Input Devices" window. Call this once per frame. */
        void Draw(entt::registry& t_Reg) override;

        /** The flag that shows or hides the window, for a menu checkbox. */
        bool& ShowWindowFlag() { return m_ShowWindow; }

    private:
        void DrawLogLevelButton();

        void DrawSummary();

        void DrawRecording();

        void DrawPlayback();

        void DrawRecordingLog();

        /** Draw one log in a scrolling child region, with a Clear button. */
        void DrawLogView(const char* t_Id, InputEventLog& t_Log, uint64& t_SeenTotal);

        void DrawInterfaces();

        void DrawDevices();

        void DrawDeviceDetails(const InputDevice& t_Device);

        bool m_ShowDisconnected = true;

        float m_RecordDelay = 3.0f;

        float m_RecordDuration = 0.0f;

        float m_PlaybackDelay = 0.0f;

        /** The playback file path, as typed or picked. */
        std::string m_PlaybackPath;

        /** The reason the last playback start failed. */
        std::string m_PlaybackError;

        /** The recorder save count that the panel has seen. */
        uint32 m_SeenSaveCount = 0;

        bool m_HideMouseMove = true;

        /** Select a log tab for one frame, when recording or playback starts. */
        bool m_SelectRecordingTab = false;

        bool m_SelectPlaybackTab = false;

        bool m_WasRecording = false;

        bool m_WasPlaying = false;

        /** The log line totals from the last frame, to find new lines. */
        uint64 m_SeenRecordingTotal = 0;

        uint64 m_SeenPlaybackTotal = 0;

        FileBrowserPopup m_FileBrowser;
    };
} // namespace Fling
