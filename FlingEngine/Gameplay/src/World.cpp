#include "pch.h"
#include "World.h"

#if WITH_LUA
#include "LuaManager.h"
#endif

namespace Fling
{
	World::World(entt::registry& t_Reg, Fling::Game* t_Game)
		: m_Registry(t_Reg)
		, m_Game(t_Game)
		, m_ShouldQuit(false)
	{ }

    void World::Init()
    {
        F_LOG_TRACE("World Init!");
		assert(m_CurrentState == WorldState::NONE);

		// Load the that is specific in the config file
		std::string LevelToLoad = FlingConfig::GetString("Game", "StartLevel");
		
		// Initialize the game! Here is where people will load lua scripts and binnd input callbacks
		m_Game->Init(m_Registry);

		m_CurrentState = WorldState::Initalized;
    }

    void World::Shutdown()
    {
		m_CurrentState = WorldState::Quitting;

		// Stop the game
		m_Game->OnStopGame(m_Registry);

		m_CurrentState = WorldState::ShuttingDown;

		// Shut down the game
		m_Game->Shutdown(m_Registry);

		F_LOG_TRACE("World shutdown complete!");
    }

	void World::RequestGameStart()
	{
		assert(m_CurrentState == WorldState::Initalized);

		m_CurrentState = WorldState::Starting;

		// Start game logic here like moving of objects, changing properties, etc
		m_Game->OnStartGame(m_Registry);
#if WITH_LUA
		// Call the start functions on any Lua scripts
		LuaManager::Get().Start();
#endif
		m_CurrentState = WorldState::Playing;
	}

	void World::RequestGameStop()
	{
		assert(m_CurrentState == WorldState::Playing);

		// Start game logic here like moving of objects, changing properties, etc
		m_Game->OnStopGame(m_Registry);

		m_CurrentState = WorldState::Initalized;

		// Load a level back so that we clear out the game state
	}
	
    void World::Update(float t_DeltaTime)
    {
		if(m_CurrentState == WorldState::Playing)
		{
			// Once we are done with core updates, then call the game!
			m_Game->Update(m_Registry, t_DeltaTime);

#if WITH_LUA
			LuaManager::Get().Tick(t_DeltaTime);
#endif

			// #TODO Update physics here
		}
    }
} // namespace Fling