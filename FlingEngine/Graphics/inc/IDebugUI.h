#pragma once

#include "Ecs/EntityStorage.h"

#include <memory>

namespace Fling
{
    class World;
    class Game;

    /**
     * Developer UI that draws with ImGui inside the ImGui subpass, for example the
     * editor. Graphics calls Draw() and does not know what is drawn, so Graphics
     * does not depend on the Editor module.
     */
    class IDebugUI
    {
    public:
        virtual ~IDebugUI() = default;

        /**
         * Called once by the application after the world and game are created, and
         * before the world starts. Component types can still be registered here. Keep
         * the world and the game as weak_ptr: the engine owns them.
         */
        virtual void Init(Fling::EntityStorage& t_Reg,
                          const std::shared_ptr<World>& t_World,
                          const std::shared_ptr<Game>& t_Game) = 0;

        /** Draw the UI with ImGui. Called once per frame by the ImGui subpass. */
        virtual void Draw(Fling::EntityStorage& t_Reg, float t_DeltaTime) = 0;
    };
} // namespace Fling
