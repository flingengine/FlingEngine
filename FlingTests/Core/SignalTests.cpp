#include "Signals.h"

#include <catch2/catch_all.hpp>
#include <vector>

using namespace Fling;

namespace
{
    std::vector<int> g_FreeCalls;

    void FreeCallback(int Value)
    {
        g_FreeCalls.push_back(Value);
    }

    struct Listener
    {
        std::vector<int> Calls;

        void OnValue(int Value) { Calls.push_back(Value); }
    };
} // namespace

TEST_CASE("Signal calls connected callbacks in order", "[core][signal]")
{
    g_FreeCalls.clear();
    Signal<void(int)> OnValue;
    Sink<void(int)> Connections{OnValue};
    Listener A;
    Listener B;

    Connections.Connect<&Listener::OnValue>(A);
    Connections.Connect<&FreeCallback>();
    Connections.Connect<&Listener::OnValue>(B);
    // A second connect of the same pair is ignored.
    Connections.Connect<&Listener::OnValue>(A);
    REQUIRE(OnValue.Size() == 3);

    OnValue.Publish(7);
    REQUIRE(A.Calls == std::vector<int>{7});
    REQUIRE(B.Calls == std::vector<int>{7});
    REQUIRE(g_FreeCalls == std::vector<int>{7});

    Connections.Disconnect<&Listener::OnValue>(A);
    Connections.Disconnect<&FreeCallback>();
    OnValue.Publish(8);
    REQUIRE(A.Calls == std::vector<int>{7});
    REQUIRE(B.Calls == std::vector<int>{7, 8});
    REQUIRE(g_FreeCalls == std::vector<int>{7});

    Connections.DisconnectAll(B);
    REQUIRE(OnValue.Empty());
}

namespace
{
    Signal<void(int)>* g_Reentrant = nullptr;
    std::vector<int> g_LateCalls;

    void LateCallback(int Value)
    {
        g_LateCalls.push_back(Value);
    }

    template <int N>
    void FillerCallback(int)
    {
    }

    /** Connects several callbacks while the signal is publishing, so the list grows and moves. */
    void ConnectWhilePublishing(int)
    {
        Sink<void(int)> Connections{*g_Reentrant};
        Connections.Connect<&FillerCallback<0>>();
        Connections.Connect<&FillerCallback<1>>();
        Connections.Connect<&FillerCallback<2>>();
        Connections.Connect<&FillerCallback<3>>();
        Connections.Connect<&LateCallback>();
    }

    /** Disconnects itself while the signal is publishing. */
    void DisconnectSelf(int)
    {
        Sink<void(int)>{*g_Reentrant}.Disconnect<&DisconnectSelf>();
    }
} // namespace

TEST_CASE("Signal callbacks may connect and disconnect while it publishes", "[core][signal]")
{
    Signal<void(int)> OnValue;
    g_Reentrant = &OnValue;
    g_LateCalls.clear();
    Sink<void(int)> Connections{OnValue};
    Connections.Connect<&DisconnectSelf>();
    Connections.Connect<&ConnectWhilePublishing>();

    // The list grows while it is read. A callback connected now runs on the next publish.
    OnValue.Publish(1);
    REQUIRE(g_LateCalls.empty());
    REQUIRE(OnValue.Size() == 6);

    OnValue.Publish(2);
    REQUIRE(g_LateCalls == std::vector<int>{2});
    g_Reentrant = nullptr;
}
