#include "Systems/AppKeysSystem.h"

namespace Sandbox
{
    void AppKeysSystem::FixedUpdate(Context& ctx) const
    {
        const Fling::InputSnapshot& input = ctx.Singleton<const Fling::InputSnapshot>();

        if (input.WasPressed(Fling::Keys::Escape) || input.WasPressed(Fling::Keys::GamepadBack))
        {
            ctx.Queue<Fling::AppRequests>().Push(Fling::AppRequest::Quit);
        }
        if (input.WasPressed(Fling::Keys::M) || input.WasPressed(Fling::Keys::GamepadStart))
        {
            ctx.Queue<Fling::AppRequests>().Push(Fling::AppRequest::ToggleCursor);
        }
    }
} // namespace Sandbox
