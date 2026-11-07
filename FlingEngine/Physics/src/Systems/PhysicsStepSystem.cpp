#include "Physics.h"
#include "Systems/PhysicsStepSystem.h"

namespace Fling
{
    void PhysicsStepSystem::FixedUpdate(Context& ctx) const
    {
        Physics::Tick(ctx.RawStorage(), ctx.Singleton<const SimTime>().FixedDelta);
    }
} // namespace Fling
