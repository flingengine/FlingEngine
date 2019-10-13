#include "pch.h"
#include "SandboxGame.h"
#include "World.h"
#include "Components/Name.hpp"
#include "Components/Transform.h"
#include "MeshRenderer.h"
#include "Stats.h"

namespace Sandbox
{
	using namespace Fling;

	void Game::Init(entt::registry& t_Reg)
	{
		// Lets create an entity! 
		F_LOG_TRACE("Sandbox Game Init!");

		// Temp saving and load functions
		Input::BindKeyPress<&Sandbox::Game::OnLoadInitated>(KeyNames::FL_KEY_O, *this);
        Input::BindKeyPress<&Sandbox::Game::OnSaveInitated>(KeyNames::FL_KEY_P, *this);
        Input::BindKeyPress<&Sandbox::Game::PrintFPS>(KeyNames::FL_KEY_1, *this);

		// notify we want to quit when we press escape
		Input::BindKeyPress<&Sandbox::Game::OnQuitPressed>(KeyNames::FL_KEY_ESCAPE, *this);

		GenerateTestMeshes(t_Reg);
	}

	void Game::Shutdown(entt::registry& t_Reg)
	{
		F_LOG_TRACE("Sandbox Game Shutdown!");
	}

	void Game::Update(entt::registry& t_Reg, float DeltaTime)
	{
		glm::vec3 Offset( 15.0f * DeltaTime );
		// For each active mesh renderer
		t_Reg.view<MeshRenderer, Transform>().each([&](MeshRenderer& t_MeshRend, Transform& t_Trans)
		{
			glm::vec3 curRot = t_Trans.GetRotation();
			t_Trans.SetRotation(curRot + Offset);
		});
	}

	void Game::OnLoadInitated()
	{
		Fling::World* World = GetWorld();
		assert(World);

		World->LoadLevelFile<Fling::NameComponent>(FlingConfig::GetString("Game", "StartLevel"));
		entt::registry& t_Reg = World->GetRegistry();
		
		// For testing -----
		t_Reg.view<NameComponent, Transform>().each([&](entt::entity t_Ent, NameComponent& t_Name, Transform& t_Trans)
		{
			F_LOG_TRACE("Entity has name {}  and transform {}", t_Name.Name, t_Trans);
		});
	}

	void Game::OnSaveInitated()
	{
		Fling::World* World = GetWorld();
		assert(World);

		// Write out the file
		World->OutputLevelFile<Fling::NameComponent>(FlingConfig::GetString("Game", "StartLevel"));
	}

	void Game::OnQuitPressed()
	{
		F_LOG_TRACE("The Sandbox game wants to quit!");
		m_WantsToQuit = true;
	}

	void Game::GenerateTestMeshes(entt::registry& t_Reg)
	{
		// Make a little cube of cubes!
		int Dimension = 5;
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
						t_Reg.assign<MeshRenderer>(e0, "Models/cone.obj");
					}

					// Add a transform to this entity
					Transform& t = t_Reg.assign<Transform>(e0);
					glm::vec3 pos = glm::vec3(static_cast<float>(x) * Offset, static_cast<float>(y) * Offset, static_cast<float>(z) * Offset);
					t.SetPos(pos);
				}
			}
		}
	}

    void Game::PrintFPS() const
    {
        float AvgFrameTime = Fling::Stats::Frames::GetAverageFrameTime();
        F_LOG_TRACE("Frame time: {} FPS: {}", AvgFrameTime, (1.0f /AvgFrameTime));
    }

}	// namespace Sandbox