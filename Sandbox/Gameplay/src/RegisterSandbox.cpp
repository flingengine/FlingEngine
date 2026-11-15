#include "BallControl.h"
#include "CameraStatus.h"
#include "Ecs/ModuleRegistration.h"
#include "LookCameraType.h"
#include "Misc/CommandLine.h"
#include "MotionProbe.h"
#include "Mover.h"
#include "SandboxGame.h"
#include "SandboxToggles.h"
#include "StasisType.h"
#include "Systems/AppKeysSystem.h"
#include "Systems/BallControlSystem.h"
#include "Systems/CameraStatusSystem.h"
#include "Systems/FirstHitSystem.h"
#include "Systems/LookCameraSystem.h"
#include "Systems/LookCameraViewSystem.h"
#include "Systems/MotionProbeSystem.h"
#include "Systems/MoverSystem.h"
#include "Systems/RotatorSystem.h"
#include "Systems/StasisSystem.h"
#include "Systems/TestSpawnSystem.h"
#include "Systems/TogglesSystem.h"

// The Sandbox register file: every Sandbox component type and system is listed
// here, one feature at a time, and only this file includes the system headers.

namespace Sandbox
{
    namespace
    {
        void RegisterMovement(Fling::ModuleRegistration& t_Modules)
        {
            t_Modules.Component<Mover>(
                [](Fling::TypeBuilder<Mover>& t_Type)
                {
                    t_Type.Category("Movement");
                    t_Type.Field("targetPos", &Mover::TargetPos);
                    t_Type.Field("speed", &Mover::Speed).Range(0.0f, 100.0f).Tooltip("Units per second");
                });
            t_Modules.Component<Rotator>(
                [](Fling::TypeBuilder<Rotator>& t_Type)
                {
                    t_Type.Category("Movement");
                    t_Type.Field("minPos", &Rotator::MinPos);
                    t_Type.Field("maxPos", &Rotator::MaxPos);
                    t_Type.Field("speed", &Rotator::Speed).Range(0.0f, 100.0f);
                });
        }

        void RegisterBallControl(Fling::ModuleRegistration& t_Modules)
        {
            t_Modules.Component<BallControl>(
                [](Fling::TypeBuilder<BallControl>& t_Type)
                {
                    t_Type.Category("Physics");
                    t_Type.Field("pushForce", &BallControl::m_PushForce).Range(0.0f, 1000.0f).Tooltip("Newtons");
                    t_Type.Field("jumpImpulse", &BallControl::m_JumpImpulse)
                        .Range(0.0f, 1000.0f)
                        .Tooltip("Newton seconds");
                });
            t_Modules.Component<SandboxPhysicsState>(
                [](Fling::TypeBuilder<SandboxPhysicsState>& t_Type)
                {
                    t_Type.Field("hasLoggedHit", &SandboxPhysicsState::m_HasLoggedHit);
                    t_Type.Field("tick", &SandboxPhysicsState::m_Tick);
                });
        }

        void RegisterAppControls(Fling::ModuleRegistration& t_Modules)
        {
            t_Modules.Component<SandboxToggles>(
                [](Fling::TypeBuilder<SandboxToggles>& t_Type)
                {
                    t_Type.Field("rotate", &SandboxToggles::Rotate);
                    t_Type.Field("moveLights", &SandboxToggles::MoveLights);
                });
            t_Modules.Component<SandboxSpawnState>([](Fling::TypeBuilder<SandboxSpawnState>& t_Type)
                                                   { t_Type.Field("lastX", &SandboxSpawnState::LastX); });
        }
    } // namespace

    void Game::Register(Fling::ModuleRegistration& t_Modules)
    {
        RegisterAppControls(t_Modules);
        RegisterBallControl(t_Modules);
        RegisterMovement(t_Modules);
        RegisterLookCameraType(t_Modules);
        RegisterStasisType(t_Modules);
        t_Modules.Component<CameraStatus>();

        // Systems in one list. The order here does not matter: the schedule orders them by what they
        // read and write, and by name where nothing else orders two of them.
        t_Modules.System<AppKeysSystem>();
        t_Modules.System<BallControlSystem>();
        t_Modules.System<FirstHitSystem>();
        t_Modules.System<LookCameraSystem>();
        t_Modules.System<LookCameraViewSystem>();
        t_Modules.System<StasisSystem>();
        t_Modules.System<CameraStatusSystem>();
        t_Modules.System<TestSpawnSystem>();
        t_Modules.System<TogglesSystem>();
        t_Modules.System<RotatorSystem>();
        t_Modules.System<MoverSystem>();

        // Test instrumentation: only headless, where the rendered-motion CTests read its log line
        if (Fling::CommandLine::Get().GetValueAs<bool>("null-render", false))
        {
            t_Modules.Component<MotionProbe>();
            t_Modules.System<MotionProbeSystem>();
        }
    }
} // namespace Sandbox
