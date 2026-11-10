#pragma once

#include "Ecs/EntityStorage.h"

namespace Sandbox
{
    /**
     * Owns the drawing of any UI elements for the Sandbox game.
     */
    class SandboxUI
    {
    public:
        SandboxUI() = default;
        ~SandboxUI() = default;

        /**
         * Draw the sandbox game ImGui UI elements
         */
        void NewFrame(Fling::EntityStorage& t_Reg);
    };
} // namespace Sandbox