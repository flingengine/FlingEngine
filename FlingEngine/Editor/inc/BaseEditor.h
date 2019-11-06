#pragma once

#include <imgui.h>
#include <entt/entity/registry.hpp>

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

        /**
         * @brief Draws the editor via IMGUI. Does NOT need to do any addition renderering pipeline things
         */
        virtual void Draw(const entt::registry& t_Reg, float DeltaTime);

        // #TODO: Init and shutdown functions 

    protected: 

		std::array<float, 400> fpsGraph {};
		float m_FrameTimeMin = 9999.0f;
        float m_FrameTimeMax = 0.0f;

        // Draw stats graph
        // Draw File Menu
        // Draw Gizmos, etc
        // Draw component editor
    };
}   // namespace Fling