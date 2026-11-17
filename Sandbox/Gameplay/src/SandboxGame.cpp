#include "FlingWindow.h"
#include "Logger.h"
#include "LookCamera.h"
#include "SandboxGame.h"
#include "Stasis.h"
#include "Stats.h"
#include "Strings/String.h"
#include "VulkanApp.h"
#include "World.h"

namespace Sandbox
{
    using namespace Fling;

    void Game::Init(Fling::EntityStorage&)
    {
        F_LOG_TRACE("Sandbox Init!");
    }

    void Game::OnStartGame(Fling::EntityStorage& t_Reg)
    {
        F_LOG_TRACE("Sandbox OnStartGame!");

        // The mouse look camera, unless the level has one. Sandbox.LookCamera turns it each
        // tick, Sandbox.Stasis can stop it (G), and Sandbox.LookCameraView writes the CameraView
        // that the renderer draws with. It has no Transform, so the transform checksum does not change.
        if (t_Reg.View<LookCamera>().Empty())
        {
            t_Reg.Create(LookCamera{}, Stasis{});
        }
    }

    void Game::OnStopGame(Fling::EntityStorage& t_Reg)
    {
        F_LOG_TRACE("Sandbox OnStopGame!");

        // The world removes the entities that play made. The Sandbox systems reset their
        // singletons in OnPlayBegin. Play can hide the cursor (M): show it again for the editor.
        FlingWindow* CurrentWindow = VulkanApp::Get().GetCurrentWindow();
        if (CurrentWindow && !CurrentWindow->GetMouseVisible())
        {
            CurrentWindow->SetMouseVisible(true);
        }
    }

    void Game::Shutdown(Fling::EntityStorage& t_Reg)
    {
        F_LOG_TRACE("Sandbox Shutdown!");

        // A checksum of every transform, to check that a change to gameplay code gives the same motion
        double Checksum = 0.0;
        std::size_t Count = 0;
        t_Reg.View<Transform>().Each(
            [&](auto, Transform& t_Trans)
            {
                const glm::vec3 Pos = t_Trans.Position;
                const glm::quat Rot = t_Trans.Rotation;
                Checksum +=
                    Pos.x + 3.0 * Pos.y + 7.0 * Pos.z + 11.0 * Rot.x + 13.0 * Rot.y + 17.0 * Rot.z + 19.0 * Rot.w;
                ++Count;
            });
        F_LOG_TRACE("Sandbox: {} transforms, checksum {:.9f}", Count, Checksum);
    }
} // namespace Sandbox