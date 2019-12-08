#include "pch.h"
#include "SandboxGame.h"
#include "World.h"
#include "Components/Name.hpp"
#include "Components/Transform.h"
#include "MeshRenderer.h"
#include "Renderer.h"
#include "Stats.h"
#include "Lighting/DirectionalLight.hpp"
#include "Lighting/PointLight.hpp"
#include "Random.h"

#include "Mover.h"

namespace Sandbox
{
    using namespace Fling;

    void Game::Init(entt::registry& t_Reg)
    {
        // Lets create an entity! 
        F_LOG_TRACE("Sandbox Game Init!");

#if WITH_EDITOR
        F_LOG_TRACE("Enable Editor!");
#endif

        // Temp saving and load functions
        Input::BindKeyPress<&Sandbox::Game::PrintFPS>(KeyNames::FL_KEY_1, *this);

        // notify we want to quit when we press escape
        Input::BindKeyPress<&Sandbox::Game::OnQuitPressed>(KeyNames::FL_KEY_ESCAPE, *this);

        // Toggle cursor/mouse visibility with M
        Input::BindKeyPress<&Sandbox::Game::ToggleCursorVisibility>(KeyNames::FL_KEY_M, *this);

        // Toggle model rotation in Update
        Input::BindKeyPress<&Sandbox::Game::ToggleRotation>(KeyNames::FL_KEY_T, *this);
        Input::BindKeyPress<&Sandbox::Game::OnToggleMoveLights>(KeyNames::FL_KEY_SPACE, *this);
		Input::BindKeyPress<&Sandbox::Game::OnTestSpawn>(KeyNames::FL_KEY_0, *this);

        LightingTest(t_Reg);
        //OnLoadInitated();
        //GenerateTestMeshes(t_Reg);

        SetWindowIcon();
    }

    void Game::Shutdown(entt::registry& t_Reg)
    {
        F_LOG_TRACE("Sandbox Game Shutdown!");
    }

	void Game::OnQuitPressed()
	{
		F_LOG_TRACE("The Sandbox game wants to quit!");
		m_WantsToQuit = true;
	}

    void Game::Update(entt::registry& t_Reg, float DeltaTime)
    {
        if (m_DoRotations)
        {
            glm::vec3 RotOffset(0.0f, 15.0f * DeltaTime, 0.0f);

            // For each active mesh renderer
            t_Reg.view<Rotator, Transform>().each([&](Rotator& t_MeshRend, Transform& t_Trans)
                {
                    const glm::vec3& curRot = t_Trans.GetRotation();
                    t_Trans.SetRotation(curRot + RotOffset);
                });
        }

        if (m_MovePointLights)
        {
            t_Reg.view<Mover, Transform>().each([&](Mover& t_Mover, Transform& t_Trans)
                {
                    glm::vec3 newPos = t_Trans.GetPos();
                    if (newPos.x <= t_Mover.MinPos || newPos.x >= t_Mover.MaxPos)
                    {
                        t_Mover.Speed *= -1.0f;
                    }

                    newPos.x += t_Mover.Speed * DeltaTime;
                    t_Trans.SetPos(newPos);
                });
        }
    }

