#pragma once

#include <entt/entity/registry.hpp>

namespace Sandbox
{
    /**
     * @brief Owns the drawing of any UI elements for the Sandbox game.
     */
    class SandboxUI
    {
    public:
        SandboxUI() = default;
        ~SandboxUI() = default;

        /**
         * @brief Draw the sandbox game ImGui UI elements
         */
        void NewFrame(entt::registry& t_Reg); 
    };
}   // namespace Sandbox