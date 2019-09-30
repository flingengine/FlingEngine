#include "pch.h"
#include "SandboxGame.h"
#include "World.h"
#include "Components/Name.hpp"
#include "Components/Transform.h"

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

		// notify we want to quit when we press escape
		Input::BindKeyPress<&Sandbox::Game::OnQuitPressed>(KeyNames::FL_KEY_ESCAPE, *this);
	}

	void Game::Shutdown(entt::registry& t_Reg)
	{
		F_LOG_TRACE("Sandbox Game Shutdown!");
	}

	void Game::Update(entt::registry& t_Reg, float DeltaTime)
	{

	}

	void Game::OnLoadInitated()
	{
		Fling::World* World = GetWorld();
		assert(World);

		World->LoadLevelFile<Fling::NameComponent>(FlingConfig::GetString("Game", "StartLevel"));
		entt::registry& t_Reg = World->GetRegistry();
		
		t_Reg.view<NameComponent, Transform>().each([&](entt::entity t_Ent, NameComponent& t_Name, Transform& t_Trans)
		{
			F_LOG_TRACE("Entity has name {}  and transform {}", t_Name.Name, t_Trans);
		});
	}

	void Game::OnSaveInitated()
	{
		Fling::World* World = GetWorld();
		assert(World);

		entt::registry& t_Reg = World->GetRegistry();

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

	void Game::OnQuitPressed()
	{
		F_LOG_TRACE("The Sandbox game wants to quit!");
		m_WantsToQuit = true;
	}

}	// namespace Sandbox