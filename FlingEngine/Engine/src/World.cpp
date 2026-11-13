#include "Components/EntityGuid.h"
#include "Components/Name.hpp"
#include "FlingConfig.h"
#include "FlingPaths.h"
#include "HierarchyUtil.h"
#include "Json.h"
#include "Logger.h"
#include "Strings/String.h"
#include "World.h"

#include <algorithm>
#include <chrono>
#include <random>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace Fling
{
    namespace
    {
        bool IsReservedEntityKey(const StringView key)
        {
            return key == "name" || key == "title" || key == "version" || key == "NameComponent";
        }

        /** True if the modules already have a type with this name. */
        bool HasType(const ModuleRegistration& t_Modules, const char* t_Name)
        {
            for (const TypeDesc& Type : t_Modules.Types())
            {
                if (Type.Name == t_Name)
                {
                    return true;
                }
            }
            return false;
        }
    } // namespace

    World::World(std::shared_ptr<Fling::Game> t_Game) : m_Game(std::move(t_Game)), m_ShouldQuit(false)
    {
    }

    World::~World() = default;

    const Schedule* World::GetSchedule() const
    {
        return m_Runner ? &m_Runner->GetSchedule() : nullptr;
    }

    bool World::Init(const StringView t_StartLevel)
    {
        F_LOG_TRACE("World Init!");
        assert(m_CurrentState == WorldState::NONE);

        m_Game->Register(m_Modules);

        // The level format always has the entity name and id, so every world knows these two
        if (!HasType(m_Modules, NameComponent::Component.Name))
        {
            m_Modules.Component<NameComponent>([](TypeBuilder<NameComponent>& t_Type)
                                               { t_Type.Field("Name", &NameComponent::Name); });
        }
        if (!HasType(m_Modules, EntityGuid::Component.Name))
        {
            m_Modules.Component<EntityGuid>([](TypeBuilder<EntityGuid>& t_Type)
                                            { t_Type.Field("value", &EntityGuid::Value).ReadOnly(); });
        }
        m_Types = TypeRegistry::Build(m_Modules);
        m_TypesBuilt = true;

        m_Game->Init(m_Storage);

        // Build the schedule before any level loads. A bad schedule never runs.
        // Component names that cannot be told apart stop the world too: levels would load the wrong type
        ScheduleResult Schedule = m_Systems.BuildSchedule();
        if (!Schedule.Ok() || !m_Types.Ok())
        {
            m_ScheduleErrors = Schedule.Ok() ? String() : Schedule.ErrorReport();
            for (const ScheduleError& Error : Schedule.Errors)
            {
                F_LOG_ERROR("System schedule: {}", Error.Message);
            }
            for (const String& Error : m_Types.Errors())
            {
                F_LOG_ERROR("Component types: {}", Error);
                m_ScheduleErrors += "Component types: " + Error + "\n";
            }
            return false;
        }
        F_LOG_TRACE("System schedule: {} systems", m_Systems.Size());

        // A component that a system uses and that is saved (not Transient) must be
        // registered, or levels and the play snapshot lose it
        std::set<String> Reported;
        for (const SystemDesc* Desc : m_Systems.Descs())
        {
            for (const AccessEntry& Entry : Desc->Access)
            {
                if (!Entry.Transient && Entry.Name && !m_Types.Find(Entry.Name) && Reported.insert(Entry.Name).second)
                {
                    F_LOG_WARN("System {} uses component {}, which is saved but not registered. Add it with "
                               "ModuleRegistration::Component<T>(), or mark it Transient.",
                               Desc->Info.Name,
                               Entry.Name);
                }
            }
        }

        // Systems append requests for the application. The world takes them each frame.
        m_Storage.SetSingleton<AppRequests>();

        // Each system makes its singletons before the first level loads
        m_Runner = std::make_unique<SystemRunner>(m_Systems, std::move(Schedule.Value), m_Storage, m_RunnerConfig);
        m_Runner->Create();

        const String LevelToLoad =
            t_StartLevel.empty() ? FlingConfig::GetString("Game", "StartLevel") : String(t_StartLevel);
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
        m_Game->Shutdown(m_Storage);

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
        m_Game->OnStartGame(m_Storage);
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
        m_Game->OnStopGame(m_Storage);
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

        // The fixed ticks that are due, then the legacy game update, then the variable rate systems
        if (m_Runner)
        {
            m_Runner->RunFixed(RealDelta, Role);
            ProcessAppRequests();
        }

        if (bPlaying)
        {
            m_Game->Update(m_Storage, t_DeltaTime);

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

    void World::ProcessAppRequests()
    {
        AppRequests* Queue = m_Storage.TryGetSingleton<AppRequests>();
        if (!Queue)
        {
            return;
        }

        for (const AppRequest Request : std::exchange(Queue->Requests, {}))
        {
            if (Request != AppRequest::Quit)
            {
                m_AppRequestsForApp.push_back(Request);
            }
            else if (m_GameQuitStopsPlay)
            {
                // Editor play: stop play, and keep the editor open
                if (m_CurrentState == WorldState::Playing || m_CurrentState == WorldState::Paused)
                {
                    m_PendingRequest = PlayRequest::Stop;
                }
            }
            else
            {
                m_ShouldQuit = true;
            }
        }
    }

    bool World::OutputLevelFile(const StringView t_LevelToLoad)
    {
        Json root = Json::Object();
        if (!SaveToJson(root, t_LevelToLoad))
        {
            F_LOG_ERROR("OutputLevelFile('{}') failed", t_LevelToLoad);
            return false;
        }

        const String FullPath = FlingPaths::EngineAssetsDir() + "/" + String(t_LevelToLoad);
        F_LOG_TRACE("Outputting Level file to {}", FullPath);
        return root.SaveToFile(FullPath);
    }

    namespace
    {
        /** A new random EntityGuid value that is not in t_Used. Adds it to t_Used. */
        uint64 MakeEntityGuid(std::unordered_set<uint64>& t_Used)
        {
            // Only level files use the id, so it does not need to be deterministic
            static std::mt19937_64 Generator{std::random_device{}()};
            while (true)
            {
                const uint64 Value = Generator();
                if (Value != 0 && t_Used.insert(Value).second)
                {
                    return Value;
                }
            }
        }
    } // namespace

    bool World::SaveToJson(Json& t_OutDoc, const StringView t_Title)
    {
        if (!m_TypesBuilt)
        {
            F_LOG_ERROR("Cannot save the world before World::Init");
            return false;
        }

        // Every entity needs an id, so that Entity fields in other components can name it.
        // Two entities that share an id (a copy) get a new one.
        std::unordered_set<uint64> UsedGuids;
        std::vector<Fling::Entity> NeedGuid;
        m_Storage.EachEntity(
            [&](const Fling::Entity t_Entity)
            {
                const EntityGuid* Guid = m_Storage.TryGet<EntityGuid>(t_Entity);
                if (!Guid || Guid->Value == 0 || !UsedGuids.insert(Guid->Value).second)
                {
                    NeedGuid.push_back(t_Entity);
                }
            });
        for (const Fling::Entity Entity : NeedGuid)
        {
            m_Storage.AddOrReplace<EntityGuid>(Entity, EntityGuid{MakeEntityGuid(UsedGuids)});
        }

        const SaveContext Context{&m_Storage};
        std::vector<std::pair<String, Json>> entityEntries;
        std::size_t unnamedIndex = 0;

        m_Storage.EachEntity(
            [&](const Fling::Entity entity)
            {
                Json entityJson = Json::Object();

                String name;
                if (m_Storage.Has<NameComponent>(entity))
                {
                    name = String(m_Storage.Get<NameComponent>(entity).Name.ToView());
                }
                if (name.empty())
                {
                    name = "Entity_" + std::to_string(unnamedIndex);
                }
                ++unnamedIndex;

                entityJson.Set("name", name);

                for (const TypeDesc& type : m_Types.All())
                {
                    if (!type.IsSaved() || type.IsSingleton() || IsReservedEntityKey(type.Name))
                    {
                        continue;
                    }

                    if (type.Has && type.Has(m_Storage, entity))
                    {
                        Json componentJson = Json::Object();
                        if (type.GetConst)
                        {
                            SaveObject(type, type.GetConst(m_Storage, entity), componentJson, Context);
                        }
                        entityJson.Set(type.Name, componentJson);
                    }
                }

                entityEntries.emplace_back(name, std::move(entityJson));
            });

        std::stable_sort(entityEntries.begin(),
                         entityEntries.end(),
                         [](const std::pair<String, Json>& a, const std::pair<String, Json>& b)
                         { return a.first < b.first; });

        Json entities = Json::Array();
        for (auto& entry : entityEntries)
        {
            entities.PushBack(entry.second);
        }

        t_OutDoc = Json::Object();
        t_OutDoc.Set("version", 1);
        t_OutDoc.Set("title", t_Title);
        t_OutDoc.Set("entities", entities);

        // Singletons that are saved (not Transient). The key is written only when
        // there is one, so a level with none saves the same as before.
        Json singletons = Json::Object();
        bool bHasSingletons = false;
        for (const TypeDesc& type : m_Types.All())
        {
            if (!type.IsSingleton() || !type.IsSaved() || !type.GetSingletonConst)
            {
                continue;
            }
            if (const void* Object = type.GetSingletonConst(m_Storage))
            {
                Json singletonJson = Json::Object();
                SaveObject(type, Object, singletonJson, Context);
                singletons.Set(type.Name, singletonJson);
                bHasSingletons = true;
            }
        }
        if (bHasSingletons)
        {
            t_OutDoc.Set("singletons", singletons);
        }
        return true;
    }

    bool World::LoadLevelFile(const StringView t_LevelToLoad)
    {
        if (!m_TypesBuilt)
        {
            F_LOG_ERROR("LoadLevelFile('{}') failed: call World::Init first", t_LevelToLoad);
            return false;
        }

        const String FullPath = FlingPaths::EngineAssetsDir() + "/" + String(t_LevelToLoad);
        F_LOG_TRACE("Loading Level file from {}", FullPath);

        Json doc;
        if (!doc.LoadFromFile(FullPath))
        {
            return false;
        }

        return LoadFromJson(doc, FullPath);
    }

    bool World::LoadFromJson(const Json& t_Doc, const StringView t_SourceName)
    {
        if (!m_TypesBuilt)
        {
            F_LOG_ERROR("Cannot load '{}': call World::Init first", t_SourceName);
            return false;
        }

        Json entities = t_Doc.Get("entities");
        if (!entities.IsArray())
        {
            F_LOG_ERROR("Level '{}' is missing an entities array", t_SourceName);
            return false;
        }

        // Fires the destroy signals, so the renderer lets go of the old entities
        m_Storage.Clear();

        const String title = t_Doc.GetString("title", t_SourceName);
        F_LOG_TRACE("Level Title: {}", title);

        // Pass 1: make every entity and read its id, so that an Entity field can
        // refer to an entity that comes later in the file
        struct PendingEntity
        {
            Fling::Entity Entity;
            Json Data;
            String Name;
        };
        std::vector<PendingEntity> Pending;
        std::unordered_map<uint64, Fling::Entity> GuidToEntity;
        Pending.reserve(entities.Size());

        for (std::size_t i = 0; i < entities.Size(); ++i)
        {
            Json entityJson = entities.At(i);
            if (!entityJson.IsObject())
            {
                F_LOG_WARN("Skipping non-object entity at index {}", i);
                continue;
            }

            const Fling::Entity entity = m_Storage.Create();
            String name = entityJson.GetString("name");
            if (name.empty())
            {
                name = "Entity_" + std::to_string(i);
            }

            const uint64 Guid = entityJson.Get(EntityGuid::Component.Name).GetUInt64("value", 0);
            if (Guid != 0 && !GuidToEntity.emplace(Guid, entity).second)
            {
                F_LOG_WARN("{}: entity '{}' has the same EntityGuid as another entity. References use the first one.",
                           t_SourceName,
                           name);
            }

            Pending.push_back(PendingEntity{entity, std::move(entityJson), std::move(name)});
        }

        // Pass 2: the components
        const LoadContext Context{&GuidToEntity, String(t_SourceName)};
        for (PendingEntity& Each : Pending)
        {
            NameComponent& nameComp = m_Storage.Add<NameComponent>(Each.Entity);
            nameComp.Name = Name(Each.Name);

            // What the file has for this entity: the types to load, and the keys that are not types
            struct SavedComponent
            {
                const TypeDesc* Type = nullptr;
                String Key;
            };
            std::vector<SavedComponent> Saved;
            for (const String& key : Each.Data.Keys())
            {
                if (IsReservedEntityKey(key))
                {
                    continue;
                }
                const TypeDesc* type = m_Types.Find(key);
                if (!type || type->IsSingleton())
                {
                    if (const RemovedComponentDesc* removed = m_Types.FindRemoved(key))
                    {
                        if (removed->Convert)
                        {
                            removed->Convert(m_Storage, Each.Entity, Each.Data.Get(key));
                        }
                        continue;
                    }
                    F_LOG_WARN("Unknown component '{}' on entity '{}', skipping", key, Each.Name);
                    continue;
                }
                if (type->IsSaved() && type->Emplace)
                {
                    Saved.push_back(SavedComponent{type, key});
                }
            }

            const auto IsSaved = [&Saved](const TypeDesc* t_Type)
            {
                return std::any_of(
                    Saved.begin(), Saved.end(), [&](const SavedComponent& t_Each) { return t_Each.Type == t_Type; });
            };
            const auto Load = [&](const SavedComponent& t_Saved)
            {
                if (t_Saved.Type->Has(m_Storage, Each.Entity))
                {
                    return;
                }
                const Json ComponentJson = Each.Data.Get(t_Saved.Key);
                const std::function<void(void*)> Fill = [&](void* t_Object)
                {
                    LoadObject(*t_Saved.Type, t_Object, ComponentJson, Context);
                };
                t_Saved.Type->Emplace(m_Storage, Each.Entity, t_Saved.Type->IsTag() ? nullptr : &Fill);
            };

            // A component loads after the components that it requires (a MeshRenderer after its
            // Transform), so its hooks see them with their saved values. One that the file does not
            // have is added with default values.
            for (const SavedComponent& Component : Saved)
            {
                for (const TypeDesc* Needed : m_Types.WithRequired(*Component.Type))
                {
                    const auto Found =
                        std::find_if(Saved.begin(),
                                     Saved.end(),
                                     [&](const SavedComponent& t_Each) { return t_Each.Type == Needed; });
                    if (Found != Saved.end())
                    {
                        Load(*Found);
                    }
                    else if (!Needed->Has(m_Storage, Each.Entity))
                    {
                        F_LOG_WARN("{}: entity '{}' has a '{}' and no '{}', which it requires. A default '{}' was "
                                   "added.",
                                   t_SourceName,
                                   Each.Name,
                                   Component.Type->Name,
                                   Needed->Name,
                                   Needed->Name);
                        Needed->Emplace(m_Storage, Each.Entity, nullptr);
                    }
                }
            }
        }

        // Children is not saved: make it from the Parent links
        HierarchyUtil::RebuildChildren(m_Storage);

        // The optional "singletons" object. Levels from before singletons have no such key.
        const Json singletons = t_Doc.Get("singletons");
        if (singletons.IsObject())
        {
            for (const String& key : singletons.Keys())
            {
                const TypeDesc* type = m_Types.Find(key);
                if (!type || !type->IsSingleton() || !type->EmplaceSingleton)
                {
                    if (!m_Types.FindRemoved(key))
                    {
                        F_LOG_WARN("Unknown singleton '{}' in level '{}', skipping", key, t_SourceName);
                    }
                    continue;
                }
                if (!type->IsSaved())
                {
                    continue;
                }
                const Json SingletonJson = singletons.Get(key);
                const std::function<void(void*)> Fill = [&](void* t_Object)
                {
                    LoadObject(*type, t_Object, SingletonJson, Context);
                };
                type->EmplaceSingleton(m_Storage, &Fill);
            }
        }

        return true;
    }
} // namespace Fling
