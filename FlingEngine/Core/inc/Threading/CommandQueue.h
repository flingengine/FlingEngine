#pragma once

#include "Threading/ThreadQueue.h"

#include <functional>

namespace Fling
{
    /**
     * A lambda for another thread. Copy values into it. Do not capture a world,
     * a registry, or a pointer to a component: the other thread must not use them.
     * A std::function must be copyable, so capture a shared_ptr to a value that
     * cannot be copied.
     */
    using Command = std::function<void()>;

    /** ThreadQueue of commands. The consumer thread runs them with Execute. */
    class CommandQueue : public ThreadQueue<Command>
    {
    public:
        /**
         * Run each command that is in the queue now, in push order.
         *
         * @return The number of commands
         */
        uint32 Execute()
        {
            return Drain([](Command& t_Command) { t_Command(); });
        }
    };
} // namespace Fling
