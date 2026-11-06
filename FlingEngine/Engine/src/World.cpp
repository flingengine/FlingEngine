#include "Components/Name.hpp"
#include "ComponentTypeRegistry.h"
#include "FlingConfig.h"
#include "FlingPaths.h"
#include "Json.h"
#include "Logger.h"
#include "World.h"

#include <algorithm>
#include <chrono>
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

	World::~World() = default;

	const Schedule* World::GetSchedule() const
	{
		return m_Runner ? &m_Runner->GetSchedule() : nullptr;
	}

	bool World::Init(const std::string& t_StartLevel)
	{
		F_LOG_TRACE("World Init!");
		assert(m_CurrentState == WorldState::NONE);

		m_Game->RegisterComponents();
		ComponentTypeRegistry::Get().RunExternalRegistrars();
		m_Game->RegisterSystems(m_Systems);
		m_Game->Init(m_Registry);
		ComponentTypeRegistry::Get().Seal();

		// Build the schedule before any level loads. A bad schedule never runs.
		ScheduleResult Schedule = m_Systems.BuildSchedule();
		if (!Schedule.Ok())
		{
			m_ScheduleErrors = Schedule.ErrorReport();
			for (const ScheduleError& Error : Schedule.Errors)
			{
				F_LOG_ERROR("System schedule: {}", Error.Message);
			}
			return false;
		}
		F_LOG_TRACE("System schedule: {} systems", m_Systems.Size());

		// Each system makes its singletons before the first level loads
		m_Runner = std::make_unique<SystemRunner>(m_Systems, std::move(Schedule.Value), m_Registry, m_RunnerConfig);
		m_Runner->Create();

		const std::string LevelToLoad = t_StartLevel.empty() ? FlingConfig::GetString("Game", "StartLevel") : t_StartLevel;
		if (!LevelToLoad.empty())
		{
			LoadLevelFile(LevelToLoad);
		}

		m_CurrentState = WorldState::Initalized;
		return true;
	}

	void World::Shutdown()
	{
        // A request that has not run yet does not matter now
        m_PendingRequest = PlayRequest::None;

        // The application exits, so there is no reason to restore the snapshot
        if (m_CurrentState == WorldState::Playing || m_CurrentState == WorldState::Paused)
        {
            m_CurrentState = WorldState::Quitting;
            StopGame(false);
        }

        m_CurrentState = WorldState::ShuttingDown;

		// Systems remove their singletons in reverse schedule order, then the game shuts down
		if (m_Runner)
		{
			m_Runner->Destroy();
			m_Runner.reset();
		}
		m_Game->Shutdown(m_Registry);

		F_LOG_TRACE("World shutdown complete!");
	}

	void World::RequestGameStart()
	{
        if (m_CurrentState != WorldState::Initalized)
        {
            F_LOG_WARN("RequestGameStart ignored: the world is not ready for play");
            return;
        }

        m_PendingRequest = PlayRequest::Start;
    }

    void World::RequestGameStop()
    {
        const bool bStartPending = (m_PendingRequest == PlayRequest::Start);
        if (m_CurrentState != WorldState::Playing && m_CurrentState != WorldState::Paused && !bStartPending)
        {
            F_LOG_WARN("RequestGameStop ignored: the world is not playing");
            return;
        }

        // Stop before the start ran: cancel the start, and nothing changed
        m_PendingRequest = bStartPending ? PlayRequest::None : PlayRequest::Stop;
    }

    void World::ProcessPlayRequest()
    {
        const PlayRequest Request = m_PendingRequest;
        m_PendingRequest = PlayRequest::None;

        if (Request == PlayRequest::Start && m_CurrentState == WorldState::Initalized)
        {
            StartGame();
        }
        else if (Request == PlayRequest::Stop &&
                 (m_CurrentState == WorldState::Playing || m_CurrentState == WorldState::Paused))
        {
            StopGame(true);
        }
    }

    void World::StartGame()
    {
        F_LOG_TRACE("World: start play");

        // Keep the entities as they are now (with unsaved editor changes), so that
        // stop can put them back
        m_PlaySnapshot = Json::Object();
        m_HasPlaySnapshot = SaveToJson(m_PlaySnapshot, "PlaySnapshot");
        if (!m_HasPlaySnapshot)
        {
            F_LOG_ERROR("World: cannot make a play snapshot. Stop will not reset the world.");
        }

        m_CurrentState = WorldState::Starting;
        if (m_Runner)
        {
            m_Runner->PlayBegin(WorldRole::Play);
        }
        m_Game->OnStartGame(m_Registry);
		m_CurrentState = WorldState::Playing;
    }

    void World::StopGame(bool t_RestoreSnapshot)
    {
        F_LOG_TRACE("World: stop play");

        // Systems and the game clean up their own state while the entities still exist
        if (m_Runner)
        {
            m_Runner->PlayEnd(WorldRole::Play);
        }
        m_Game->OnStopGame(m_Registry);
        m_Game->m_WantsToQuit = false;

        if (t_RestoreSnapshot && m_HasPlaySnapshot)
        {
            // Removes every entity that play made or changed, then makes the
            // entities from the start of play again
            LoadFromJson(m_PlaySnapshot, "PlaySnapshot");
        }

        m_PlaySnapshot = Json();
        m_HasPlaySnapshot = false;

        if (m_CurrentState != WorldState::Quitting)
        {
            m_CurrentState = WorldState::Initalized;
        }
    }

	void World::Update(float t_DeltaTime)
	{
        // Start and stop requests come from anywhere (the editor UI draws during
        // rendering), so they run here, before the game ticks
        ProcessPlayRequest();

        const bool bPlaying = m_CurrentState == WorldState::Playing;
        const WorldRole Role = bPlaying ? WorldRole::Play : WorldRole::Edit;
        const auto RealDelta = std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::duration<double>(static_cast<double>(t_DeltaTime)));

        // The fixed ticks that are due, then the legacy game update, then the frame systems
        if (m_Runner)
        {
            m_Runner->RunFixed(RealDelta, Role);
        }

        if(bPlaying)
		{
			m_Game->Update(m_Registry, t_DeltaTime);

            // In editor play, a game that wants to quit stops play instead
            if (m_GameQuitStopsPlay && m_Game->WantsToQuit())
            {
                m_Game->m_WantsToQuit = false;
                m_PendingRequest = PlayRequest::Stop;
            }
        }

        if (m_Runner)
        {
            m_Runner->RunFrame(RealDelta, Role);
        }
	}

	bool World::OutputLevelFile(const std::string& t_LevelToLoad)
	{
        Json root = Json::Object();
        if (!SaveToJson(root, t_LevelToLoad))
        {
            F_LOG_ERROR("OutputLevelFile('{}') failed", t_LevelToLoad);
            return false;
		}

        const std::string FullPath = FlingPaths::EngineAssetsDir() + "/" + t_LevelToLoad;
        F_LOG_TRACE("Outputting Level file to {}", FullPath);
        return root.SaveToFile(FullPath);
    }

    bool World::SaveToJson(Json& t_OutDoc, const std::string& t_Title) const
    {
        const ComponentTypeRegistry& typeRegistry = ComponentTypeRegistry::Get();
        if (!typeRegistry.IsSealed())
        {
            F_LOG_ERROR("Cannot save the world: ComponentTypeRegistry is not sealed");
            return false;
        }

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

        t_OutDoc = Json::Object();
        t_OutDoc.Set("version", 1);
        t_OutDoc.Set("title", t_Title);
        t_OutDoc.Set("entities", entities);
        return true;
    }

    bool World::LoadLevelFile(const std::string& t_LevelToLoad)
	{
        if (!ComponentTypeRegistry::Get().IsSealed())
        {
			F_LOG_ERROR("LoadLevelFile('{}') failed: ComponentTypeRegistry is not sealed", t_LevelToLoad);
			return false;
		}

        const std::string FullPath = FlingPaths::EngineAssetsDir() + "/" + t_LevelToLoad;
        F_LOG_TRACE("Loading Level file from {}", FullPath);

		Json doc;
		if (!doc.LoadFromFile(FullPath))
		{
			return false;
		}

        return LoadFromJson(doc, FullPath);
    }

    bool World::LoadFromJson(const Json& t_Doc, const std::string& t_SourceName)
    {
        const ComponentTypeRegistry& typeRegistry = ComponentTypeRegistry::Get();
        if (!typeRegistry.IsSealed())
        {
            F_LOG_ERROR("Cannot load '{}': ComponentTypeRegistry is not sealed", t_SourceName);
            return false;
        }

        Json entities = t_Doc.Get("entities");
        if (!entities.IsArray())
		{
            F_LOG_ERROR("Level '{}' is missing an entities array", t_SourceName);
            return false;
		}

        // Fires the destroy signals, so the renderer lets go of the old entities
        m_Registry.reset();

        const std::string title = t_Doc.GetString("title", t_SourceName);
        F_LOG_TRACE("Level Title: {}", title);

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

			auto LoadNamedComponent = [&](const std::string& key)
			{
				if (IsReservedEntityKey(key) || !entityJson.Contains(key))
				{
					return;
				}

				const ComponentTypeInfo* info = typeRegistry.Find(key);
				if (!info || !info->load)
				{
					F_LOG_WARN("Unknown component '{}' on entity '{}', skipping", key, name);
					return;
				}

				info->load(m_Registry, entity, entityJson.Get(key));
			};

			// nlohmann objects iterate keys alphabetically, so MeshRenderer would
			// otherwise construct before Transform. Load pose first so renderer
			// hooks and later components see the authored transform.
			LoadNamedComponent("Transform");
			for (const std::string& key : entityJson.Keys())
			{
				if (key == "Transform")
				{
					continue;
				}
				LoadNamedComponent(key);
			}
		}

		return true;
    }
} // namespace Fling
