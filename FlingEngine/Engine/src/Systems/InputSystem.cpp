#include "Systems/InputSystem.h"

namespace Fling
{
    void InputSystem::OnCreate(Setup& setup) const
    {
        setup.AddSingleton<InputState>();
    }

    void InputSystem::OnDestroy(Setup& setup) const
    {
        setup.RemoveSingleton<InputState>();
    }

    void InputSystem::FixedUpdate(Context& ctx) const
    {
        ctx.Singleton<InputState>().Tick = ctx.ReadTickInput();
    }
} // namespace Fling
