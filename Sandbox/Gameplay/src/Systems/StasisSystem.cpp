#include "Systems/StasisSystem.h"

namespace Sandbox
{
    void StasisSystem::FixedUpdate(Context& ctx) const
    {
        const bool pressed = ctx.Singleton<const Fling::InputState>().WasPressed(Fling::Keys::G);
        const int64 tickNs = ctx.Singleton<const Fling::SimTime>().FixedDeltaNs;
        for (auto [entity, stasis, camera] : ctx.Query<Stasis, LookCamera>())
        {
            StasisUtil::Step(stasis, camera.Policy, pressed, tickNs);
        }
    }
} // namespace Sandbox
