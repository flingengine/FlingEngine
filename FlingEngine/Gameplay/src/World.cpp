#include "pch.h"
#include "World.h"

namespace Fling
{
	World::World(entt::registry& t_Reg, Fling::Game* t_Game)
		: m_Registry(t_Reg)
		, m_Game(t_Game)
	{ }

    void World::Init()
    {
        F_LOG_TRACE("World Init!");

		// Load the that is specific in the config file
		std::string LevelToLoad = FlingConfig::GetString("Game", "StartLevel");
		
		// Initalize the game!
		m_Game->Init(m_Registry);
    }

    void World::Shutdown()
    {
        F_LOG_TRACE("World shutdown!");
		
		// Shut down the game
		m_Game->Shutdown(m_Registry);
    }
	
    void World::Update(float t_DeltaTime)
    {
		// TODO: Update any _world_ systems 

		// The physics of our objects (position and what not)

		// Once we are done with core updates, then call the game!
		m_Game->Update(m_Registry, t_DeltaTime);
    }
} // namespace Fling