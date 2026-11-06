#include "Ecs/SystemRegistry.h"
#include "SandboxGame.h"
#include "Systems/MoverSystem.h"
#include "Systems/RotatorSystem.h"

// The Sandbox register file: every Sandbox system is listed here, and only this
// file includes the system headers (Plans/ecs-systems.md section 3.3).

namespace Sandbox
{
    void Game::RegisterSystems(Fling::SystemRegistry& t_Systems)
    {
        t_Systems.Add<RotatorSystem>();
        t_Systems.Add<MoverSystem>();
    }
} // namespace Sandbox
