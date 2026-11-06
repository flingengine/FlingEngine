#include "Logger.h"
#include "Systems/TestSpawnSystem.h"

namespace Sandbox
{
    void TestSpawnSystem::OnCreate(Setup& setup) const
    {
        setup.AddSingleton<SandboxSpawnState>();
    }

    void TestSpawnSystem::OnDestroy(Setup& setup) const
    {
        setup.RemoveSingleton<SandboxSpawnState>();
    }

    void TestSpawnSystem::OnPlayBegin(Context& ctx) const
    {
        ctx.Singleton<SandboxSpawnState>() = SandboxSpawnState{};
    }

    void TestSpawnSystem::FixedUpdate(Context& ctx) const
    {
        const Fling::InputState& input = ctx.Singleton<const Fling::InputState>();
        if (!input.WasPressed(Fling::Keys::Zero) && !input.WasPressed(Fling::Keys::GamepadFaceTop))
        {
            return;
        }

        SandboxSpawnState& state = ctx.Singleton<SandboxSpawnState>();
        state.LastX -= 1.0f;

        Fling::Transform transform;
        transform.SetPos(glm::vec3(state.LastX, 0.0f, 0.0f));
        ctx.Spawn(Fling::MeshRenderer("Models/sphere.obj", "Materials/DeferredBronzeMat.mat"), Rotator{}, transform);
        F_LOG_TRACE("Spawn a sphere to the left!");
    }
} // namespace Sandbox
