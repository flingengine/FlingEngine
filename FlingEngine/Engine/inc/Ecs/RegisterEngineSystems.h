#pragma once

namespace Fling
{
    class SystemRegistry;

    /** Adds the Engine module's systems (Engine.Input). Engine::Startup calls it before World::Init. */
    void RegisterEngineSystems(SystemRegistry& t_Systems);
} // namespace Fling
