#include "BallControl.h"
#include "Components/Name.hpp"
#include "Components/Transform.h"
#include "Lighting/DirectionalLight.hpp"
#include "Lighting/PointLight.hpp"
#include "MeshRenderer.h"
#include "Physics.h"
#include "Random.h"
#include "SandboxGame.h"
#include "Stats.h"
#include "VulkanApp.h"
#include "World.h"
// For getting some lighting info
#include "ComponentTypeRegistry.h"
#include "Lighting/DeferredLightSettings.h"
#include "Mover.h"

// Test command line args
#include "FlingWindow.h"
#include "Input/Input.h"
#include "Logger.h"
#include "Misc/CommandLine.h"

namespace Sandbox
{
    using namespace Fling;

    namespace
    {
        /**
         * Push and jump each BallControl body with the player's input. The requests
         * are queued, and physics applies them in this tick's physics step.
         * Reads: BallControl, Input. Writes: PhysicsCommands.
         */
        void BallControlSystem(entt::registry& t_Reg, const Input& t_Input)
        {
            glm::vec3 Dir(0.0f);
            if (t_Input.IsHeld(Keys::Up) || t_Input.IsHeld(Keys::GamepadDPadUp))
            {
                Dir.z -= 1.0f;
            }
            if (t_Input.IsHeld(Keys::Down) || t_Input.IsHeld(Keys::GamepadDPadDown))
            {
                Dir.z += 1.0f;
            }
            if (t_Input.IsHeld(Keys::Left) || t_Input.IsHeld(Keys::GamepadDPadLeft))
            {
                Dir.x -= 1.0f;
            }
            if (t_Input.IsHeld(Keys::Right) || t_Input.IsHeld(Keys::GamepadDPadRight))
            {
                Dir.x += 1.0f;
            }
            const bool bJump = t_Input.WasPressed(Keys::Space) || t_Input.WasPressed(Keys::GamepadFaceBottom);

            t_Reg.view<BallControl>().each([&](entt::entity t_Entity, const BallControl& t_Control)
            {
                if (glm::length(Dir) > 0.0f)
                {
                    Physics::ApplyForce(t_Reg, t_Entity, glm::normalize(Dir) * t_Control.m_PushForce);
                }
                if (bJump)
                {
                    Physics::ApplyImpulse(t_Reg, t_Entity, glm::vec3(0.0f, t_Control.m_JumpImpulse, 0.0f));
                }
            });
        }

        /**
         * Log the first time that a BallControl body hits another moving body. The
         * headless physics test level looks for this line.
         * Reads: ContactEvents (from the last physics step), BallControl, RigidBody.
         * Writes: SandboxPhysicsState.
         */
        void LogFirstHitSystem(entt::registry& t_Reg)
        {
            SandboxPhysicsState* State = t_Reg.try_ctx<SandboxPhysicsState>();
            const ContactEvents* Contacts = t_Reg.try_ctx<ContactEvents>();
            if (State == nullptr || Contacts == nullptr || State->m_HasLoggedHit)
            {
                return;
            }

            auto IsMovingBody = [&t_Reg](entt::entity t_Entity)
            {
                return t_Reg.valid(t_Entity) && t_Reg.has<RigidBody>(t_Entity) && t_Reg.get<RigidBody>(t_Entity).m_Mass > 0.0f;
            };

            for (const ContactEvent& Contact : Contacts->m_Contacts)
            {
                const bool bAIsBall = t_Reg.valid(Contact.m_A) && t_Reg.has<BallControl>(Contact.m_A);
                const bool bBIsBall = t_Reg.valid(Contact.m_B) && t_Reg.has<BallControl>(Contact.m_B);
                const entt::entity Other = bAIsBall ? Contact.m_B : Contact.m_A;
                if ((bAIsBall || bBIsBall) && !(bAIsBall && bBIsBall) && IsMovingBody(Other))
                {
                    F_LOG_TRACE("PhysicsTest: ball hit cube on tick {}", State->m_Tick);
                    State->m_HasLoggedHit = true;
                    return;
                }
            }
        }
    } // namespace

    void Game::Init(entt::registry& t_Reg)
    {
        // Lets create an entity! 
        F_LOG_TRACE("Sandbox Init!");

		{
			using namespace Fling;
			const CommandLine& CmdLineOpts = CommandLine::Get();
			const bool bMyOption = CmdLineOpts.GetValueAs<bool>("BenTest", false);
			F_LOG_TRACE("Sandbox:Ben Test : {}\t", bMyOption ? "true" : "false");

			const double DoubleOpt = CmdLineOpts.GetValueAs<double>("TestDouble", -1.0);
			F_LOG_TRACE("Sandbox:Ben Test : {}\t", DoubleOpt);
		}

		F_LOG_TRACE("Done cmd line testing");
    }

