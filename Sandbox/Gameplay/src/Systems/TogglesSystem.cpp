#include "Systems/TogglesSystem.h"

namespace Sandbox
{
    void TogglesSystem::OnPlayBegin(Context& ctx) const
    {
        ctx.Singleton<SandboxToggles>() = SandboxToggles{};
    }

    void TogglesSystem::FixedUpdate(Context& ctx) const
    {
        const Fling::InputState& input = ctx.Singleton<const Fling::InputState>();
        SandboxToggles& toggles = ctx.Singleton<SandboxToggles>();

        if (input.WasPressed(Fling::Keys::T) || input.WasPressed(Fling::Keys::GamepadFaceLeft))
        {
            toggles.Rotate = !toggles.Rotate;
        }

        auto balls = ctx.Query<const BallControl>();
        const bool hasBall = balls.begin() != balls.end();
        if (!hasBall && (input.WasPressed(Fling::Keys::Space) || input.WasPressed(Fling::Keys::GamepadFaceBottom)))
        {
            toggles.MoveLights = !toggles.MoveLights;
        }
    }
} // namespace Sandbox
