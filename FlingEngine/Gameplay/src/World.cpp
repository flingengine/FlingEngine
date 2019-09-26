#include "pch.h"
#include "World.h"
#include "FlingConfig.h"
#include "JsonFile.h"

// Test component serialization
#include "Components/Transform.hpp"

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
		
		LoadLevel(LevelToLoad);

		// Initalize the game!
		m_Game->Init(m_Registry);
    }

    void World::Shutdown()
    {
        F_LOG_TRACE("World shutdown!");
		
		// Shut down the game
		m_Game->Shutdown(m_Registry);
    }

	void World::PreTick()
	{
		F_LOG_TRACE("World PreTick!");
	}

    void World::Update(float t_DeltaTime)
    {
		m_ShouldQuit = (m_ShouldQuit ? m_ShouldQuit : Input::IsKeyDown(KeyNames::FL_KEY_ESCAPE));
		// TODO: Update any _world_ systems 
			// The transforms of objects

		// If the key L is pressed, then do some loading! 
		// #TODO: Repleace is key poll with a delegate 
		if(Input::IsKeyDown(KeyNames::FL_KEY_L))
		{
			std::string LevelToLoad = FlingConfig::GetString("Game", "StartLevel");
			
			LoadLevel(LevelToLoad);
		}
		// Save the world
		else if(Input::IsKeyDown(KeyNames::FL_KEY_K))
		{
			WriteLevel();
		}

		// Once we are done with core updates, then call the game!
		m_Game->Update(m_Registry, t_DeltaTime);
    }

	void World::WriteLevel()
	{
		F_LOG_TRACE("Write the world to: {}", m_CurrentLevelFile);

		if (!m_CurrentLevelFile)
		{
			F_LOG_ERROR("Uh oh");
			return;
		}

		entt::entity e0 = m_Registry.create();
		m_Registry.assign<Fling::Transform>(e0);
		m_Registry.assign<NamedEntity>(e0, "Entity 0 Name");

		entt::entity e1 = m_Registry.create();
		m_Registry.assign<Fling::Transform>(e1);
		m_Registry.assign<NamedEntity>(e1, "Entity 1 Name");

		m_Registry.view<NamedEntity, Fling::Transform>().each([&](NamedEntity& t_Name, Fling::Transform& t_Transform)
		{
			F_LOG_TRACE("Entity name: {}", t_Name);
   		});

		m_Registry.reset();
	}

	// #TODO: Add a callback func for when the level loading is complete
    void World::LoadLevel(const std::string& t_LevelPath)
    {
		std::string FullPath = FlingPaths::EngineAssetsDir() + "/" + t_LevelPath;

		F_LOG_TRACE("World loading level: {}", FullPath);

		m_CurrentLevelFile = JsonFile::Create(entt::hashed_string{ FullPath.c_str() });

		// #TODO: Unload the current level? Depends on how we want to do async loading in the future
		m_Game->Read(m_Registry);
    }
} // namespace Fling