	void Game::RegisterComponents()
	{
		Fling::ComponentTypeRegistry::Get().Register<Mover>();
		Fling::ComponentTypeRegistry::Get().Register<Rotator>();
		Fling::ComponentTypeRegistry::Get().Register<BallControl>();
	}

	void Game::OnStartGame(entt::registry& t_Reg)
	{
		F_LOG_TRACE("Sandbox OnStartGame!");

        // Play can start many times in one editor run, so each play starts from the same state
        ResetPlayState();
        t_Reg.set<SandboxPhysicsState>();

        // A level with physics bodies (for example Levels/PhysicsTest.json) is a
        // scene of its own. Only an empty level gets the lighting test grid.
        if (t_Reg.view<RigidBody>().empty())
        {
            LightingTest(t_Reg);
        }
	}

    void Game::Update(entt::registry& t_Reg, float DeltaTime)
    {
        const Input& GameInput = Input::Get();

        // Each action has a keyboard key and a gamepad button (Xbox names in the comments)
        auto WasPressed = [&GameInput](InputKey t_Key, InputKey t_GamepadButton)
        {
            return GameInput.WasPressed(t_Key) || GameInput.WasPressed(t_GamepadButton);
        };

        // Quit with Escape or View
        if (WasPressed(Keys::Escape, Keys::GamepadBack))
        {
            OnQuitPressed();
        }

        // Toggle cursor/mouse visibility with M or Menu
        if (WasPressed(Keys::M, Keys::GamepadStart))
        {
            ToggleCursorVisibility();
        }

        // T and Space toggle the lighting test in the Sandbox.Toggles system, which reads
        // the tick's InputState.

        // Physics test level: the arrow keys push the ball and Space jumps.
        // ContactEvents is from the physics step of the last tick.
        const bool bHasBall = !t_Reg.view<BallControl>().empty();
        if (bHasBall)
        {
            BallControlSystem(t_Reg, GameInput);
            LogFirstHitSystem(t_Reg);
        }
        if (SandboxPhysicsState* PhysicsState = t_Reg.try_ctx<SandboxPhysicsState>())
        {
            ++PhysicsState->m_Tick;
        }

        // Spawn a test entity with 0 or Y
        if (WasPressed(Keys::Zero, Keys::GamepadFaceTop))
        {
            OnTestSpawn();
        }

        // The Rotator and Mover loops are systems now (Sandbox/Gameplay/inc/Systems). They
        // run in the fixed simulation tick, before this update.
    }

	void Game::OnStopGame(entt::registry& t_Reg)
	{
		F_LOG_TRACE("Sandbox OnStopGame!");

        // The world removes the entities that play made. Only state outside the
        // registry is cleaned up here.
        ResetPlayState();

        // Play can hide the cursor (M). Show it again for the editor.
        FlingWindow* CurrentWindow = VulkanApp::Get().GetCurrentWindow();
        if (CurrentWindow && !CurrentWindow->GetMouseVisible())
        {
            CurrentWindow->SetMouseVisible(true);
        }
    }

    void Game::ResetPlayState()
    {
        m_TestSpawnX = -1.0f;
    }

    void Game::Shutdown(entt::registry& t_Reg)
	{
		F_LOG_TRACE("Sandbox Shutdown!");

		// A checksum of every transform, to check that a change to gameplay code gives the same motion
		double Checksum = 0.0;
		std::size_t Count = 0;
		t_Reg.view<Transform>().each([&](auto, Transform& t_Trans)
		{
			const glm::vec3 Pos = t_Trans.GetPos();
			const glm::vec3 Rot = t_Trans.GetRotation();
			Checksum += Pos.x + 3.0 * Pos.y + 7.0 * Pos.z + 11.0 * Rot.x + 13.0 * Rot.y + 17.0 * Rot.z;
			++Count;
		});
		F_LOG_TRACE("Sandbox: {} transforms, checksum {:.9f}", Count, Checksum);
	}

	void Game::OnQuitPressed()
	{
		F_LOG_TRACE("The Sandbox game wants to quit!");
		m_WantsToQuit = true;
	}

	void Game::AddFloor(entt::registry& t_Reg, const std::string& t_Model, const std::string& t_Mat, const glm::vec3 t_Scale /* = glm::vec3(1.0f) */)
	{
		entt::entity e0 = t_Reg.create();
		t_Reg.assign<MeshRenderer>(e0, t_Model, t_Mat);
		Transform& t0 = t_Reg.assign<Transform>(e0);
		t0.SetPos(glm::vec3(0.0f, -1.0f, 0.0f));
		t0.SetScale(t_Scale);
	}

