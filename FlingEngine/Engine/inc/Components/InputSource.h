#pragma once

#include "Ecs/Component.h"
#include "Input/InputMessageProcessor.h"

#include <memory>

namespace Fling
{
    class InputMessageHandler;

    /**
     * Singleton component: where the Engine.Input system gets its input. It is the state
     * of that system, and no other system uses it. Gameplay reads InputSnapshot.
     *
     * The application connects it to the input message handler with InputUtil::Connect.
     * A world that is not connected (a test, a server) has empty input on every tick.
     */
    struct InputSource
    {
        static constexpr ComponentInfo Component{
            .Name = "InputSource", .Kind = ComponentKind::Singleton, .Transient = true};

        /** Gives the newest device state after the reader falls behind the queue. */
        std::weak_ptr<const InputMessageHandler> Handler;

        /**
         * The simulation's own cursor on the input message queue. Each tick takes the
         * messages that no earlier tick took. Null if the world is not connected.
         */
        std::unique_ptr<InputMessageProcessor> Reader;

        /** UI owns the keyboard: its keys give neutral input to the simulation. */
        bool KeyboardCaptured = false;

        /** UI owns the mouse: its buttons and movement give neutral input to the simulation. */
        bool MouseCaptured = false;
    };
} // namespace Fling
