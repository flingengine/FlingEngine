#include "Ecs/Access.h"
#include "Ecs/Component.h"
#include "Ecs/Entity.h"
#include "Ecs/Phase.h"
#include "Ecs/TypeHash.h"
#include "Ecs/TypeList.h"
#include "FlingTypes.h"

#include <catch2/catch_all.hpp>
#include <vector>

namespace
{
    struct Position
    {
        static constexpr Fling::ComponentInfo Component{.Name = "Test.Position"};
        float X = 0.0f;
    };

    struct Velocity
    {
        static constexpr Fling::ComponentInfo Component{.Name = "Test.Velocity"};
        float X = 0.0f;
    };

    struct Camera
    {
        static constexpr Fling::ComponentInfo Component{.Name = "Test.Camera",
                                                        .Domain = Fling::ComponentDomain::Presentation};
        float Smoothing = 0.0f;
    };

    struct Player
    {
        static constexpr Fling::ComponentInfo Component{.Name = "Test.Player", .Kind = Fling::ComponentKind::Tag};
    };

    struct Clock
    {
        static constexpr Fling::ComponentInfo Component{
            .Name = "Test.Clock", .Kind = Fling::ComponentKind::Singleton, .Transient = true};
        double Seconds = 0.0;
    };

    struct SpawnQueue
    {
        static constexpr Fling::ComponentInfo Component{
            .Name = "Test.SpawnQueue", .Kind = Fling::ComponentKind::Queue, .Transient = true};
        std::vector<int> Requests;
    };

    /** A type that cannot be edited: it opts in through ComponentTraits. */
    struct External
    {
        int Value = 0;
    };

    /** A struct with no declaration. */
    struct NotAComponent
    {
        int Value = 0;
    };

    /** Declared as a table, but empty. */
    struct EmptyTable
    {
        static constexpr Fling::ComponentInfo Component{.Name = "Test.EmptyTable"};
    };

    /** Declared as a tag, but has data. */
    struct TagWithData
    {
        static constexpr Fling::ComponentInfo Component{.Name = "Test.TagWithData", .Kind = Fling::ComponentKind::Tag};
        int Value = 0;
    };
} // namespace

template <>
struct Fling::ComponentTraits<External>
{
    static constexpr Fling::ComponentInfo Info{.Name = "Test.External"};
};

using namespace Fling;

TEST_CASE("TypeList finds its types", "[ecs]")
{
    using List = TypeList<int, float>;
    STATIC_REQUIRE(List::Size == 2);
    STATIC_REQUIRE(Contains<List, int>);
    STATIC_REQUIRE(Contains<List, float>);
    STATIC_REQUIRE_FALSE(Contains<List, double>);
    STATIC_REQUIRE(AllUnique<int, float, double>);
    STATIC_REQUIRE_FALSE(AllUnique<int, float, int>);
}

TEST_CASE("HashName is FNV-1a and runs at compile time", "[ecs]")
{
    // Published FNV-1a 64-bit test vectors.
    STATIC_REQUIRE(HashName("") == uint64{0xcbf29ce484222325});
    STATIC_REQUIRE(HashName("a") == uint64{0xaf63dc4c8601ec8c});
    STATIC_REQUIRE(HashName("foobar") == uint64{0x85944171f73967e8});
    STATIC_REQUIRE(TypeHashOf<Position> == HashName("Test.Position"));
    STATIC_REQUIRE(TypeHashOf<Position> != TypeHashOf<Velocity>);
}

TEST_CASE("Entity is an 8-byte handle with a null value", "[ecs]")
{
    STATIC_REQUIRE(sizeof(Entity) == 8);
    STATIC_REQUIRE(Entity{}.IsNull());
    STATIC_REQUIRE(Entity::Null.IsNull());
    STATIC_REQUIRE(Entity{} == Entity::Null);
    STATIC_REQUIRE_FALSE(Entity{3, 0}.IsNull());

    constexpr Entity e{7, 42};
    STATIC_REQUIRE(e.ToBits() == ((uint64{42} << 32) | uint64{7}));
    STATIC_REQUIRE(Entity::FromBits(e.ToBits()) == e);
    STATIC_REQUIRE(Entity{7, 43} != e);
}