    void Game::LightingTest(entt::registry& t_Reg)
    {
        auto AddModel = [&](INT32 t_Itr, 
			const std::string& t_Model,
			const std::string& t_Mat, 
			const glm::vec3 t_Scale = glm::vec3(1.0f), 
			const glm::vec3 t_Rot = glm::vec3(0.0f))
        {
            entt::entity e0 = t_Reg.create();
            t_Reg.assign<MeshRenderer>(e0, t_Model, t_Mat);
			t_Reg.assign<Rotator>(e0);
            Transform& t0 = t_Reg.assign<Transform>(e0);
            t0.SetPos(glm::vec3(-2.0f + (1.5f * (float)t_Itr), 0.0f, 0.0f));
			t0.SetRotation(t_Rot);
            t0.SetScale(t_Scale);
        };

        auto AddRandomPointLight = [&]()
        {
            entt::entity e0 = t_Reg.create();
            PointLight& Light = t_Reg.assign<PointLight>(e0);
            Transform& t0 = t_Reg.get_or_assign<Transform>(e0);

            Light.DiffuseColor = glm::vec4(Fling::Random::GetRandomVec3(glm::vec3(0.0f), glm::vec3(1.0f)), 1.0f);
            Light.Intensity = 5.0f;
            Light.Range = 3.0f;

			glm::vec3 min = { -15.0f, 0.2f, 15.0f };
			glm::vec3 max = { 15.0f, 1.0f, -15.0f };
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

		// Add a bunch of rando boi
		for (size_t i = 0; i < DeferredLightSettings::MaxPointLights; i++)
		{
			AddRandomPointLight();
		}
		// Spawn a little grid of spheres
		float Spacing = 1.5f;
		INT32 GridSize = 4;
		for (size_t i = 0; i < GridSize; i++)
		{
			for (size_t j = 0; j < GridSize; j++)
			{
				entt::entity e0 = t_Reg.create();
				t_Reg.assign<MeshRenderer>(e0, "Models/sphere.obj", "Materials/DeferredBronzeMat.mat");
				t_Reg.assign<Rotator>(e0);
				Transform& t0 = t_Reg.assign<Transform>(e0);
				t0.SetPos(glm::vec3(((float)i * Spacing), 0.0f, float(j) * Spacing));
			}
		}

        auto AddDirLight = [&](glm::vec3 t_Dir, glm::vec3 t_Color)
        {
            entt::entity e0 = t_Reg.create();
            DirectionalLight& Light = t_Reg.assign<DirectionalLight>(e0);
            Light.Direction = glm::vec4(t_Dir, 1.0f);
            Light.DiffuseColor = glm::vec4(t_Color, 1.0f);
        };

        // Directional Lights
        //AddDirLight(glm::vec3(+1.0f, -1.0f, -0.5f), glm::vec3(1.0f, 1.0f, 1.0f));
    }

    void Game::GenerateTestMeshes(entt::registry& t_Reg)
    {
        // Make a little cube of cubes!
        int Dimension = 10;
        float Offset = 2.5f;

        for (int x = 0; x < Dimension; ++x)
        {
            for (int y = 0; y < Dimension; ++y)
            {
                for (int z = 0; z < Dimension; ++z)
                {
                    entt::entity e0 = t_Reg.create();
                    if (x % 2 == 0)
                    {
                        t_Reg.assign<MeshRenderer>(e0, "Models/cube.obj");
                    }
                    else
                    {
                        t_Reg.assign<MeshRenderer>(e0, "Models/cone.obj", "Materials/Wood.mat");
                    }

                    // Add a transform to this entity
                    Transform& t = t_Reg.assign<Transform>(e0);
                    glm::vec3 pos = glm::vec3(static_cast<float>(x)* Offset, static_cast<float>(y)* Offset, static_cast<float>(z)* Offset);
                    t.SetPos(pos);
                }
            }
        }
    }

    void Game::ToggleCursorVisibility()
    {
        FlingWindow* CurrentWindow = Renderer::Get().GetCurrentWindow();
        if (CurrentWindow)
        {
            CurrentWindow->SetMouseVisible(!CurrentWindow->GetMouseVisible());
        }
    }

    void Game::OnToggleMoveLights()
    {
        m_MovePointLights = !m_MovePointLights;
    }

    void Game::SetWindowIcon()
    {
        FlingWindow* CurrentWindow = Renderer::Get().GetCurrentWindow();
        if (CurrentWindow)
        {
            CurrentWindow->SetWindowIcon("Icons/Fling_Logo.png"_hs);
        }
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

    void Game::PrintFPS() const
    {
        float AvgFrameTime = Fling::Stats::Frames::GetAverageFrameTime();
        F_LOG_TRACE("Frame time: {} FPS: {}", AvgFrameTime, (1.0f / AvgFrameTime));
    }
}	// namespace Sandbox