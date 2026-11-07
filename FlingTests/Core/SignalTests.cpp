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
