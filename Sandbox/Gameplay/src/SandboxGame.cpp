#include "pch.h"
#include "SandboxGame.h"

namespace Sandbox
{
	void Game::Init()
	{
	}

	void Game::Shutdown()
	{
	}

	void Game::Update(float DeltaTime)
	{
		F_LOG_TRACE("Sandbox Game update!");
	}

	bool Game::Read()
	{
		return false;
	}

	bool Game::Write()
	{
		return false;
	}
}	// namespace Sandbox