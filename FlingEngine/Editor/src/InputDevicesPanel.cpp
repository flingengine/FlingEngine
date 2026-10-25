#include "InputDevicesPanel.h"

#include "Input/Input.h"

#include <imgui.h>

#include <algorithm>
#include <memory>
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
    } // namespace

    void InputDevicesPanel::Draw(entt::registry& t_Reg)
    {
        if (!m_ShowWindow)
        {
            return;
        }

        if (ImGui::Begin("Input Devices", &m_ShowWindow))
        {
            ImGui::SetWindowSize(ImVec2(560.0f, 480.0f), ImGuiCond_FirstUseEver);

            if (!Input::Get().IsInitialized())
            {
                ImGui::TextUnformatted("The input system is not started.");
            }
            else
            {
                DrawLogLevelButton();
                DrawSummary();
                ImGui::Separator();
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
                                      ", " + std::to_string(Owned) + " devices)";
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
        if (!ImGui::BeginTable("InputDevices", 5, Flags))
        {
            return;
        }
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("ID");
        ImGui::TableSetupColumn("Type");
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
            if (Device->IsConnected())
            {
                ImGui::TextColored(ImVec4(0.4f, 0.9f, 0.4f, 1.0f), "Connected");
            }
            else
            {
                ImGui::TextDisabled("Disconnected");
            }
            ImGui::TableSetColumnIndex(4);
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
