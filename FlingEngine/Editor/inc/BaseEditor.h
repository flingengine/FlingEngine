#pragma once

#include <imgui.h>
#include <entt/entity/registry.hpp>
#include "imgui_entt_entity_editor.hpp"
#include "FileBrowser.h"

namespace Fling
{
    /**
     * @brief The BaseEditor of the Fling Engine. Draw and add any game specifc Editor UI tools here
     */
    class BaseEditor
    {
    public:
        BaseEditor() = default;
        virtual ~BaseEditor() = default;

		/** Register  */
		virtual void RegisterComponents(entt::registry& t_Reg);

        /**
         * @brief Draws the editor via IMGUI. Does NOT need to do any addition renderering pipeline things
         */
        virtual void Draw(entt::registry& t_Reg, float DeltaTime);

        // #TODO: Init and shutdown functions 

    protected: 

		std::array<float, 400> fpsGraph {};
		float m_FrameTimeMin = 9999.0f;
        float m_FrameTimeMax = 0.0f;

		bool m_DisplayGPUInfo = false;
		bool m_DisplayComponentEditor = true;
		bool m_DisplayWorldOutline = true;

		/** Component editor so that we can draw our component window */
		entt::entity m_CompEditorEntityType = entt::null;
		MM::ImGuiEntityEditor<entt::registry> m_ComponentEditor;

        Fling::FileBrowser m_FileBrowser;

		virtual void DrawFileMenu();

		void DrawGpuInfo();

        void DrawWorldOutline(entt::registry& t_Reg);

        // Draw stats graph
        // Draw File Menu
        // Draw Gizmos, etc
        // Draw component editor
    };
}   // namespace Fling