#include "InputDevicesPanel.h"

#include "FlingPaths.h"
#include "Input/Input.h"

#include <imgui.h>

#include <algorithm>
#include <ctime>
#include <iomanip>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace Fling
{
    namespace
    {
        const char* ToLabel(InputLogLevel t_Level)
        {
            switch (t_Level)
            {
            case InputLogLevel::Off:
                return "Off";
            case InputLogLevel::Events:
                return "Events";
            case InputLogLevel::All:
                return "All";
            }
            return "Off";
        }

        InputLogLevel NextLevel(InputLogLevel t_Level)
        {
            switch (t_Level)
            {
            case InputLogLevel::Off:
                return InputLogLevel::Events;
            case InputLogLevel::Events:
                return InputLogLevel::All;
            case InputLogLevel::All:
                return InputLogLevel::Off;
            }
            return InputLogLevel::Off;
        }

        /** The name of the interface that owns a device, or "-" if it is disconnected. */
        std::string OwnerName(const InputDevice& t_Device)
        {
            if (const std::shared_ptr<IInputDeviceInterface> Owner = Input::Get().FindInterface(t_Device.GetOwner()))
            {
                return Owner->GetName();
            }
            return "-";
        }

#if WITH_INPUT_RECORDING
        /** A new file path for a recording, named with the local time. */
        std::string MakeRecordingPath()
        {
            const std::time_t Now = std::time(nullptr);
            std::tm LocalTime = {};
#if defined(_WIN32)
            localtime_s(&LocalTime, &Now);
#else
            localtime_r(&Now, &LocalTime);
#endif
            std::ostringstream Name;
            Name << FlingPaths::ProjectSavedDir() << "/InputRecordings/Recording_"
                 << std::put_time(&LocalTime, "%Y-%m-%d_%H-%M-%S") << ".input.jsonl";
            return Name.str();
        }

        /** A progress bar that goes down from full to empty, for a countdown. */
        void DrawCountdownBar(const char* t_Text, float t_Remaining, float t_Total)
        {
            const float Fraction = t_Total > 0.0f ? std::clamp(t_Remaining / t_Total, 0.0f, 1.0f) : 0.0f;
            char Overlay[64];
            std::snprintf(Overlay, sizeof(Overlay), "%s %.1f s", t_Text, t_Remaining);
            ImGui::ProgressBar(Fraction, ImVec2(-1.0f, 0.0f), Overlay);
        }
#endif // WITH_INPUT_RECORDING
    } // namespace

    void InputDevicesPanel::Draw(entt::registry& t_Reg)
    {
        if (!m_ShowWindow)
        {
            return;
        }

        if (ImGui::Begin("Input Devices", &m_ShowWindow))
        {
            ImGui::SetWindowSize(ImVec2(560.0f, 720.0f), ImGuiCond_FirstUseEver);

            if (!Input::Get().IsInitialized())
            {
                ImGui::TextUnformatted("The input system is not started.");
            }
            else
            {
                DrawLogLevelButton();
                DrawSummary();
                ImGui::Separator();
#if WITH_INPUT_RECORDING
                if (ImGui::CollapsingHeader("Recording + Playback", ImGuiTreeNodeFlags_DefaultOpen))
                {
                    DrawRecording();
                    ImGui::Separator();
                    DrawPlayback();
                    ImGui::Separator();
                    DrawRecordingLog();
                }
#endif
                DrawInterfaces();
                DrawDevices();
            }
        }
        ImGui::End();
    }

    void InputDevicesPanel::DrawLogLevelButton()
    {
        const InputLogLevel Level = Input::Get().GetLogLevel();
        const std::string Label = std::string("Input log: ") + ToLabel(Level);
        if (ImGui::Button(Label.c_str()))
        {
            Input::Get().SetLogLevel(NextLevel(Level));
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Click to change the input log level.\n"
                              "Off: no input logs.\n"
                              "Events: every input message except mouse movement.\n"
                              "All: every input message.");
        }
    }

    void InputDevicesPanel::DrawSummary()
    {
        const Input& Input = Input::Get();
        const std::shared_ptr<InputMessageHandler>& Handler = Input.GetHandler();
        const InputMessageQueue& Queue = Handler->GetQueue();

        ImGui::Text("UI capture: keyboard %s, mouse %s",
                    Input.IsKeyboardCaptured() ? "yes" : "no",
                    Input.IsMouseCaptured() ? "yes" : "no");

        const uint64 Newest = Queue.NewestSequence();
        const uint64 Held = Newest == 0 ? 0 : Newest - Queue.OldestSequence() + 1;
        ImGui::Text("Messages: newest #%llu, queue holds %llu of %u",
                    static_cast<unsigned long long>(Newest),
                    static_cast<unsigned long long>(Held),
                    Queue.Capacity());

        const WindowHandle Window = Handler->GetCurrentWindow();
        ImGui::Text("Window: %s", Window.IsValid() ? "set" : "none (headless)");
    }