TEST_CASE("The Component concept accepts declared types only", "[ecs]")
{
    STATIC_REQUIRE(Component<Position>);
    STATIC_REQUIRE(Component<Player>);
    STATIC_REQUIRE(Component<Clock>);
    STATIC_REQUIRE(Component<SpawnQueue>);
    STATIC_REQUIRE(Component<External>);

    STATIC_REQUIRE_FALSE(Component<NotAComponent>);
    STATIC_REQUIRE_FALSE(Component<int>);
    STATIC_REQUIRE_FALSE(Component<const Position>);
    STATIC_REQUIRE_FALSE(Component<Position*>);
}

TEST_CASE("ComponentInfo is read from the struct or from ComponentTraits", "[ecs]")
{
    STATIC_REQUIRE(ComponentInfoOf<Position>.Kind == ComponentKind::Table);
    STATIC_REQUIRE(ComponentInfoOf<Position>.Domain == ComponentDomain::Simulation);
    STATIC_REQUIRE_FALSE(ComponentInfoOf<Position>.Transient);
    STATIC_REQUIRE(ComponentInfoOf<Clock>.Transient);
    STATIC_REQUIRE(ComponentInfoOf<External>.Name[5] == 'E');

    STATIC_REQUIRE(IsSingletonComponent<Clock>);
    STATIC_REQUIRE(IsSingletonComponent<SpawnQueue>);
    STATIC_REQUIRE_FALSE(IsSingletonComponent<Position>);
}

TEST_CASE("A tag must be empty and a table must not be", "[ecs]")
{
    STATIC_REQUIRE(IsWellFormedComponent<Position>);
    STATIC_REQUIRE(IsWellFormedComponent<Player>);
    STATIC_REQUIRE_FALSE(IsWellFormedComponent<EmptyTable>);
    STATIC_REQUIRE_FALSE(IsWellFormedComponent<TagWithData>);
}

namespace
{
    inline constexpr PhaseInfo TestPhase{.Name = "Test.Phase", .UpdateRate = UpdateRate::Frame, .After = {"A", "B"}};
} // namespace

TEST_CASE("Phases decide the update rate", "[ecs]")
{
    STATIC_REQUIRE(IsFixed(Phases::Input));
    STATIC_REQUIRE(IsFixed(Phases::Simulation));
    STATIC_REQUIRE(IsFixed(Phases::ResolveDeferred));
    STATIC_REQUIRE_FALSE(IsFixed(Phases::PostSimulation));
    STATIC_REQUIRE_FALSE(IsFixed(Phases::Extract));
    STATIC_REQUIRE_FALSE(IsFixed(Phases::Editor));
    STATIC_REQUIRE_FALSE(IsFixed(PhaseRef{}));

    // A phase from outside the engine works the same way, at compile time.
    constexpr PhaseRef ref = TestPhase;
    STATIC_REQUIRE(ref.IsSet());
    STATIC_REQUIRE_FALSE(IsFixed(ref));
    STATIC_REQUIRE(ref->After.Size() == 2);
    STATIC_REQUIRE(Phases::All.size() == 6);
}

TEST_CASE("WorldRole is a bit mask", "[ecs]")
{
    STATIC_REQUIRE(Overlaps(WorldRole::Play, WorldRole::All));
    STATIC_REQUIRE(Overlaps(WorldRole::Edit | WorldRole::Play, WorldRole::Play));
    STATIC_REQUIRE_FALSE(Overlaps(WorldRole::Edit, WorldRole::Play));
    STATIC_REQUIRE_FALSE(Overlaps(WorldRole::None, WorldRole::All));
}

TEST_CASE("Valid access lists", "[ecs][access]")
{
    STATIC_REQUIRE(IsValidAccess<>);
    STATIC_REQUIRE(IsValidAccess<Read<Position>, Write<Velocity>>);
    STATIC_REQUIRE(IsValidAccess<Read<Player>, Read<Clock>>);
    STATIC_REQUIRE(IsValidAccess<Append<SpawnQueue>>);
    STATIC_REQUIRE(IsValidAccess<Consume<SpawnQueue>>);
    STATIC_REQUIRE(IsValidAccess<Write<Position>, Fling::Spawns>);
    STATIC_REQUIRE(IsValidAccess<Read<External>>);
}

