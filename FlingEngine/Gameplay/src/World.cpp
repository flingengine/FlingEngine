#include "pch.h"
#include "World.h"
#include "FlingConfig.h"
#include <cereal/archives/json.hpp>
#include <fstream>

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
		std::ofstream OutStream(m_CurrentLevelFile);
		cereal::JSONOutputArchive Archive(OutStream);

		entt::entity e0 = m_Registry.create();
		m_Registry.assign<Fling::Transform>(e0);

		entt::entity e1 = m_Registry.create();
		m_Registry.assign<Fling::Transform>(e1);

		m_Registry.view<Fling::Transform>().each([&Archive](Fling::Transform& t_Transform) 
		{
			// gets all the components of the view at once ...
			Archive(
				CEREAL_NVP(t_Transform.Pos.x),
				CEREAL_NVP(t_Transform.Pos.y),
				CEREAL_NVP(t_Transform.Pos.z),
				CEREAL_NVP(t_Transform.Scale.x),
				CEREAL_NVP(t_Transform.Scale.y),
				CEREAL_NVP(t_Transform.Scale.z)
			);
   		});

		m_Registry.reset();
	}

	// #TODO: Add a callback func for when the level loading is complete
    void World::LoadLevel(const std::string& t_LevelPath)
    {
		std::string FullPath = FlingPaths::EngineAssetsDir() + "/" + t_LevelPath;

		F_LOG_TRACE("World loading level: {}", FullPath);

		m_CurrentLevelFile = FullPath;

		// #TODO: Unload the current level? Depends on how we want to do async loading in the future

		m_Game->Read(m_Registry);
    }
} // namespace Fling