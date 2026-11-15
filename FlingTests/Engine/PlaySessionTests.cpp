#include "Ecs/ModuleRegistration.h"
#include "Ecs/RegisterEngine.h"
#include "ExecMode.h"
#include "Game.h"
#include "GameLane.h"
#include "Logger.h"
#include "PlatformTime.h"
#include "World.h"

#include <catch2/catch_all.hpp>
#include <memory>

// What one play session leaves behind when it stops (Docs/threading-model.md section 5): the
// clock state of the session (a pause, clock commands) ends with it, so Edit and the next
// session tick again in every exec mode.

using namespace Fling;

namespace
{
    class EmptyGame final : public Game
    {
    public:
        void Init(EntityStorage&) override {}
        void Register(ModuleRegistration& t_Modules) override { RegisterEngine(t_Modules); }
        void OnStartGame(EntityStorage&) override {}
        void OnStopGame(EntityStorage&) override {}
        void Shutdown(EntityStorage&) override {}
    };

    /** A world on a virtual clock; in Threaded play the game thread runs one wake per Frame (Stepped pacing). */
    class SessionWorld
    {
    public:
        explicit SessionWorld(const ExecMode t_Mode)
        {
            Logger::Get().Init();
            Logger::GetCurrentConsole()->set_level(spdlog::level::off);
            Logger::GetCurrentLogFile()->set_level(spdlog::level::off);
            m_World = std::make_unique<World>(std::make_shared<EmptyGame>());
            m_World->SetExecMode(t_Mode);
            m_World->SetTickClock([]() { return PlatformTime::NowNanoseconds(); });
            m_World->SetTickTimelineStart(0);
            GameLaneDesc Desc;
            Desc.Pacing = GameLanePacing::Stepped;
            m_World->SetGameLaneDesc(Desc);
            REQUIRE(m_World->Init());
        }

        ~SessionWorld() { m_World->Shutdown(); }

        SessionWorld(const SessionWorld&) = delete;
        SessionWorld& operator=(const SessionWorld&) = delete;

        /** Move the clock on by t_Ticks tick lengths, then run one frame. */
        void Frame(const int64 t_Ticks = 1)
        {
            m_NowNs += t_Ticks * m_World->GetSystemRunnerConfig().FixedStep().count();
            m_Clock.Clock().Set(m_NowNs);
            if (m_World->GetGameLane() != nullptr)
            {
                m_World->StepGameLane(1);
            }
            m_World->Update(0.001f);
        }

        World& Get() { return *m_World; }

    private:
        PlatformTime::ScopedVirtualClock m_Clock{0};
        int64 m_NowNs = 0;
        std::unique_ptr<World> m_World;
    };
} // namespace

TEST_CASE("Play sessions: a stop while the ticks are paused leaves Edit and the next session ticking",
          "[world][play][threads]")
{
    // Mutation note: fails if StopGame keeps the runner's pause or the queued clock commands
    const ExecMode Mode = GENERATE(ExecMode::Direct, ExecMode::Inline, ExecMode::Threaded);
    INFO("exec mode " << ExecModeName(Mode));
    SessionWorld Rig(Mode);
    Rig.Get().RequestGameStart();
    Rig.Frame();
    REQUIRE(Rig.Get().IsPlaying());
    Rig.Frame(2);

    SECTION("The pause ran")
    {
        Rig.Get().PauseTicks();
        Rig.Frame();
        const uint64 Paused = Rig.Get().GetTickCount();
        Rig.Frame(2);
        REQUIRE(Rig.Get().GetTickCount() == Paused);
    }
    SECTION("The pause is still queued")
    {
        Rig.Get().PauseTicks();
    }

    Rig.Get().RequestGameStop();
    Rig.Frame(0);
    REQUIRE_FALSE(Rig.Get().IsPlaying());

    // Edit ticks run on this thread
    const uint64 Stopped = Rig.Get().GetTickCount();
    Rig.Frame(2);
    CHECK(Rig.Get().GetTickCount() > Stopped);

    // A new session ticks, also on a new game thread
    Rig.Get().RequestGameStart();
    Rig.Frame(0);
    REQUIRE(Rig.Get().IsPlaying());
    const uint64 Started = Rig.Get().GetTickCount();
    Rig.Frame(2);
    CHECK(Rig.Get().GetTickCount() > Started);
}
