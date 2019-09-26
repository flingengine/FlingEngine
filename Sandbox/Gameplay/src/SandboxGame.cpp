#include "pch.h"
#include "SandboxGame.h"

namespace Sandbox
{
	void Game::Init(entt::registry& t_Reg)
	{
		// Lets create an entity! 
		F_LOG_TRACE("Sandbox Game Init!");
	}

	void Game::Shutdown(entt::registry& t_Reg)
	{
		F_LOG_TRACE("Sandbox Game Shutdown!");
	}

	void Game::Update(entt::registry& t_Reg, float DeltaTime)
	{
	}

	void Game::Read(entt::registry& t_Reg, nlohmann::json& t_JsonData)
	{
		F_LOG_TRACE("Sandbox Game Read!");

		// #TODO Read in custom game component systems
	}

	void Game::Write(entt::registry& t_Reg, nlohmann::json& t_JsonData)
	{
		F_LOG_TRACE("Sandbox Game Write!");
		// #TODO Write out custom game component systems
	}
}	// namespace Sandbox