TEST_CASE("Invalid access lists", "[ecs][access]")
{
    // Not a component, or const.
    STATIC_REQUIRE_FALSE(IsValidAccess<Read<NotAComponent>>);
    STATIC_REQUIRE_FALSE(IsValidAccess<Read<const Position>>);
    STATIC_REQUIRE_FALSE(IsValidAccess<int>);

    // A type twice. Write already implies read.
    STATIC_REQUIRE_FALSE(IsValidAccess<Read<Position>, Write<Position>>);
    STATIC_REQUIRE_FALSE(IsValidAccess<Write<Position>, Write<Position>>);
    STATIC_REQUIRE_FALSE(IsValidAccess<Fling::Spawns, Fling::Spawns>);

    // Queues use Append and Consume only. Append and Consume use queues only.
    STATIC_REQUIRE_FALSE(IsValidAccess<Read<SpawnQueue>>);
    STATIC_REQUIRE_FALSE(IsValidAccess<Write<SpawnQueue>>);
    STATIC_REQUIRE_FALSE(IsValidAccess<Append<Clock>>);
    STATIC_REQUIRE_FALSE(IsValidAccess<Consume<Position>>);
    STATIC_REQUIRE_FALSE(IsValidAccess<Append<SpawnQueue>, Consume<SpawnQueue>>);

    // Shape rules.
    STATIC_REQUIRE_FALSE(IsValidAccess<Read<EmptyTable>>);
    STATIC_REQUIRE_FALSE(IsValidAccess<Read<TagWithData>>);
}

TEST_CASE("Allows follows the declared access", "[ecs][access]")
{
    using A = Access<Read<Position>, Write<Velocity>, Append<SpawnQueue>>;

    STATIC_REQUIRE(Allows<A, AccessMode::Read, Position>);
    STATIC_REQUIRE(Allows<A, AccessMode::Read, const Position>);
    STATIC_REQUIRE_FALSE(Allows<A, AccessMode::Write, Position>);

    STATIC_REQUIRE(Allows<A, AccessMode::Read, Velocity>);
    STATIC_REQUIRE(Allows<A, AccessMode::Write, Velocity>);

    STATIC_REQUIRE(Allows<A, AccessMode::Append, SpawnQueue>);
    STATIC_REQUIRE_FALSE(Allows<A, AccessMode::Consume, SpawnQueue>);
    STATIC_REQUIRE_FALSE(Allows<A, AccessMode::Read, SpawnQueue>);

    STATIC_REQUIRE_FALSE(Allows<A, AccessMode::Read, Clock>);
    STATIC_REQUIRE_FALSE(Allows<A, AccessMode::Read, Player>);

    using B = Access<Consume<SpawnQueue>>;
    STATIC_REQUIRE(Allows<B, AccessMode::Consume, SpawnQueue>);
    STATIC_REQUIRE_FALSE(Allows<B, AccessMode::Append, SpawnQueue>);
}

TEST_CASE("Access makes a table in declaration order", "[ecs][access]")
{
    using A = Access<Write<Velocity>, Fling::Spawns, Read<Camera>, Append<SpawnQueue>>;

    STATIC_REQUIRE(A::HasSpawns);
    STATIC_REQUIRE_FALSE(Access<Read<Position>>::HasSpawns);
    STATIC_REQUIRE(A::Table.size() == 3);

    STATIC_REQUIRE(A::Table[0].Hash == TypeHashOf<Velocity>);
    STATIC_REQUIRE(A::Table[0].Mode == AccessMode::Write);
    STATIC_REQUIRE(A::Table[1].Hash == TypeHashOf<Camera>);
    STATIC_REQUIRE(A::Table[1].Mode == AccessMode::Read);
    STATIC_REQUIRE(A::Table[1].Domain == ComponentDomain::Presentation);
    STATIC_REQUIRE(A::Table[2].Mode == AccessMode::Append);
    STATIC_REQUIRE(A::Table[2].Kind == ComponentKind::Queue);
    STATIC_REQUIRE(Access<>::Table.size() == 0);
}

TEST_CASE("Simulation writes and Presentation reads are found", "[ecs][access]")
{
    STATIC_REQUIRE(WritesSimulation<Access<Write<Position>>>);
    STATIC_REQUIRE(WritesSimulation<Access<Append<SpawnQueue>>>);
    STATIC_REQUIRE_FALSE(WritesSimulation<Access<Read<Position>, Write<Camera>>>);

    STATIC_REQUIRE(ReadsPresentation<Access<Read<Camera>>>);
    STATIC_REQUIRE(ReadsPresentation<Access<Write<Camera>>>);
    STATIC_REQUIRE_FALSE(ReadsPresentation<Access<Read<Position>, Write<Velocity>>>);
}
