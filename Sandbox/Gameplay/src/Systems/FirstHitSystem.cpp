#include "Logger.h"
#include "Systems/FirstHitSystem.h"

namespace Sandbox
{
    void FirstHitSystem::OnCreate(Setup& setup) const
    {
        setup.AddSingleton<SandboxPhysicsState>();
    }

    void FirstHitSystem::OnDestroy(Setup& setup) const
    {
        setup.RemoveSingleton<SandboxPhysicsState>();
    }

    void FirstHitSystem::OnPlayBegin(Context& ctx) const
    {
        ctx.Singleton<SandboxPhysicsState>() = SandboxPhysicsState{};
    }

    void FirstHitSystem::FixedUpdate(Context& ctx) const
    {
        SandboxPhysicsState& state = ctx.Singleton<SandboxPhysicsState>();

        if (!state.m_HasLoggedHit)
        {
            const auto isBall = [&ctx](Fling::Entity entity)
            {
                return ctx.TryGet<const BallControl>(entity) != nullptr;
            };
            const auto isMovingBody = [&ctx](Fling::Entity entity)
            {
                const Fling::RigidBody* body = ctx.TryGet<const Fling::RigidBody>(entity);
                return body != nullptr && body->m_Mass > 0.0f;
            };

            for (const Fling::ContactEvent& contact : ctx.Singleton<const Fling::ContactEvents>().m_Contacts)
            {
                const bool aIsBall = isBall(contact.m_A);
                const bool bIsBall = isBall(contact.m_B);
                const Fling::Entity other = aIsBall ? contact.m_B : contact.m_A;
                if (aIsBall != bIsBall && isMovingBody(other))
                {
                    F_LOG_TRACE("PhysicsTest: ball hit cube on tick {}", state.m_Tick);
                    state.m_HasLoggedHit = true;
                    break;
                }
            }
        }

        ++state.m_Tick;
    }
} // namespace Sandbox