    void Game::LightingTest(entt::registry& t_Reg)
    {
        
        auto AddRandomPointLight = [&]()
        {
            entt::entity e0 = t_Reg.create();

            glm::vec3 min = { -15.0f, 0.2f, -15.0f };
			glm::vec3 max = { 15.0f, 1.0f, 15.0f };
			const glm::vec3 startPos = Fling::Random::GetRandomVec3(min, max);

			// The renderer adds a Transform to a new PointLight, but there is no renderer
			// under --null-render. get_or_assign works in both cases.
            PointLight& Light = t_Reg.assign<PointLight>(e0);
			Transform& t0 = t_Reg.get_or_assign<Transform>(e0);
			t0.SetPos(startPos);
            Mover& m0 = t_Reg.assign<Mover>(e0);
            m0.TargetPos = Fling::Random::GetRandomVec3(min, max);
            
            Light.DiffuseColor = glm::vec4(Fling::Random::GetRandomVec3(glm::vec3(0.0f), glm::vec3(1.0f)), 1.0f);
            Light.Intensity = 5.0f;
            Light.Range = 3.0f;
        };

		

		AddFloor(t_Reg, "Models/cube.obj", "Materials/Cobblestone.mat", glm::vec3(40.0f, 0.1f, 40.0f));

		// Add a bunch of random light bois
		for (size_t i = 0; i < DeferredLightSettings::MaxPointLights; i++)
		{
			AddRandomPointLight();
		}

		// Spawn a little grid of spheres
		float Spacing = 1.5f;
		int32 GridSize = 25;
		
		for (int32 i = 0; i < GridSize; i++)
		{
			std::string ModelPath = "Models/sphere.obj";
			std::string MatPath = "Materials/DeferredBronzeMat.mat";

			for (int32 j = 0; j < GridSize; j++)
			{
				if (j == 0)
				{
					MatPath = "Materials/Damascus.mat";
				}
				else if (j == 1)
				{
					MatPath = "Materials/SheetMetal.mat";
				}
				else if (j == 2)
				{
					MatPath = "Materials/Snow.mat";
				}
				else if (j == 3)
				{
					MatPath = "Materials/Cobblestone.mat";
				}
				else if (j == 4)
				{
					MatPath = "Materials/DotMetal.mat";
				}
				else
				{
					MatPath = "Materials/DeferredBronzeMat.mat";
				}

				if (i == 1)
				{
					ModelPath = "Models/cube.obj";
				}
				else if(i == 2)
				{
					ModelPath = "Models/cone.obj";
				}
				else if (i == 3)
				{
					ModelPath = "Models/RoundedCube.obj";
				}
				else if (i == 4)
				{
					ModelPath = "Models/torus.obj";
				}
				else
				{
					ModelPath = "Models/sphere.obj";
				}

				entt::entity e0 = t_Reg.create();
				t_Reg.assign<MeshRenderer>(e0, ModelPath.c_str(), MatPath.c_str());
				t_Reg.assign<Rotator>(e0);
				Transform& t0 = t_Reg.assign<Transform>(e0);
				t0.SetPos(
					glm::vec3(( -(float)(GridSize / 2) + ((float)i * Spacing)), (i > GridSize / 2) ? 1.0f : 0.0f, -(float)(GridSize / 2) + float(j) * Spacing )
				);
			}
		}
    }

    void Game::ToggleCursorVisibility()
    {
		// You have to include VulkanApp for this
        FlingWindow* CurrentWindow = VulkanApp::Get().GetCurrentWindow();
        if (CurrentWindow)
        {
            CurrentWindow->SetMouseVisible(!CurrentWindow->GetMouseVisible());
        }
    }

	void Game::OnTestSpawn()
	{
		entt::registry& t_Reg = m_OwningWorld->GetRegistry();

        m_TestSpawnX -= 1.0f;

        entt::entity e0 = t_Reg.create();
		t_Reg.assign<MeshRenderer>(e0, "Models/sphere.obj", "Materials/DeferredBronzeMat.mat");
		t_Reg.assign<Rotator>(e0);
		Transform& t0 = t_Reg.assign<Transform>(e0);
        t0.SetPos(glm::vec3(m_TestSpawnX, 0.0f, 0.0f));
        F_LOG_TRACE("Spawn a sphere to the left!");
	}
}	// namespace Sandbox