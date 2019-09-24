#include "pch.h"
#include "SandboxGame.h"

namespace Sandbox
{
	void Game::Init(entt::registry& t_Reg)
	{
		// Lets create an entity! 
		entt::entity ent = t_Reg.create();
	}

	void Game::Shutdown(entt::registry& t_Reg)
	{
	}

	void Game::Update(entt::registry& t_Reg, float DeltaTime)
	{
		F_LOG_TRACE("Sandbox Game update!");
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