#pragma once

#include "FlingEditorPanel.h"

#include <entt/entity/registry.hpp>

namespace Fling
{
    class InputDevice;

    /**
     * Debug window for the input system. It lists every input interface and the devices
     * it owns, every device with its metadata and live state, and has a button that
     * changes the input log level.
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

        void DrawInterfaces();

        void DrawDevices();

        void DrawDeviceDetails(const InputDevice& t_Device);

        bool m_ShowDisconnected = true;
    };
} // namespace Fling
