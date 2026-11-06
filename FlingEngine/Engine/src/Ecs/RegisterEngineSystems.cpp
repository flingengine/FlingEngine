#include "Ecs/RegisterEngineSystems.h"
#include "Ecs/SystemRegistry.h"
#include "Systems/InputSystem.h"

// The Engine module's register file: every Engine system is listed here, and only
// this file includes the system headers (Plans/ecs-systems.md section 3.3).

namespace Fling
{
    void RegisterEngineSystems(SystemRegistry& t_Systems)
    {
        t_Systems.Add<InputSystem>();
    }
} // namespace Fling
