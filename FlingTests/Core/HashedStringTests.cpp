#include "FlingTypes.h"
#include "HashedString.h"
#include "Signal.h"

#include <catch2/catch_all.hpp>
#include <string>
#include <vector>

using namespace Fling;

TEST_CASE("HashedString matches 32-bit FNV-1a", "[core][hashed_string]")
{
    // Known FNV-1a 32-bit values.
    STATIC_REQUIRE(HashedString::ToValue("") == 2166136261u);
    STATIC_REQUIRE(HashedString::ToValue("a") == 0xe40c292cu);
    STATIC_REQUIRE(HashedString::ToValue("foobar") == 0xbf9cf968u);

    constexpr HashedString Literal{"foobar"};
    STATIC_REQUIRE(Literal.value() == 0xbf9cf968u);

    const std::string Runtime = "foobar";
    const HashedString FromPointer{Runtime.c_str()};
    REQUIRE(FromPointer == Literal);
    REQUIRE(FromPointer.value() == HashedString::ToValue(Runtime.data(), Runtime.size()));
    REQUIRE(std::string(FromPointer.data()) == "foobar");
    REQUIRE(HS("foobar") == Literal);
    REQUIRE(HS("foo") != Literal);
    REQUIRE(INVALID_GUID == Guid{"INVALID_GUID"});
}

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
