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
		
		//LoadLevel(LevelToLoad);

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
		if (!m_CurrentLevelFile)
		{
			F_LOG_ERROR("Uh oh");
			return;
		}

		// For testing -------------------
		entt::entity e0 = m_Registry.create();
		m_Registry.assign<Fling::Transform>(e0);
		m_Registry.assign<NamedEntity>(e0, "Entity 0 Name");

		entt::entity e1 = m_Registry.create();
		m_Registry.assign<Fling::Transform>(e1);
		m_Registry.assign<NamedEntity>(e1, "Entity 1 Name");
		// end For testing -------------------

		nlohmann::json& JsonData = m_CurrentLevelFile->GetJsonData();

		nlohmann::json EntityJsonArray = nlohmann::json::array();

		JsonData["entities"] = EntityJsonArray;

		m_Registry.view<NamedEntity, Fling::Transform>().each([&EntityJsonArray](NamedEntity& t_Name, Fling::Transform& t_Transform)
		{
			nlohmann::json EntityObject = nlohmann::json::object();

			EntityObject["name"] = t_Name.Name;

			EntityObject["transform"]["pos"]["x"] = t_Transform.Pos.x;
			EntityObject["transform"]["pos"]["y"] = t_Transform.Pos.y;
			EntityObject["transform"]["pos"]["z"] = t_Transform.Pos.z;

			EntityObject["transform"]["scale"]["x"] = t_Transform.Scale.x;
			EntityObject["transform"]["scale"]["y"] = t_Transform.Scale.y;
			EntityObject["transform"]["scale"]["z"] = t_Transform.Scale.z;

			EntityJsonArray.push_back(EntityObject);
   		});

		m_CurrentLevelFile->Write();

		m_Registry.reset();
	}

    void World::LoadLevel(const std::string& t_LevelPath)
    {
		F_LOG_TRACE("World Load Level {}", t_LevelPath);
		m_CurrentLevelFile = JsonFile::Create(entt::hashed_string{ t_LevelPath.c_str() });

		// #TODO: Unload the current level? Depends on how we want to do async loading in the future
		m_Game->Read(m_Registry);
    }
} // namespace Fling