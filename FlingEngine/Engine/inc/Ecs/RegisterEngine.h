#pragma once

namespace Fling
{
    class ModuleRegistration;

    /**
     * Adds the Engine module's component types (Transform, NameComponent, EntityGuid,
     * and the engine singletons) and systems (Engine.Input). Engine::Startup calls it
     * before World::Init.
     */
    void RegisterEngine(ModuleRegistration& t_Modules);
} // namespace Fling
