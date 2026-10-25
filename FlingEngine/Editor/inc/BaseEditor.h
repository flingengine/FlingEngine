#pragma once

#include <imgui.h>
#include <entt/entity/registry.hpp>
#include "IDebugUI.h"
#include "WorldOutlinePanel.h"
#include "EntityEditorPanel.h"
#include "InputDevicesPanel.h"
#include <array>
#include <string>

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

        /** Keeps the world and game, then calls RegisterComponents(). */
        void Init(entt::registry& t_Reg, World* t_World, Game* t_Game) override;

		/** Register  */
		virtual void RegisterComponents(entt::registry& t_Reg);

        /**
         * Draws the editor via IMGUI. Does NOT need to do any addition renderering pipeline things
         */
        void Draw(entt::registry& t_Reg, float DeltaTime) override;

        // #TODO: Init and shutdown functions

    protected:

        virtual void OnLoadLevel(std::string t_FileName);

        virtual void OnSaveLevel(std::string t_FileName);

		std::array<float, 400> fpsGraph {};
		float m_FrameTimeMin = 9999.0f;
        float m_FrameTimeMax = 0.0f;

		bool m_DisplayGPUInfo = false;
		bool m_DisplayComponentEditor = true;
		bool m_DisplayWorldOutline = true;
		bool m_DisplayWindowOptions = false;
        bool m_DisplayCameraOptions = false;

		/** Lists entities in the world and tracks which one is selected */
		WorldOutlinePanel m_WorldOutline;

		/** Inspects the entity currently selected in m_WorldOutline */
		EntityEditorPanel m_EntityEditor;

		/** Lists input interfaces and devices, and changes the input log level */
		InputDevicesPanel m_InputDevices;

		virtual void DrawFileMenu();

		void DrawGpuInfo();

        void DrawCameraOptions();

        void DrawWorldOutline(entt::registry& t_Reg);

        /** assumes that m_DisplayComponentEditor is true */
		void DrawEntityEditor(entt::registry& t_Reg);

		void DrawWindowOptions();

        class World* m_OwningWorld = nullptr;

        class Game* m_Game = nullptr;


        // Draw stats graph
        // Draw File Menu
        // Draw Gizmos, etc
        // Draw component editor
    };
}   // namespace Fling
