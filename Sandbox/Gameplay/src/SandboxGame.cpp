#include "pch.h"
#include "SandboxGame.h"
#include "World.h"
#include "Components/Name.hpp"
#include "Components/Transform.h"
#include "MeshRenderer.h"
#include "Stats.h"
#include "VulkanApp.h"
#include "Lighting/DirectionalLight.hpp"
#include "Lighting/PointLight.hpp"
#include "Random.h"
// For getting some lighting info
#include "GeometrySubpass.h"
#include "Mover.h"

namespace Sandbox
{
    using namespace Fling;

    void Game::Init(entt::registry& t_Reg)
    {
        // Lets create an entity! 
        F_LOG_TRACE("Sandbox Init!");

        // notify we want to quit when we press escape
        Input::BindKeyPress<&Sandbox::Game::OnQuitPressed>(KeyNames::FL_KEY_ESCAPE, *this);

        // Toggle cursor/mouse visibility with M
        Input::BindKeyPress<&Sandbox::Game::ToggleCursorVisibility>(KeyNames::FL_KEY_M, *this);

        // Toggle model rotation in Update
        Input::BindKeyPress<&Sandbox::Game::ToggleRotation>(KeyNames::FL_KEY_T, *this);
        Input::BindKeyPress<&Sandbox::Game::OnToggleMoveLights>(KeyNames::FL_KEY_SPACE, *this);
		Input::BindKeyPress<&Sandbox::Game::OnTestSpawn>(KeyNames::FL_KEY_0, *this);
    }

	void Game::OnStartGame(entt::registry& t_Reg)
	{
		F_LOG_TRACE("Sandbox OnStartGame!");

		LightingTest(t_Reg);
	}

    void Game::Update(entt::registry& t_Reg, float DeltaTime)
    {
        if (m_DoRotations)
        {
            glm::vec3 RotOffset(0.0f, 15.0f * DeltaTime, 0.0f);

            // For each active mesh renderer
            t_Reg.view<Transform, Rotator>().each([&](auto ent, Transform& t_Trans, Rotator& t_MeshRend)
            {
                const glm::vec3& curRot = t_Trans.GetRotation();
                t_Trans.SetRotation(curRot + RotOffset);
            });
        }

        if (m_MovePointLights)
        {
            t_Reg.view<Transform, Mover>().each([&](auto ent, Transform& t_Trans, Mover& t_Mover)
            {
                glm::vec3 curPos = t_Trans.GetPos();

                glm::vec3 DistanceToTarget = curPos - t_Mover.TargetPos;

                // Move in a direction between two points and the speed
                if(glm::length(DistanceToTarget) <= 0.5f)
                {
                    t_Mover.TargetPos = glm::vec3(-1.0f * t_Mover.TargetPos);
                }

                // Lerp towards the target position
                glm::vec3 newPos = glm::lerp(curPos, t_Mover.TargetPos,  t_Mover.Speed * DeltaTime);
                t_Trans.SetPos(newPos);
            });
        }
    }

	void Game::OnStopGame(entt::registry& t_Reg)
	{
		F_LOG_TRACE("Sandbox OnStopGame!");

		auto RotateView = t_Reg.view<Transform, Rotator>();
		t_Reg.destroy(RotateView.begin(), RotateView.end());
		
		auto MoverView = t_Reg.view<Transform, Mover>();
		t_Reg.destroy(MoverView.begin(), MoverView.end());
	}

	void Game::Shutdown(entt::registry& t_Reg)
	{
		F_LOG_TRACE("Sandbox Shutdown!");
	}

	void Game::OnQuitPressed()
	{
		F_LOG_TRACE("The Sandbox game wants to quit!");
		m_WantsToQuit = true;
	}

    void Game::LightingTest(entt::registry& t_Reg)
    {
        
        auto AddRandomPointLight = [&]()
        {
            entt::entity e0 = t_Reg.create();
            PointLight& Light = t_Reg.assign<PointLight>(e0);
            Transform& t0 = t_Reg.get_or_assign<Transform>(e0);
            Mover& m0 = t_Reg.assign<Mover>(e0);

            // The min and max bounds of our little demo
            glm::vec3 min = { -15.0f, 0.2f, 15.0f };
			glm::vec3 max = { 15.0f, 1.0f, -15.0f };
            m0.TargetPos = Fling::Random::GetRandomVec3(min, max);
            
            Light.DiffuseColor = glm::vec4(Fling::Random::GetRandomVec3(glm::vec3(0.0f), glm::vec3(1.0f)), 1.0f);
            Light.Intensity = 5.0f;
            Light.Range = 3.0f;

            t0.SetPos(Fling::Random::GetRandomVec3(min, max));
        };

		auto AddFloor = [&](
			const std::string& t_Model,
			const std::string& t_Mat,
			const glm::vec3 t_Scale = glm::vec3(1.0f)
			)
		{
			entt::entity e0 = t_Reg.create();
			t_Reg.assign<MeshRenderer>(e0, t_Model, t_Mat);
			Transform& t0 = t_Reg.assign<Transform>(e0);
			t0.SetPos(glm::vec3(0.0f, -1.0f, 0.0f));
			t0.SetScale(t_Scale);
		};

		AddFloor("Models/cube.obj", "Materials/Cobblestone.mat", glm::vec3(40.0f, 0.1f, 40.0f));

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
				entt::entity e0 = t_Reg.create();

				// Set the material -----------------
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

				// Set the model -------------
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

    void Game::OnToggleMoveLights()
    {
        m_MovePointLights = !m_MovePointLights;
    }

    void Game::ToggleRotation()
    {
        m_DoRotations = !m_DoRotations;
    }

	void Game::OnTestSpawn()
	{
		entt::registry& t_Reg = m_OwningWorld->GetRegistry();

		static float pos = -1.0f;
		pos -= 1.0f;

		entt::entity e0 = t_Reg.create();
		t_Reg.assign<MeshRenderer>(e0, "Models/sphere.obj", "Materials/DeferredBronzeMat.mat");
		t_Reg.assign<Rotator>(e0);
		Transform& t0 = t_Reg.assign<Transform>(e0);
		t0.SetPos(glm::vec3(pos, 0.0f, 0.0f));
		F_LOG_TRACE("Spawn a sphere to the left!");
	}
}	// namespace Sandbox