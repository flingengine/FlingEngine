#include "Ecs/SystemRegistry.h"
#include "SandboxGame.h"
#include "Systems/AppKeysSystem.h"
#include "Systems/BallControlSystem.h"
#include "Systems/FirstHitSystem.h"
#include "Systems/MoverSystem.h"
#include "Systems/RotatorSystem.h"
#include "Systems/TestSpawnSystem.h"
#include "Systems/TogglesSystem.h"

// The Sandbox register file: every Sandbox system is listed here, and only this
// file includes the system headers (Plans/ecs-systems.md section 3.3).

namespace Sandbox
{
    void Game::RegisterSystems(Fling::SystemRegistry& t_Systems)
    {
        t_Systems.Add<AppKeysSystem>();
        t_Systems.Add<BallControlSystem>();
        t_Systems.Add<FirstHitSystem>();
        t_Systems.Add<TestSpawnSystem>();
        t_Systems.Add<TogglesSystem>();
        t_Systems.Add<RotatorSystem>();
        t_Systems.Add<MoverSystem>();
    }
} // namespace Sandbox
