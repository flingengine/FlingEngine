#include "Ecs/SystemRegistry.h"
#include "Physics.h"
#include "Systems/PhysicsStepSystem.h"

// The Physics module's register file: every Physics system is listed here, and only
// this file includes the system headers (Plans/ecs-systems.md section 3.3).

namespace Fling
{
    void RegisterPhysicsSystems(SystemRegistry& t_Systems)
    {
        t_Systems.Add<PhysicsStepSystem>();
    }
} // namespace Fling
