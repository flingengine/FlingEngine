#pragma once

#include "Ecs/EngineOrder.h"

namespace Fling
{
    // The Physics systems that other modules may order against with SystemOf<T>. The
    // step runs in Order::Physics. To run before or after it, prefer a group of your own
    // in Order::Physics with .Before or .After = SystemOf<PhysicsStepSystem>.
    class PhysicsStepSystem;
} // namespace Fling
