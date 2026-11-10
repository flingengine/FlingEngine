#include "Systems/InputSystem.h"

namespace Fling
{
    void InputSystem::FixedUpdate(Context& ctx) const
    {
        ctx.Singleton<InputState>().Tick = ctx.ReadTickInput();
    }
} // namespace Fling
