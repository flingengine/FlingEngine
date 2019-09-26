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

	bool Game::Read(entt::registry& t_Reg)
	{
		return false;
	}

	bool Game::Write(entt::registry& t_Reg)
	{
		return false;
	}
}	// namespace Sandbox