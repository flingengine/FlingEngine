#include "pch.h"
#include "World.h"
#include "ComponentTypeRegistry.h"
#include "Components/Name.hpp"
#include "FlingConfig.h"
#include "FlingPaths.h"
#include "Json.h"

#include <algorithm>
#include <utility>
#include <vector>

namespace Fling
{
	namespace
	{
		bool IsReservedEntityKey(const std::string& key)
		{
			return key == "name" || key == "title" || key == "version" || key == "NameComponent";
		}
	}

	World::World(entt::registry& t_Reg, Fling::Game* t_Game)
		: m_Registry(t_Reg)
		, m_Game(t_Game)
		, m_ShouldQuit(false)
	{ }

	void World::Init()
	{
		F_LOG_TRACE("World Init!");
		assert(m_CurrentState == WorldState::NONE);

		m_Game->RegisterComponents();
		ComponentTypeRegistry::Get().RunExternalRegistrars();
		m_Game->Init(m_Registry);
		ComponentTypeRegistry::Get().Seal();

		const std::string LevelToLoad = FlingConfig::GetString("Game", "StartLevel");
		if (!LevelToLoad.empty())
		{
			LoadLevelFile(LevelToLoad);
		}

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

			// #TODO Update physics here
		}
	}

	bool World::OutputLevelFile(const std::string& t_LevelToLoad)
	{
		ComponentTypeRegistry& typeRegistry = ComponentTypeRegistry::Get();
		if (!typeRegistry.IsSealed())
		{
			F_LOG_ERROR("OutputLevelFile('{}') failed: ComponentTypeRegistry is not sealed", t_LevelToLoad);
			return false;
		}

		std::string FullPath = FlingPaths::EngineAssetsDir() + "/" + t_LevelToLoad;

		std::vector<std::pair<std::string, Json>> entityEntries;
		std::size_t unnamedIndex = 0;

		m_Registry.each([&](const entt::entity entity)
		{
			Json entityJson = Json::Object();

			std::string name;
			if (m_Registry.has<NameComponent>(entity))
			{
				name = m_Registry.get<NameComponent>(entity).Name;
			}
			if (name.empty())
			{
				name = "Entity_" + std::to_string(unnamedIndex);
			}
			++unnamedIndex;

			entityJson.Set("name", name);

			for (const ComponentTypeInfo& type : typeRegistry.All())
			{
				if (!type.name || IsReservedEntityKey(type.name))
				{
					continue;
				}

				if (type.has && type.has(m_Registry, entity) && type.save)
				{
					Json componentJson = Json::Object();
					type.save(m_Registry, entity, componentJson);
					entityJson.Set(type.name, componentJson);
				}
			}

			entityEntries.emplace_back(name, std::move(entityJson));
		});

		std::sort(entityEntries.begin(), entityEntries.end(),
			[](const std::pair<std::string, Json>& a, const std::pair<std::string, Json>& b)
			{
				return a.first < b.first;
			});

		Json entities = Json::Array();
		for (auto& entry : entityEntries)
		{
			entities.PushBack(entry.second);
		}

		Json root = Json::Object();
		root.Set("version", 1);
		root.Set("title", t_LevelToLoad);
		root.Set("entities", entities);

		F_LOG_TRACE("Outputting Level file to {}", FullPath);
		if (!root.SaveToFile(FullPath))
		{
			return false;
		}

		return true;
	}

	bool World::LoadLevelFile(const std::string& t_LevelToLoad)
	{
		ComponentTypeRegistry& typeRegistry = ComponentTypeRegistry::Get();
		if (!typeRegistry.IsSealed())
		{
			F_LOG_ERROR("LoadLevelFile('{}') failed: ComponentTypeRegistry is not sealed", t_LevelToLoad);
			return false;
		}

		std::string FullPath = FlingPaths::EngineAssetsDir() + "/" + t_LevelToLoad;
		F_LOG_TRACE("Loading Level file from {}", FullPath);

		Json doc;
		if (!doc.LoadFromFile(FullPath))
		{
			return false;
		}

		m_Registry.reset();

		const std::string title = doc.GetString("title", t_LevelToLoad);
		F_LOG_TRACE("Level Title: {}", title);

		Json entities = doc.Get("entities");
		if (!entities.IsArray())
		{
			F_LOG_ERROR("Level file '{}' is missing an entities array", FullPath);
			return false;
		}

		for (std::size_t i = 0; i < entities.Size(); ++i)
		{
			Json entityJson = entities.At(i);
			if (!entityJson.IsObject())
			{
				F_LOG_WARN("Skipping non-object entity at index {}", i);
				continue;
			}

			entt::entity entity = m_Registry.create();
			std::string name = entityJson.GetString("name");
			if (name.empty())
			{
				name = "Entity_" + std::to_string(i);
			}

			NameComponent& nameComp = m_Registry.assign<NameComponent>(entity);
			nameComp.Name = name;

			for (const std::string& key : entityJson.Keys())
			{
				if (IsReservedEntityKey(key))
				{
					continue;
				}

				const ComponentTypeInfo* info = typeRegistry.Find(key);
				if (!info || !info->load)
				{
					F_LOG_WARN("Unknown component '{}' on entity '{}', skipping", key, name);
					continue;
				}

				info->load(m_Registry, entity, entityJson.Get(key));
			}
		}

		return true;
	}
} // namespace Fling
