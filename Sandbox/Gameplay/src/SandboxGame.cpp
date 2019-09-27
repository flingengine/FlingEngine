#include "pch.h"
#include "SandboxGame.h"
#include "World.h"
#include "Components/Name.hpp"
#include "Components/Transform.hpp"

namespace Sandbox
{
	using namespace Fling;

	void Game::Init(entt::registry& t_Reg)
	{
		// Lets create an entity! 
		F_LOG_TRACE("Sandbox Game Init!");
		Input::BindKeyPress<&Sandbox::Game::OnKeyPress>(KeyNames::FL_KEY_M, *this);
	}

	void Game::Shutdown(entt::registry& t_Reg)
	{
		F_LOG_TRACE("Sandbox Game Shutdown!");
	}

	void Game::Update(entt::registry& t_Reg, float DeltaTime)
	{
		Fling::World* World = GetWorld();
		assert(World);

		// #TODO Move this input polling to a delete function instead of polling every frame
		if(Input::IsKeyDown(Fling::KeyNames::FL_KEY_O))
		{
			// Add some test entities  -------------------
			entt::entity e0 = t_Reg.create();
			t_Reg.assign<Fling::Transform>(e0);
			t_Reg.assign<Fling::NameComponent>(e0, "Entity 0 Name");

			entt::entity e1 = t_Reg.create();
			t_Reg.assign<Fling::Transform>(e1);
			t_Reg.assign<Fling::NameComponent>(e1, "Entity 1 Name");
			// end For testing -------------------

			// Write out the file
			World->OutputLevelFile<Fling::NameComponent>(FlingConfig::GetString("Game", "StartLevel"));
		}
		else if(Input::IsKeyDown(Fling::KeyNames::FL_KEY_P))
		{
			// Load in the file
			World->LoadLevelFile<Fling::NameComponent>(FlingConfig::GetString("Game", "StartLevel"));

			t_Reg.view<NameComponent, Transform>().each([&](entt::entity t_Ent, NameComponent& t_Name, Transform& t_Trans)
			{
				F_LOG_TRACE("Entity has name {}  and pos {},{},{}",
				 	t_Name.Name, t_Trans.Pos.x, t_Trans.Pos.y, t_Trans.Pos.z
				);
			});
		}
	}

	void Game::OnKeyPress()
	{
		F_LOG_TRACE("THE KEY WAS PRESSED FROM A DELEGATE BOI");
	}

}	// namespace Sandbox