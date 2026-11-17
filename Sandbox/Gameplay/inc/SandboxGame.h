#pragma once

#include "FlingMath.h"
#include "Game.h"
#include "Strings/String.h"

namespace Sandbox
{
    /**
     * Custom game class that will have control of it's gameplay systems.
     */
    class Game : public Fling::Game
    {
        void Init(Fling::EntityStorage& t_Reg) override final;

        /** Adds the Sandbox component types and systems. Defined in RegisterSandbox.cpp. */
        void Register(Fling::ModuleRegistration& t_Modules) override final;

        /**
         * Called before the first gameplay loop tick.
         * 			Do any initalization for custom gameplay systems here.
         */
        void OnStartGame(Fling::EntityStorage& t_Reg) override final;

        /** Called when play stops. Resets the state that is not in the registry. */
        void OnStopGame(Fling::EntityStorage& t_Reg) override final;

        void Shutdown(Fling::EntityStorage& t_Reg) override final;
    };
} // namespace Sandbox