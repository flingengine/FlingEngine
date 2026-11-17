#pragma once

#include "Ecs/EntityStorage.h"
#include "EditorWidgets.h"
#include "EntityEditorPanel.h"
#include "IDebugUI.h"
#include "InputDevicesPanel.h"
#include "Strings/String.h"
#include "WorldOutlinePanel.h"
#include "WorldSingletonsPanel.h"

#include <array>
#include <imgui.h>
#include <memory>

namespace Fling
{
    /**
     * The BaseEditor of the Fling Engine. Draw and add any game specifc Editor UI tools here
     */
    class BaseEditor : public IDebugUI
    {
    public:
        BaseEditor() = default;
        virtual ~BaseEditor() = default;

        /** Keeps the world, the game and the run controls, then calls RegisterWidgets(). */
        void Init(Fling::EntityStorage& t_Reg,
                  const std::shared_ptr<World>& t_World,
                  const std::shared_ptr<Game>& t_Game,
                  RunControls& t_Controls) override;

        /**
         * Add editor widget overrides for components that the generated inspector cannot
         * edit well. Call the base version from an override, to keep the engine's.
         */
        virtual void RegisterWidgets(EditorWidgets& t_Widgets);

        /**
         * Draws the editor via IMGUI. Does NOT need to do any addition renderering pipeline things
         */
        void Draw(Fling::EntityStorage& t_Reg, float DeltaTime) override;

        // #TODO: Init and shutdown functions

    protected:
        virtual void OnLoadLevel(String t_FileName);

        virtual void OnSaveLevel(String t_FileName);

        std::array<float, 400> fpsGraph{};
        float m_FrameTimeMin = 9999.0f;
        float m_FrameTimeMax = 0.0f;

        bool m_DisplayGpuOptions = false;

        /**
         * The last screenshot taken with the GPU Options button: what the window shows under it.
         * Shared, so a result that arrives after the editor is gone writes nowhere.
         */
        std::shared_ptr<String> m_ScreenshotStatus = std::make_shared<String>();
        bool m_DisplayComponentEditor = true;
        bool m_DisplayWorldOutline = true;
        bool m_DisplayWindowOptions = false;
        bool m_DisplayCameraOptions = false;

        /** Lists entities in the world and tracks which one is selected */
        WorldOutlinePanel m_WorldOutline;

        /** How each component type is drawn in the entity editor */
        EditorWidgets m_Widgets;

        /** Inspects the entity currently selected in m_WorldOutline */
        EntityEditorPanel m_EntityEditor;

        /** Lists input interfaces and devices, and changes the input log level */
        InputDevicesPanel m_InputDevices;

        /** Singleton components and the system schedule */
        WorldSingletonsPanel m_WorldSingletons;

        virtual void DrawFileMenu();

        /** Draws the Play and Stop buttons in the menu bar */
        void DrawPlayControls();

        /**
         * The GPU Options window: the device, VSync (with the present mode in use), the frame
         * cap, the simulation rate, render interpolation and frustum culling (with the meshes
         * drawn and culled), with what the frame limiter does
         * and the measured rates, and a button that saves the next frame as a PNG
         * (Screenshot::Request). Each control
         * calls RunControls, which checks the request; a number box applies when its edit is
         * committed (Enter or focus loss).
         */
        void DrawGpuOptions();

        void DrawCameraOptions();

        void DrawWorldOutline(Fling::EntityStorage& t_Reg);

        /** assumes that m_DisplayComponentEditor is true */
        void DrawEntityEditor(Fling::EntityStorage& t_Reg);

        void DrawWindowOptions();

        /** The world this editor works on. Asserts that it is still alive. */
        std::shared_ptr<World> OwningWorld() const;

        /** The engine owns the world and the game. The editor only looks at them. */
        std::weak_ptr<World> m_OwningWorld;

        std::weak_ptr<Game> m_Game;

        /** @see IDebugUI::Init. Set by Init. */
        RunControls* m_Controls = nullptr;

        /** A GPU Options number box: the typed value, and whether it was being edited last frame. */
        struct NumberField
        {
            uint32 Value = 0;
            bool bEditing = false;
        };

        NumberField m_FrameCapField;
        NumberField m_SimHzField;

        /** The message of the last request that RunControls refused. */
        String m_ControlsError;

        // Draw stats graph
        // Draw File Menu
        // Draw Gizmos, etc
        // Draw component editor
    };
} // namespace Fling
