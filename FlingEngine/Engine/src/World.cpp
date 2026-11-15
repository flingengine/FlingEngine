#include "Components/EntityGuid.h"
#include "Components/Name.hpp"
#include "Ecs/Schedule.h"
#include "Ecs/SimTime.h"
#include "Ecs/StateHash.h"
#include "FlingConfig.h"
#include "FlingPaths.h"
#include "HierarchyUtil.h"
#include "Input/Input.h"
#include "Json.h"
#include "Logger.h"
#include "MathUtils.h"
#include "PlatformTime.h"
#include "PresentationLink.h"
#include "Strings/String.h"
#include "Threading/Lane.h"
#include "World.h"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstdio>
#include <limits>
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

        /** The id of the registered type with this saved-name hash, or InvalidComponentId. */
        Detail::ComponentId FindComponentId(const TypeHash t_Hash)
        {
            for (Detail::ComponentId Id = 0; Id < Detail::MaxComponentTypes; ++Id)
            {
                const Detail::ComponentOps* Ops = Detail::g_ComponentOps[Id].load(std::memory_order_acquire);
                if (Ops != nullptr && Ops->Key == t_Hash)
                {
                    return Id;
                }
            }
            return Detail::InvalidComponentId;
        }

        /** The type and, when it is registered, every type it requires. */
        std::vector<TypeHash> WithRequired(const TypeHash t_Hash, const TypeRegistry& t_Types)
        {
            std::vector<TypeHash> Hashes;
            if (const TypeDesc* Type = t_Types.Find(t_Hash))
            {
                for (const TypeDesc* Each : t_Types.WithRequired(*Type))
                {
                    Hashes.push_back(Each->Hash);
                }
            }
            else
            {
                Hashes.push_back(t_Hash);
            }
            return Hashes;
        }

        /**
         * The storage-level published set (Docs/threading-model.md, "The published set"), by
         * component id: each type of Schedule::Published with the types it requires. A type
         * with no id is in no storage and is left out. Call it after SystemRunner::Create,
         * which gives every type in the access lists its id. Returns one line per type that
         * cannot be copied.
         */
        String MakePublishedSet(const Schedule& t_Schedule, const TypeRegistry& t_Types, PublishedSet& t_Out)
        {
            String Errors;
            for (const PublishedType& Published : t_Schedule.Published)
            {
                for (const TypeHash Hash : WithRequired(Published.Hash, t_Types))
                {
                    const Detail::ComponentId Id = FindComponentId(Hash);
                    if (Id == Detail::InvalidComponentId)
                    {
                        continue;
                    }
                    const String Reader(t_Schedule.Systems[Published.Readers.front()]->Info.Name);
                    if (Detail::GetComponentOps(Id).Domain == ComponentDomain::Presentation)
                    {
                        // Nothing copies a Presentation type to the mirror, so the mirror entity would lack it
                        Errors += Reader + " reads " + Published.Name + " at the frame rate, and it requires " +
                                  Detail::GetComponentOps(Id).Name +
                                  ", a Presentation type, which is never published. Remove the requirement.\n";
                        continue;
                    }
                    String Error;
                    if (!t_Out.AddById(Id, &Error))
                    {
                        Errors += Reader + " reads " + Published.Name + " at the frame rate, and " + Error +
                                  ". Or read a copyable summary that a fixed system writes.\n";
                    }
                }
                // Only the type a frame system blends: not the types it requires
                const Detail::ComponentId Id = FindComponentId(Published.Hash);
                if (Published.IsBlended() && Id != Detail::InvalidComponentId)
                {
                    t_Out.KeepBlendedById(Id);
                }
            }
            return Errors;
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

    World::World(std::shared_ptr<Fling::Game> t_Game) : m_Game(std::move(t_Game))
    {
    }

    World::~World()
    {
        // The game thread uses this world: it must end first, and the storage is this thread's again
        m_GameLane.store(nullptr, std::memory_order_release);
        m_Driver.reset();
        m_Storage.SetOwnerLane(Lane::None);
    }

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

        // A component that a system uses must be registered when a rollback restores it (the
        // state hash reads the registered fields: an error), or when it is saved (levels and
        // the play snapshot lose it: a warning)
        std::set<String> Reported;
        String UnregisteredState;
        for (const SystemDesc* Desc : m_Systems.Descs())
        {
            for (const AccessEntry& Entry : Desc->Access)
            {
                if (!Entry.Name || m_Types.Find(Entry.Name) || !Reported.insert(Entry.Name).second)
                {
                    continue;
                }
                if (IsRollbackState(Entry.Domain, Entry.Kind, Entry.Rollback))
                {
                    UnregisteredState += fmt::format("Component types: system {} uses {}, which a rollback restores "
                                                     "(RollbackClass::Restore) and which is not registered, so the "
                                                     "state hash cannot see it. Add it with "
                                                     "ModuleRegistration::Component<T>(), or give it another "
                                                     "RollbackClass.\n",
                                                     Desc->Info.Name,
                                                     Entry.Name);
                }
                else if (!Entry.Transient)
                {
                    F_LOG_WARN("System {} uses component {}, which is saved but not registered. Add it with "
                               "ModuleRegistration::Component<T>(), or mark it Transient.",
                               Desc->Info.Name,
                               Entry.Name);
                }
            }
        }
        if (!UnregisteredState.empty())
        {
            F_LOG_ERROR("{}", UnregisteredState);
            m_ScheduleErrors = UnregisteredState;
            return false;
        }

        // Systems append requests for the application. The world takes them after each tick.
        m_Storage.SetSingleton<AppRequests>();

        // One clock for the ticks and the frames, in every exec mode
        if (!m_TickClock)
        {
            m_TickClock = [this]()
            {
                return m_SummedDeltaNs.load(std::memory_order_acquire);
            };
            m_TickTimelineStart = m_TickTimelineStart.value_or(0);
        }

        // Each system makes its singletons before the first level loads
        m_Runner = std::make_unique<SystemRunner>(m_Systems, std::move(Schedule.Value), m_Storage, m_RunnerConfig);
        m_SimHz.store(m_RunnerConfig.SimHz, std::memory_order_release);
        m_Runner->SetTickInputSource(m_TickInputSource);
        m_Runner->SetPendingInputSource(m_PendingInputSource);
        m_Runner->SetTickOutput([this](const uint64 t_Tick) { return OnTickDone(t_Tick); });
        // The ledger's state hash: the saved simulation components of this world's types
        if (m_StateHashEnabled)
        {
            m_Runner->SetStateHasher([this](const EntityStorage& t_Storage)
                                     { return HashSimulationState(t_Storage, m_Types); });
        }
        if (m_TickTimelineStart)
        {
            m_Runner->StartTimeline(*m_TickTimelineStart);
        }
        m_Runner->Create();

        // What Inline and Threaded play copy for the frame systems (after Create: the types have
        // ids). Checked in every exec mode, so a schedule that cannot be published fails in Direct too.
        m_Published = PublishedSet{};
        const String PublishErrors = MakePublishedSet(m_Runner->GetSchedule(), m_Types, m_Published);
        if (!PublishErrors.empty())
        {
            F_LOG_ERROR("Published set: {}", PublishErrors);
            m_ScheduleErrors = PublishErrors;
            return false;
        }
        F_LOG_TRACE("Exec mode {}: {} tables ({} blended), {} singletons published",
                    ExecModeName(m_ExecMode),
                    m_Published.Tables.size(),
                    m_Published.Blended.size(),
                    m_Published.Singletons.size());

        m_Blend.SetTypes(m_Published.Blended);

        // Edit: the frame side reads the simulation storage (no blend), and the ticks run on this thread
        UseLink(std::make_unique<DirectLink>(m_Storage, *m_Runner, std::vector<Detail::ComponentId>{}));
        UseEditDriver();

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
        if (m_CurrentState == WorldState::ShuttingDown)
        {
            return;
        }
        // A request that has not run yet does not matter now
        m_PendingRequest = PlayRequest::None;

        // The application exits, so there is no reason to restore the snapshot
        if (m_CurrentState == WorldState::Playing)
        {
            StopGame(RestoreSnapshot::No);
        }

        m_CurrentState = WorldState::ShuttingDown;

        // Systems remove their singletons in reverse schedule order (the frame storage still
        // exists), then the game shuts down
        m_Driver.reset();
        if (m_Runner)
        {
            m_Runner->Destroy();
            m_Runner.reset();
        }
        m_Link.reset();
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
        if (m_CurrentState != WorldState::Playing && !bStartPending)
        {
            F_LOG_WARN("RequestGameStop ignored: the world is not playing");
            return;
        }

        // Stop before the start ran: cancel the start, and nothing changed
        m_PendingRequest = bStartPending ? PlayRequest::None : PlayRequest::Stop;
    }

    void World::ProcessPlayRequest()
    {
        const PlayRequest Request = std::exchange(m_PendingRequest, PlayRequest::None);
        if (Request == PlayRequest::Start && m_CurrentState == WorldState::Initalized)
        {
            StartGame();
        }
        else if (Request == PlayRequest::Stop && m_CurrentState == WorldState::Playing)
        {
            StopGame(RestoreSnapshot::Yes);
        }
    }

    void World::StartGame()
    {
        F_LOG_TRACE("World: start play ({})", ExecModeName(m_ExecMode));

        // Keep the entities as they are now (with unsaved editor changes), so that
        // stop can put them back
        m_PlaySnapshot = Json::Object();
        if (!SaveToJson(m_PlaySnapshot, "PlaySnapshot"))
        {
            F_LOG_ERROR("World: cannot make a play snapshot. Stop will not reset the world.");
            m_PlaySnapshot = Json();
        }

        // One order in every exec mode: a new link, the simulation begins, then its first state
        // is shown, then the frame systems begin on that state
        m_CurrentState = WorldState::Starting;
        if (m_ExecMode == ExecMode::Direct)
        {
            UseLink(std::make_unique<DirectLink>(m_Storage, *m_Runner, m_Published.Blended));
        }
        else
        {
            UseLink(std::make_unique<MirrorLink>(m_Published));
        }
        m_Runner->PlayBegin(WorldRole::Play, UpdateRate::Fixed);
        m_Game->OnStartGame(m_Storage);
        m_Link->Publish(m_Storage, *m_Runner);
        ShowLatest();
        m_Runner->PlayBegin(WorldRole::Play, UpdateRate::Variable);

        // The ticks of the session, then the thread that runs them
        m_TickRole = WorldRole::Play;
        m_QuitTick = 0;
        m_RanLastTick.store(false, std::memory_order_release);
        m_Storage.SetOwnerLane(m_Link->SimulationOwner());
        m_CurrentState = WorldState::Playing;
        if (!StartPlayDriver())
        {
            F_LOG_ERROR("World: play cannot start in exec mode {}; it stops again", ExecModeName(m_ExecMode));
            StopGame(RestoreSnapshot::Yes);
        }
    }

    bool World::StartPlayDriver()
    {
        if (m_ExecMode != ExecMode::Threaded)
        {
            m_Driver = std::make_unique<SameThreadDriver>(AsTickHost(), m_Link->SimulationOwner());
            return true;
        }
        // Published before the thread starts: the game thread's input waits read it
        auto Lane = std::make_unique<GameLane>(AsTickHost(), m_GameLaneDesc);
        m_GameLane.store(Lane.get(), std::memory_order_release);
        m_Driver = std::move(Lane);
        F_LOG_TRACE("World: the game thread runs the ticks (--exec threaded)");
        return m_GameLane.load(std::memory_order_relaxed)->Start();
    }

    void World::UseEditDriver()
    {
        // The game thread stops after its wake and joins: the simulation storage is this thread's again
        m_GameLane.store(nullptr, std::memory_order_release);
        std::unique_ptr<TickDriver> Previous =
            std::exchange(m_Driver, std::make_unique<SameThreadDriver>(AsTickHost(), Lane::None));
        Previous.reset();
        m_Storage.SetOwnerLane(Lane::None);
    }

    void World::UseLink(std::unique_ptr<PresentationLink> t_Link)
    {
        m_Runner->SetFrameStorage(t_Link->FrameStorage());
        m_Blend.Reset();
        m_Link = std::move(t_Link);
    }

    TickStamp World::ShowLatest()
    {
        const ShownTicks Shown = m_Link->ShowLatest();
        m_Blend.Show(Shown);
        return Shown.Latest;
    }

    void World::StopGame(const RestoreSnapshot t_Restore)
    {
        F_LOG_TRACE("World: stop play");
        UseEditDriver();

        // Systems and the game clean up their own state while the entities still exist
        m_Runner->PlayEnd(WorldRole::Play);
        m_Game->OnStopGame(m_Storage);
        UseLink(std::make_unique<DirectLink>(m_Storage, *m_Runner, std::vector<Detail::ComponentId>{}));

        if (t_Restore == RestoreSnapshot::Yes && !m_PlaySnapshot.IsNull())
        {
            // Removes every entity that play made or changed, then makes the
            // entities from the start of play again
            LoadFromJson(m_PlaySnapshot, "PlaySnapshot");
        }
        m_PlaySnapshot = Json();

        // Edit ticks run again, also after a Quit ended the session, and the session's clock
        // state ends with it: a pause or a waiting clock command would hold Edit and the next session
        m_TickRole = WorldRole::Edit;
        m_QuitTick = 0;
        m_RanLastTick.store(false, std::memory_order_release);
        m_ClockCommands.Drain([](const ClockCommand&) {});
        m_WaitingClockCommands.clear();
        if (m_Runner->IsPaused())
        {
            m_Runner->SetPaused(false, m_TickClock());
        }
        m_CurrentState = WorldState::Initalized;
    }

    void World::Update(const float t_DeltaTime)
    {
        const auto RealDelta = std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::duration<double>(static_cast<double>(t_DeltaTime)));
        m_SummedDeltaNs.fetch_add(RealDelta.count(), std::memory_order_acq_rel);

        // Start and stop requests come from anywhere (the editor UI draws during rendering,
        // a Quit from a tick), so they run here, before the frame's wakes
        ProcessPlayRequest();
        if (!m_Runner)
        {
            return;
        }
        m_Driver->OnFrame();
        ApplyTickOutbox();
        RunFrame(RealDelta);
    }

    uint32 World::RunWake()
    {
        const int64 ClockNs = m_TickClock();
        ApplyClockCommands(WakeTimeFrom(ClockNs));
        // After the commands: a new rate changes the step and MaxTicksPerWake
        const uint32 Ticks = m_Runner->RunFixedAt(WakeTimeFrom(ClockNs), m_TickRole, TickLimit());
        m_TickCount.store(m_Runner->TickCount(), std::memory_order_release);
        if (m_Runner->TickCount() >= LastTickOfSession())
        {
            m_RanLastTick.store(true, std::memory_order_release);
        }
        return Ticks;
    }

    int64 World::WakeTimeFrom(const int64 t_ClockNs) const
    {
        const TickTimeline& Timeline = m_Runner->GetTimeline();
        if (!m_TickClockOnlyMovesWithFrames || !Timeline.IsStarted() || Timeline.IsPaused())
        {
            return t_ClockNs;
        }
        const int64 OneWakeNs = static_cast<int64>(m_Runner->GetConfig().MaxTicksPerWake() - 1) * Timeline.StepNs();
        return Math::Min(t_ClockNs, Timeline.NextWindow().EndNs + OneWakeNs);
    }

    void World::ApplyClockCommands(const int64 t_NowNs)
    {
        m_ClockCommands.Drain([this](const ClockCommand& t_Command) { m_WaitingClockCommands.push_back(t_Command); });
        const uint64 NextTick = m_Runner->TickCount() + 1;
        std::erase_if(m_WaitingClockCommands,
                      [&](const ClockCommand& t_Command)
                      {
                          if (t_Command.BeforeTick != 0 && t_Command.BeforeTick < NextTick)
                          {
                              // Its tick already ran: a repeat
                              return true;
                          }
                          if (t_Command.BeforeTick > NextTick)
                          {
                              return false;
                          }
                          switch (t_Command.Type)
                          {
                          case ClockCommandType::Pause:
                          case ClockCommandType::Resume:
                          {
                              const bool bPause = t_Command.Type == ClockCommandType::Pause;
                              if (bPause != m_Runner->IsPaused())
                              {
                                  m_Runner->SetPaused(bPause, t_NowNs);
                              }
                              break;
                          }
                          case ClockCommandType::SetRate:
                          {
                              const uint32 Was = m_Runner->GetConfig().SimHz;
                              String Locked;
                              if (InputLocksSimRate(Was, Locked))
                              {
                                  F_LOG_WARN("Simulation rate change to {} Hz dropped before tick {}: {}",
                                             t_Command.SimHz,
                                             NextTick,
                                             Locked);
                              }
                              else if (m_Runner->SetSimHz(t_Command.SimHz))
                              {
                                  m_SimHz.store(t_Command.SimHz, std::memory_order_release);
                                  F_LOG_TRACE(
                                      "Simulation rate {} Hz from tick {} (was {} Hz)", t_Command.SimHz, NextTick, Was);
                              }
                              break;
                          }
                          }
                          return true;
                      });
    }

    uint64 World::LastTickOfSession() const
    {
        uint64 Last = std::numeric_limits<uint64>::max();
        for (const uint64 Stop : {m_LastTick, m_QuitTick})
        {
            if (Stop != 0)
            {
                Last = Math::Min(Last, Stop);
            }
        }
        return Last;
    }

    uint32 World::TickLimit() const
    {
        uint64 Last = LastTickOfSession();
        // A stamped clock command waits for its tick: no tick past the one before it
        for (const ClockCommand& Waiting : m_WaitingClockCommands)
        {
            Last = Math::Min(Last, Waiting.BeforeTick - 1);
        }
        const uint64 Ran = m_Runner->TickCount();
        return Last <= Ran ? 0u : static_cast<uint32>(Math::Min<uint64>(Last - Ran, UINT32_MAX));
    }

    int64 World::NanosecondsToNextTick() const
    {
        const TickTimeline& Timeline = m_Runner->GetTimeline();
        return Timeline.IsStarted() ? Timeline.NextWindow().EndNs - m_TickClock() : 0;
    }

    bool World::IsTickClockPaused() const
    {
        return m_Runner->IsPaused();
    }

    bool World::OnTickDone(const uint64 t_Tick)
    {
        // Published before its outbox entry: the frame that applies the entry can show the tick
        m_Link->Publish(m_Storage, *m_Runner);

        if (m_KeepTickRecords)
        {
            m_NewTickRecords.Push(*m_Runner->GetLedger().Latest());
        }
        if (std::binary_search(m_StateDumpTicks.begin(), m_StateDumpTicks.end(), t_Tick))
        {
            StateDump Dump;
            Dump.Tick = t_Tick;
            if (SaveStateToJson(Dump.Doc, t_Tick))
            {
                m_StateDumps.Push(std::move(Dump));
            }
        }
        AppRequests* Queue = m_Storage.TryGetSingleton<AppRequests>();
        if (!Queue || Queue->Requests.empty())
        {
            return true;
        }
        TickOutboxEntry Entry{t_Tick, std::exchange(Queue->Requests, {})};
        const bool bQuit = std::ranges::find(Entry.Requests, AppRequest::Quit) != Entry.Requests.end();
        m_Outbox.Push(std::move(Entry));

        // A Quit ends the session at its tick, unless it has no effect here (editor Edit mode)
        if (bQuit && (m_TickRole == WorldRole::Play || !m_GameQuitStopsPlay))
        {
            m_QuitTick = t_Tick;
            return false;
        }
        return true;
    }

    void World::ApplyTickOutbox()
    {
        m_Outbox.Drain(
            [this](TickOutboxEntry& t_Entry)
            {
                if (t_Entry.Tick <= m_LastAppliedOutboxTick)
                {
                    return;
                }
                m_LastAppliedOutboxTick = t_Entry.Tick;
                for (const AppRequest Request : t_Entry.Requests)
                {
                    if (Request == AppRequest::Quit)
                    {
                        OnGameQuit();
                    }
                    else
                    {
                        m_AppRequestsForApp.push_back(Request);
                    }
                }
            });
    }

    void World::OnGameQuit()
    {
        if (!m_GameQuitStopsPlay)
        {
            m_QuitRequested = true;
        }
        else if (m_CurrentState == WorldState::Playing && m_PendingRequest == PlayRequest::None)
        {
            // Editor play: stop play at the next Update, and keep the editor open
            m_PendingRequest = PlayRequest::Stop;
        }
    }

    void World::RunFrame(const std::chrono::nanoseconds t_RealDelta)
    {
        m_FrameJitter.Sleep();
        const TickStamp Shown = ShowLatest();

        // The input that came in while the ticks ran, pending for the frame systems
        if (m_LateInputPoll)
        {
            m_LateInputPoll();
        }

        m_FrameJitter.Sleep();
        // Play draws between the latest two published ticks; Edit draws the latest state, so an edit shows at once
        const int64 FrameNs = m_TickClock();
        const bool bPlay = m_CurrentState == WorldState::Playing;
        const BlendFrame* Blend = bPlay ? m_Blend.Advance(FrameNs) : nullptr;
        m_Runner->RunFrameAt(FrameNs, t_RealDelta, bPlay ? WorldRole::Play : WorldRole::Edit, Shown, Blend);
    }

    EntityStorage& World::GetPresentedStorage()
    {
        return m_Link ? m_Link->FrameStorage() : m_Storage;
    }

    uint64 World::GetShownTick() const
    {
        return m_Link ? m_Link->ShownTick() : 0;
    }

    const PublishChannel* World::GetPublishChannel() const
    {
        return m_Link ? m_Link->Channel() : nullptr;
    }

    bool World::ShouldTickStopWaiting() const
    {
        const GameLane* Lane = m_GameLane.load(std::memory_order_acquire);
        return Lane == nullptr || Lane->ShouldStopWaiting();
    }

    TickInputProviderConfig World::MakeTickInputConfig(const int64 t_ThreadedWatermarkWaitNs) const
    {
        TickInputProviderConfig Config;
        if (m_ExecMode != ExecMode::Threaded)
        {
            return Config;
        }
        Config.CutSender = TickCutSender::Writer;
        Config.TickLane = Lane::Game;
        Config.WatermarkWaitNs = t_ThreadedWatermarkWaitNs;
        Config.StopWaiting = [this]()
        {
            return ShouldTickStopWaiting();
        };
        return Config;
    }

    void World::StepGameLane(const uint32 t_Wakes)
    {
        GameLane* Lane = m_GameLane.load(std::memory_order_relaxed);
        assert(Lane && "StepGameLane is for Threaded play");
        Lane->Step(t_Wakes);
    }

    std::vector<TickRecord> World::TakeNewTickRecords()
    {
        std::vector<TickRecord> Records;
        m_NewTickRecords.Drain([&Records](const TickRecord& t_Record) { Records.push_back(t_Record); });
        return Records;
    }

    std::vector<World::StateDump> World::TakeStateDumps()
    {
        std::vector<StateDump> Dumps;
        m_StateDumps.Drain([&Dumps](StateDump& t_Dump) { Dumps.push_back(std::move(t_Dump)); });
        return Dumps;
    }

    bool World::RequestSimHz(const uint32 t_Hz, String& t_OutError, const uint64 t_BeforeTick)
    {
        t_OutError.clear();
        if (!SimRate::IsValidHz(t_Hz))
        {
            t_OutError = "The simulation rate must be from " + std::to_string(SimRate::MinHz) + " to " +
                         std::to_string(SimRate::MaxHz) + " ticks a second, not " + std::to_string(t_Hz);
        }
        else
        {
            InputLocksSimRate(GetSimHz(), t_OutError);
        }
        if (!t_OutError.empty())
        {
            F_LOG_WARN("Simulation rate change to {} Hz refused: {}", t_Hz, t_OutError);
            return false;
        }
        // Sent also when it is the rate now: a rate change still queued may be undone by it
        SendClockCommand(ClockCommand{.Type = ClockCommandType::SetRate, .BeforeTick = t_BeforeTick, .SimHz = t_Hz});
        return true;
    }

    bool World::InputLocksSimRate(const uint32 t_Hz, String& t_OutReason)
    {
#if WITH_INPUT_RECORDING
        Input& Engine = Input::Get();
        if (!Engine.IsInitialized())
        {
            return false;
        }
        if (Engine.GetPlayback().IsActive())
        {
            t_OutReason = "An input replay is playing: its tick cuts are at one rate, so the rate stays " +
                          std::to_string(t_Hz) + " Hz until it ends";
            return true;
        }
        if (Engine.GetRecorder().IsActive())
        {
            t_OutReason = "An input recording is running: a recording stores one tick rate, so the rate stays " +
                          std::to_string(t_Hz) + " Hz until it stops";
            return true;
        }
#else
        (void)t_Hz;
        (void)t_OutReason;
#endif
        return false;
    }

    void World::SetInterpolation(const bool t_On)
    {
        m_Blend.SetInterpolation(t_On);
    }

    void World::SendClockCommand(const ClockCommand& t_Command)
    {
        m_ClockCommands.Push(t_Command);
        if (m_Driver)
        {
            m_Driver->OnClockCommand();
        }
    }

    bool World::OutputLevelFile(const StringView t_LevelToLoad)
    {
        for (const auto& [Entity, Guid] : MakeMissingEntityGuids())
        {
            m_Storage.AddOrReplace<EntityGuid>(Entity, EntityGuid{Guid});
        }

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

    std::unordered_map<Fling::Entity, uint64> World::MakeMissingEntityGuids(const bool t_FromHandles) const
    {
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

        // Ids name entities across level files, so they are random. No simulation state holds
        // one (EntityGuid is RollbackClass::Keep, and the save does not write it into the storage).
        std::unordered_map<Fling::Entity, uint64> NewGuids;
        if (NeedGuid.empty())
        {
            return NewGuids;
        }
        // A state dump makes the same ids in the same run: each one comes from the handle,
        // with the top bit set so it is not 0, and the next free value if it is taken
        std::mt19937_64 Generator{t_FromHandles ? 0u : std::random_device{}()};
        for (const Fling::Entity Entity : NeedGuid)
        {
            uint64 Value = t_FromHandles ? (Entity.ToBits() | (uint64{1} << 63)) : 0;
            while (Value == 0 || !UsedGuids.insert(Value).second)
            {
                Value = t_FromHandles ? Value + 1 : Generator();
            }
            NewGuids.emplace(Entity, Value);
        }
        return NewGuids;
    }

    bool World::SaveToJson(Json& t_OutDoc, const StringView t_Title) const
    {
        if (!m_TypesBuilt)
        {
            F_LOG_ERROR("Cannot save the world before World::Init");
            return false;
        }

        // Every entity needs an id, so that Entity fields in other components can name it
        const std::unordered_map<Fling::Entity, uint64> NewGuids = MakeMissingEntityGuids();
        const TypeDesc* GuidType = m_Types.Find(EntityGuid::Component.Name);
        assert(GuidType != nullptr && "World::Init registers EntityGuid");
        const SaveContext Context{&m_Storage, &NewGuids};
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
                if (const auto Found = NewGuids.find(entity); Found != NewGuids.end())
                {
                    const EntityGuid Assigned{Found->second};
                    Json GuidJson = Json::Object();
                    SaveObject(*GuidType, &Assigned, GuidJson, Context);
                    entityJson.Set(GuidType->Name, GuidJson);
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

    bool World::SaveStateToJson(Json& t_OutDoc, const uint64 t_Tick) const
    {
        if (!m_TypesBuilt)
        {
            F_LOG_ERROR("Cannot write the world state before World::Init");
            return false;
        }

        // The types that the state hash covers (rollback state) and the ones a level saves,
        // with every field, Transient ones too
        std::vector<const TypeDesc*> ComponentTypes;
        std::vector<const TypeDesc*> SingletonTypes;
        for (const TypeDesc& Type : m_Types.All())
        {
            if (!Type.IsSaved() && !Type.IsRollbackState())
            {
                continue;
            }
            if (Type.IsSingleton())
            {
                SingletonTypes.push_back(&Type);
            }
            else if (!IsReservedEntityKey(Type.Name))
            {
                ComponentTypes.push_back(&Type);
            }
        }
        const auto ByName = [](const TypeDesc* a, const TypeDesc* b)
        {
            return a->Name < b->Name;
        };
        std::sort(ComponentTypes.begin(), ComponentTypes.end(), ByName);
        std::sort(SingletonTypes.begin(), SingletonTypes.end(), ByName);

        // Entities in handle order, which is the same for the same run. Ids made from the handles,
        // so Entity fields name their targets the same way in every run
        const std::unordered_map<Fling::Entity, uint64> NewGuids = MakeMissingEntityGuids(/* t_FromHandles */ true);
        SaveContext Context{&m_Storage, &NewGuids};
        Context.IncludeTransientFields = true;
        std::vector<Fling::Entity> Entities;
        m_Storage.EachEntity([&](const Fling::Entity t_Entity) { Entities.push_back(t_Entity); });
        std::sort(Entities.begin(), Entities.end());

        Json EntitiesJson = Json::Array();
        for (const Fling::Entity Each : Entities)
        {
            Json EntityJson = Json::Object();
            char Handle[32];
            std::snprintf(Handle, sizeof(Handle), "%u:%u", Each.Index, Each.Generation);
            EntityJson.Set("entity", StringView(Handle));
            if (const NameComponent* Named = m_Storage.TryGet<NameComponent>(Each))
            {
                EntityJson.Set("name", Named->Name.ToView());
            }
            if (const auto Found = NewGuids.find(Each); Found != NewGuids.end())
            {
                EntityJson.SetUInt64("guid", Found->second);
            }
            else if (const EntityGuid* Guid = m_Storage.TryGet<EntityGuid>(Each))
            {
                EntityJson.SetUInt64("guid", Guid->Value);
            }
            for (const TypeDesc* Type : ComponentTypes)
            {
                if (Type->Has == nullptr || !Type->Has(m_Storage, Each) || Type->Name == EntityGuid::Component.Name)
                {
                    continue;
                }
                Json ComponentJson = Json::Object();
                if (!Type->IsTag() && Type->GetConst)
                {
                    SaveObject(*Type, Type->GetConst(m_Storage, Each), ComponentJson, Context);
                }
                EntityJson.Set(Type->Name, ComponentJson);
            }
            EntitiesJson.PushBack(EntityJson);
        }

        Json Singletons = Json::Object();
        for (const TypeDesc* Type : SingletonTypes)
        {
            if (const void* Object = Type->GetSingletonConst ? Type->GetSingletonConst(m_Storage) : nullptr)
            {
                Json SingletonJson = Json::Object();
                SaveObject(*Type, Object, SingletonJson, Context);
                Singletons.Set(Type->Name, SingletonJson);
            }
        }

        uint64 StateHash = 0;
        if (m_Runner)
        {
            if (const TickRecord* Latest = m_Runner->GetLedger().Latest(); Latest && Latest->Stamp.Tick == t_Tick)
            {
                StateHash = Latest->Stamp.StateHash;
            }
        }
        char HashText[17];
        std::snprintf(HashText, sizeof(HashText), "%016llx", static_cast<unsigned long long>(StateHash));

        t_OutDoc = Json::Object();
        t_OutDoc.SetUInt64("tick", t_Tick);
        t_OutDoc.Set("stateHash", StringView(HashText));
        t_OutDoc.SetUInt64("simHz", GetSimHz());
        t_OutDoc.SetUInt64("entityCount", Entities.size());
        t_OutDoc.Set("entities", EntitiesJson);
        t_OutDoc.Set("singletons", Singletons);
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
