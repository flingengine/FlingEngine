#pragma once

#include <memory>

namespace Fling
{
    class EntityStorage;
    class InputMessageHandler;

    /** Functions that connect a world to the input message handler. */
    namespace InputUtil
    {
        /**
         * Connect the world's simulation input to a handler. From the next tick, the
         * Engine.Input system builds InputSnapshot from the messages that arrive after
         * this call. Call it again to connect to a different handler. A null handler
         * disconnects the world.
         */
        void Connect(EntityStorage& t_Storage, const std::shared_ptr<const InputMessageHandler>& t_Handler);

        /**
         * Tell the simulation that UI (for example ImGui) owns the keyboard or the mouse.
         * Does nothing if the world is not connected.
         */
        void SetUICapture(EntityStorage& t_Storage, bool t_Keyboard, bool t_Mouse);
    } // namespace InputUtil
} // namespace Fling