#if WITH_INPUT_RECORDING
    void InputDevicesPanel::DrawRecording()
    {
        InputRecorder& Recorder = Input::Get().GetRecorder();
        const InputRecorderState State = Recorder.GetState();

        if (State == InputRecorderState::Recording && !m_WasRecording)
        {
            m_SelectRecordingTab = true;
        }
        m_WasRecording = State == InputRecorderState::Recording;

        ImGui::TextUnformatted("Recording");

        if (State == InputRecorderState::Idle)
        {
            ImGui::SetNextItemWidth(120.0f);
            ImGui::InputFloat("Start delay (s)##Record", &m_RecordDelay, 0.5f, 1.0f, "%.1f");
            m_RecordDelay = std::max(m_RecordDelay, 0.0f);
            ImGui::SetNextItemWidth(120.0f);
            ImGui::InputFloat("Record for (s)##Record", &m_RecordDuration, 1.0f, 5.0f, "%.1f");
            m_RecordDuration = std::max(m_RecordDuration, 0.0f);
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("0 records until you press Stop.");
            }
            ImGui::SameLine();
            ImGui::TextDisabled("(0 = until stopped)");

            if (ImGui::Button("Start recording"))
            {
                Recorder.Start(MakeRecordingPath(), m_RecordDelay, m_RecordDuration);
            }
        }
        else if (State == InputRecorderState::Countdown)
        {
            DrawCountdownBar("Recording starts in", Recorder.GetCountdownRemaining(), Recorder.GetCountdownTotal());
            if (ImGui::Button("Cancel##Record"))
            {
                Recorder.Stop();
            }
        }
        else
        {
            const float Elapsed = Recorder.GetElapsed();
            const float Duration = Recorder.GetDuration();
            if (Duration > 0.0f)
            {
                char Overlay[64];
                std::snprintf(Overlay, sizeof(Overlay), "Recording %.1f / %.1f s", Elapsed, Duration);
                ImGui::ProgressBar(std::clamp(1.0f - Elapsed / Duration, 0.0f, 1.0f), ImVec2(-1.0f, 0.0f), Overlay);
            }
            else
            {
                ImGui::Text("Recording %.1f s (until stopped)", Elapsed);
            }
            ImGui::Text("Messages: %zu", Recorder.GetMessageCount());
            if (ImGui::Button("Stop recording"))
            {
                // The click on this button is not part of the recording
                Recorder.Stop(true);
            }
        }

        if (!Recorder.GetLastSavedPath().empty())
        {
            ImGui::TextWrapped("Last recording: %s", Recorder.GetLastSavedPath().c_str());
        }
        if (!Recorder.GetLastError().empty())
        {
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", Recorder.GetLastError().c_str());
        }
    }

    void InputDevicesPanel::DrawPlayback()
    {
        InputRecorder& Recorder = Input::Get().GetRecorder();
        InputPlayback& Playback = Input::Get().GetPlayback();
        const InputPlaybackState State = Playback.GetState();

        // A new recording fills the path only when no file is selected
        if (Recorder.GetSaveCount() != m_SeenSaveCount)
        {
            if (m_PlaybackPath.empty())
            {
                m_PlaybackPath = Recorder.GetLastSavedPath();
            }
            m_SeenSaveCount = Recorder.GetSaveCount();
        }

        if (State == InputPlaybackState::Playing && !m_WasPlaying)
        {
            m_SelectPlaybackTab = true;
        }
        m_WasPlaying = State == InputPlaybackState::Playing;

        ImGui::TextUnformatted("Playback");

        if (State == InputPlaybackState::Idle)
        {
            char Buffer[1024] = {};
            m_PlaybackPath.copy(Buffer, sizeof(Buffer) - 1);
            ImGui::SetNextItemWidth(-90.0f);
            if (ImGui::InputText("##PlaybackPath", Buffer, sizeof(Buffer)))
            {
                m_PlaybackPath = Buffer;
            }
            ImGui::SameLine();
            if (ImGui::Button("Browse..."))
            {
                const std::string Start =
                    m_PlaybackPath.empty() ? FlingPaths::ProjectSavedDir() + "/InputRecordings" : m_PlaybackPath;
                m_FileBrowser.Open(FlingPaths::ResolveProjectPath(Start), ".jsonl");
            }

            ImGui::SetNextItemWidth(120.0f);
            ImGui::InputFloat("Start delay (s)##Play", &m_PlaybackDelay, 0.5f, 1.0f, "%.1f");
            m_PlaybackDelay = std::max(m_PlaybackDelay, 0.0f);

            ImGui::BeginDisabled(m_PlaybackPath.empty());
            if (ImGui::Button("Play"))
            {
                m_PlaybackError.clear();
                Playback.Start(FlingPaths::ResolveProjectPath(m_PlaybackPath), m_PlaybackDelay, m_PlaybackError);
            }
            ImGui::EndDisabled();
            if (!m_PlaybackError.empty())
            {
                ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", m_PlaybackError.c_str());
            }
        }
        else if (State == InputPlaybackState::Countdown)
        {
            DrawCountdownBar("Playback starts in", Playback.GetCountdownRemaining(), Playback.GetCountdownTotal());
            if (ImGui::Button("Cancel##Play"))
            {
                Playback.Stop();
            }
        }
        else
        {
            const double Length = Playback.GetLength();
            char Overlay[96];
            std::snprintf(Overlay,
                          sizeof(Overlay),
                          "%.1f / %.1f s  (%zu / %zu messages)",
                          Playback.GetTime(),
                          Length,
                          Playback.GetSentCount(),
                          Playback.GetMessageCount());
            const float Fraction =
                Length > 0.0 ? static_cast<float>(std::clamp(Playback.GetTime() / Length, 0.0, 1.0)) : 0.0f;
            ImGui::ProgressBar(Fraction, ImVec2(-1.0f, 0.0f), Overlay);
            if (ImGui::Button("Stop##Play"))
            {
                Playback.Stop();
            }
        }

        std::string Picked;
        if (m_FileBrowser.Draw("Select an input recording", Picked))
        {
            m_PlaybackPath = Picked;
            m_PlaybackError.clear();
        }
    }

    void InputDevicesPanel::DrawRecordingLog()
    {
        ImGui::Checkbox("Hide mouse movement", &m_HideMouseMove);

        if (!ImGui::BeginTabBar("InputLogs"))
        {
            return;
        }

        ImGuiTabItemFlags RecordingFlags = ImGuiTabItemFlags_None;
        if (m_SelectRecordingTab)
        {
            RecordingFlags |= ImGuiTabItemFlags_SetSelected;
            m_SelectRecordingTab = false;
        }
        if (ImGui::BeginTabItem("Recording log", nullptr, RecordingFlags))
        {
            DrawLogView("RecordingLog", Input::Get().GetRecorder().GetLog(), m_SeenRecordingTotal);
            ImGui::EndTabItem();
        }

        ImGuiTabItemFlags PlaybackFlags = ImGuiTabItemFlags_None;
        if (m_SelectPlaybackTab)
        {
            PlaybackFlags |= ImGuiTabItemFlags_SetSelected;
            m_SelectPlaybackTab = false;
        }
        if (ImGui::BeginTabItem("Playback log", nullptr, PlaybackFlags))
        {
            DrawLogView("PlaybackLog", Input::Get().GetPlayback().GetLog(), m_SeenPlaybackTotal);
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    void InputDevicesPanel::DrawLogView(const char* t_Id, InputEventLog& t_Log, uint64& t_SeenTotal)
    {
        if (ImGui::Button("Clear"))
        {
            t_Log.Clear();
        }

        if (ImGui::BeginChild(t_Id, ImVec2(0.0f, 180.0f), ImGuiChildFlags_Borders))
        {
            const std::deque<std::string>& Lines = t_Log.GetLines();
            const bool WasAtBottom = ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 1.0f;

            if (m_HideMouseMove)
            {
                for (const std::string& Line : Lines)
                {
                    if (Line.find("MouseMove") == std::string::npos)
                    {
                        ImGui::TextUnformatted(Line.c_str());
                    }
                }
            }
            else
            {
                ImGuiListClipper Clipper;
                Clipper.Begin(static_cast<int>(Lines.size()));
                while (Clipper.Step())
                {
                    for (int Index = Clipper.DisplayStart; Index < Clipper.DisplayEnd; ++Index)
                    {
                        ImGui::TextUnformatted(Lines[static_cast<size_t>(Index)].c_str());
                    }
                }
            }

            if (t_Log.GetTotalAdded() != t_SeenTotal && WasAtBottom)
            {
                ImGui::SetScrollHereY(1.0f);
            }
            t_SeenTotal = t_Log.GetTotalAdded();
        }
        ImGui::EndChild();
    }
#endif // WITH_INPUT_RECORDING

    void InputDevicesPanel::DrawInterfaces()
    {
        const std::shared_ptr<InputMessageHandler>& Handler = Input::Get().GetHandler();
        const std::vector<std::shared_ptr<IInputDeviceInterface>> Interfaces = Handler->GetInterfaces();
        const std::vector<std::shared_ptr<InputDevice>> Devices = Handler->GetDeviceFactory().GetDevices();

        const std::string Header = "Interfaces (" + std::to_string(Interfaces.size()) + ")";
        if (!ImGui::CollapsingHeader(Header.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
        {
            return;
        }

        if (Interfaces.empty())
        {
            ImGui::TextUnformatted("No interfaces. Under --null-render this is normal.");
            return;
        }

        for (const std::shared_ptr<IInputDeviceInterface>& Interface : Interfaces)
        {
            const InputInterfaceId Id = Interface->GetInterfaceId();
            ImGui::PushID(static_cast<int>(Id.Value));

            int Owned = 0;
            for (const std::shared_ptr<InputDevice>& Device : Devices)
            {
                if (Device->IsConnected() && Device->GetOwner() == Id)
                {
                    ++Owned;
                }
            }

            const std::string Label = std::string(Interface->GetName()) + "  (interface " + std::to_string(Id.Value) +
                                      ", " + std::to_string(Owned) + " devices" +
                                      (Interface->IsSimulated() ? ", simulated" : "") + ")";
            if (ImGui::TreeNodeEx(Label.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
            {
                if (Owned == 0)
                {
                    ImGui::TextDisabled("No connected devices");
                }
                for (const std::shared_ptr<InputDevice>& Device : Devices)
                {
                    if (!Device->IsConnected() || Device->GetOwner() != Id)
                    {
                        continue;
                    }
                    ImGui::BulletText("%s  (device %u, %s)",
                                      Device->GetInfo().DisplayName.c_str(),
                                      Device->GetId().Value,
                                      ToString(Device->GetInfo().Type));
                }
                ImGui::TreePop();
            }
            ImGui::PopID();
        }
    }

    void InputDevicesPanel::DrawDevices()
    {
        const std::vector<std::shared_ptr<InputDevice>> Devices =
            Input::Get().GetHandler()->GetDeviceFactory().GetDevices();

        const std::string Header = "Devices (" + std::to_string(Devices.size()) + ")";
        if (!ImGui::CollapsingHeader(Header.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
        {
            return;
        }

        ImGui::Checkbox("Show disconnected devices", &m_ShowDisconnected);

        const ImGuiTableFlags Flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable |
                                      ImGuiTableFlags_SizingFixedFit;
        if (!ImGui::BeginTable("InputDevices", 6, Flags))
        {
            return;
        }
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("ID");
        ImGui::TableSetupColumn("Type");
        ImGui::TableSetupColumn("Source");
        ImGui::TableSetupColumn("Status");
        ImGui::TableSetupColumn("Interface");
        ImGui::TableHeadersRow();

        for (const std::shared_ptr<InputDevice>& Device : Devices)
        {
            if (!m_ShowDisconnected && !Device->IsConnected())
            {
                continue;
            }
            ImGui::PushID(static_cast<int>(Device->GetId().Value));
            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            const std::string& Name = Device->GetInfo().DisplayName;
            const bool Open =
                ImGui::TreeNodeEx(Name.empty() ? "(no name)" : Name.c_str(), ImGuiTreeNodeFlags_SpanFullWidth);

            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%u", Device->GetId().Value);
            ImGui::TableSetColumnIndex(2);
            ImGui::TextUnformatted(ToString(Device->GetInfo().Type));
            ImGui::TableSetColumnIndex(3);
            if (Device->IsSimulated())
            {
                ImGui::TextColored(ImVec4(0.95f, 0.75f, 0.3f, 1.0f), "Simulated");
                if (ImGui::IsItemHovered())
                {
                    ImGui::SetTooltip("Code makes this device's input: playback, a test, or a script.");
                }
            }
            else
            {
                ImGui::TextUnformatted("Hardware");
            }
            ImGui::TableSetColumnIndex(4);
            if (Device->IsConnected())
            {
                ImGui::TextColored(ImVec4(0.4f, 0.9f, 0.4f, 1.0f), "Connected");
            }
            else
            {
                ImGui::TextDisabled("Disconnected");
            }
            ImGui::TableSetColumnIndex(5);
            ImGui::TextUnformatted(OwnerName(*Device).c_str());

            if (Open)
            {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                DrawDeviceDetails(*Device);
                ImGui::TreePop();
            }
            ImGui::PopID();
        }
        ImGui::EndTable();
    }

    void InputDevicesPanel::DrawDeviceDetails(const InputDevice& t_Device)
    {
        const InputDeviceInfo& Info = t_Device.GetInfo();
        ImGui::Text("Vendor:Product  %04X:%04X", Info.VendorId, Info.ProductId);
        ImGui::Text("Hardware path   %s", Info.HardwarePath.empty() ? "-" : Info.HardwarePath.c_str());
        ImGui::TextWrapped("Capabilities    %s", ToString(Info.Capabilities).c_str());
        ImGui::Text("Window          %s", t_Device.GetWindow().IsValid() ? "set" : "none");

        // The newest state, with every message applied, before any consumer reads it
        const InputDeviceState* State = Input::Get().GetHandler()->GetLatestState().Find(t_Device.GetId());
        if (State == nullptr)
        {
            ImGui::TextDisabled("No input yet");
            return;
        }

        if (Info.HasCapability(InputDeviceCapabilities::Pointer))
        {
            ImGui::Text("Pointer         (%.1f, %.1f)", State->PointerX, State->PointerY);
        }

        std::string HeldKeys;
        std::vector<std::pair<InputKey, float>> Axes;
        for (const auto& Entry : State->Keys)
        {
            if (Entry.first.IsAxis())
            {
                Axes.emplace_back(Entry.first, Entry.second.Value);
            }
            else if (Entry.second.IsHeld)
            {
                HeldKeys += HeldKeys.empty() ? "" : ", ";
                HeldKeys += Entry.first.GetName();
            }
        }
        ImGui::TextWrapped("Held            %s", HeldKeys.empty() ? "-" : HeldKeys.c_str());

        std::sort(Axes.begin(), Axes.end(), [](const auto& t_A, const auto& t_B) { return t_A.first < t_B.first; });
        for (const auto& Axis : Axes)
        {
            // A bar from -1 to 1, so stick drift and trigger travel are easy to see
            const float Value = Axis.second;
            ImGui::ProgressBar((Value + 1.0f) * 0.5f, ImVec2(160.0f, 0.0f), "");
            ImGui::SameLine();
            ImGui::Text("%s  %.3f", Axis.first.GetName(), Value);
        }
    }
} // namespace Fling
