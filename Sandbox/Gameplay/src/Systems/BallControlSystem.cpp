#include "Systems/BallControlSystem.h"

namespace Sandbox
{
    void BallControlSystem::FixedUpdate(Context& ctx) const
    {
        const Fling::InputSnapshot& input = ctx.Singleton<const Fling::InputSnapshot>();

        glm::vec3 dir(0.0f);
        if (input.IsHeld(Fling::Keys::Up) || input.IsHeld(Fling::Keys::GamepadDPadUp))
        {
            dir.z -= 1.0f;
        }
        if (input.IsHeld(Fling::Keys::Down) || input.IsHeld(Fling::Keys::GamepadDPadDown))
        {
            dir.z += 1.0f;
        }
        if (input.IsHeld(Fling::Keys::Left) || input.IsHeld(Fling::Keys::GamepadDPadLeft))
        {
            dir.x -= 1.0f;
        }
        if (input.IsHeld(Fling::Keys::Right) || input.IsHeld(Fling::Keys::GamepadDPadRight))
        {
            dir.x += 1.0f;
        }
        const bool jump = input.WasPressed(Fling::Keys::Space) || input.WasPressed(Fling::Keys::GamepadFaceBottom);

        for (auto [entity, control] : ctx.Query<const BallControl>())
        {
            if (glm::length(dir) > 0.0f)
            {
                ctx.Queue<Fling::PhysicsCommands>().Push(Fling::PhysicsCommand{
                    entity, Fling::PhysicsCommandType::Force, glm::normalize(dir) * control.m_PushForce});
            }
            if (jump)
            {
                ctx.Queue<Fling::PhysicsCommands>().Push(Fling::PhysicsCommand{
                    entity, Fling::PhysicsCommandType::Impulse, glm::vec3(0.0f, control.m_JumpImpulse, 0.0f)});
            }
        }
    }
} // namespace Sandbox
