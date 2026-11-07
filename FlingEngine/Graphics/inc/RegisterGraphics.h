#pragma once

namespace Fling
{
    class ModuleRegistration;

    /**
     * Adds the Graphics module's component types (MeshRenderer, the lights, and the
     * RenderSnapshot) and systems (Graphics.Extract). Engine::Startup calls it before
     * World::Init.
     */
    void RegisterGraphics(ModuleRegistration& t_Modules);
} // namespace Fling
