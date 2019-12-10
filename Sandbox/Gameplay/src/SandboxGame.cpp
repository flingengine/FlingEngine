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

#if WITH_LUA
#include "LuaManager.h"
#endif

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

#if WITH_LUA
		F_LOG_TRACE("Enable Lua!");
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

		// Switch between window modes 
		Input::BindKeyPress<&Sandbox::Game::SetWindowFullscreen>(KeyNames::FL_KEY_2, *this);
		Input::BindKeyPress<&Sandbox::Game::SetWindowWindowed>(KeyNames::FL_KEY_3, *this);
		Input::BindKeyPress<&Sandbox::Game::SetWindowBorderlessWindowed>(KeyNames::FL_KEY_4, *this);

#if WITH_LUA
		Input::BindKeyPress<&Sandbox::Game::ToggleLua>(KeyNames::FL_KEY_L, *this);
#endif

        //LightingTest(t_Reg);
        //OnLoadInitated();
        //GenerateTestMeshes(t_Reg);
		//ScriptingTest(t_Reg);

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
            t_Reg.group<Transform>(entt::get<Rotator>).each([&](auto ent, Transform& t_Trans, Rotator& t_MeshRend)
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

#if WITH_LUA
		if (m_RunLua)
		{
			LuaManager::Get().Tick(DeltaTime);
		}
#endif
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
		INT32 GridSize = 10;
		glm::vec3 Center(0.0f);
		for (size_t i = 0; i < GridSize; i++)
		{
			for (size_t j = 0; j < GridSize; j++)
			{
				entt::entity e0 = t_Reg.create();
				t_Reg.assign<MeshRenderer>(e0, "Models/sphere.obj", "Materials/DeferredBronzeMat.mat");
				t_Reg.assign<Rotator>(e0);
				Transform& t0 = t_Reg.assign<Transform>(e0);

				t0.SetPos(
					glm::vec3(( -(float)(GridSize / 2) + ((float)i * Spacing)), 0.0f, -(float)(GridSize / 2) + float(j) * Spacing )
				);
			}
		}
    }

	void Game::ScriptingTest(entt::registry& t_Reg)
	{
#if WITH_LUA
		entt::entity e0 = t_Reg.create();
		t_Reg.assign<Transform>(e0);
		t_Reg.assign<MeshRenderer>(e0, "Models/cube.obj");
		ScriptComponent& script = t_Reg.assign<ScriptComponent>(e0, "Scripts/Test.lua");

		entt::entity e1 = t_Reg.create();
		t_Reg.assign<Transform>(e1);
		t_Reg.assign<MeshRenderer>(e1, "Models/sphere.obj");
		Transform& t0 = t_Reg.get<Transform>(e1);
		t0.SetPos(glm::vec3(0, 3, 0));
		ScriptComponent& script2 = t_Reg.assign<ScriptComponent>(e1, "Scripts/Test.lua");

		LuaManager::Get().Start();
#endif
	}

    void Game::GenerateTestMeshes(entt::registry& t_Reg)
    {
        // Make a little cube of cubes!
        int Dimension = 1;
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
                        t_Reg.assign<MeshRenderer>(e0, "Models/cube.obj", "Materials/Cobblestone.mat");
                    }
                    else
                    {
                        t_Reg.assign<MeshRenderer>(e0, "Models/cone.obj", "Materials/DeferredBronzeMat.mat");
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

	void Game::ToggleLua()
	{
		m_RunLua = !m_RunLua;
	}

    void Game::SetWindowIcon()
    {
        FlingWindow* CurrentWindow = VulkanApp::Get().GetCurrentWindow();
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

	void Game::SetWindowFullscreen()
	{
		FlingWindow* CurrentWindow = VulkanApp::Get().GetCurrentWindow();
		if (CurrentWindow)
		{
			CurrentWindow->SetWindowMode(WindowMode::Fullscreen);
		}
	}

	void Game::SetWindowBorderlessWindowed()
	{
		FlingWindow* CurrentWindow = VulkanApp::Get().GetCurrentWindow();
		if (CurrentWindow)
		{
			CurrentWindow->SetWindowMode(WindowMode::BorderlessWindowed);
		}
	}

	void Game::SetWindowWindowed()
	{
		FlingWindow* CurrentWindow = VulkanApp::Get().GetCurrentWindow();
		if (CurrentWindow)
		{
			CurrentWindow->SetWindowMode(WindowMode::Windowed);
		}
	}
}	// namespace Sandbox