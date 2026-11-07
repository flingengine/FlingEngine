#include "FlingIntegers.h"
#include "Strings/DisplayText.h"
#include "Strings/Hash.h"
#include "Strings/Name.h"
#include "Strings/String.h"

#include <atomic>
#include <catch2/catch_all.hpp>
#include <cstdint>
#include <cstdio>
#include <set>
#include <thread>
#include <unordered_set>
#include <vector>

using namespace Fling;

namespace
{
    // Reference values from the xxHash64 reference implementation, seed 0. They cover
    // each input size path: under 4 bytes, 4 to 7, 8 to 31, and 32 or more.
    struct HashVector
    {
        const char* Text;
        uint64 Hash;
    };

    constexpr HashVector Vectors[] = {
        {"", 0xEF46DB3751D8E999ull},
        {"a", 0xD24EC4F1A98C6E5Bull},
        {"abc", 0x44BC2CF5AD770999ull},
        {"Jump", 0x5828DE3135B70F9Dull},
        {"jump", 0x9C654EF013F646D6ull},
        {"Enemy_000001", 0xC69504AF9A92F630ull},
        {"Enemy_000002", 0x8AA40C36E00E35BDull},
        {"Deferred/mrt.vert.spv", 0x2A307383BCF69AEFull},
        {"The quick brown fox jumps over the lazy dog", 0x0B242D361FDA71BCull},
        // "Ünïcödé ✓ 日本語" as UTF-8 bytes
        {"\xC3\x9Cn\xC3\xAF"
         "c\xC3\xB6"
         "d\xC3\xA9 \xE2\x9C\x93 \xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E",
         0xD12992F21F987E79ull},
        {"abcdefghijklmnopqrstuvwxyz0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ", 0xD5000C4AC53D14A0ull},
    };

    // A literal name that only this file uses. It must be in the table before main.
    constexpr Name OnlyInThisFile = "NameTests.OnlyInThisFile"_name;

    StringView NameTextFor(const Name t_Name)
    {
        return t_Name.ToView();
    }
} // namespace

// The hash is computed at compile time.
static_assert(HashText("Jump") == 0x5828DE3135B70F9Dull);
static_assert(HashText("The quick brown fox jumps over the lazy dog") == 0x0B242D361FDA71BCull);
static_assert("Jump"_name.GetHash() == 0x5828DE3135B70F9Dull);
static_assert(Name("Jump") == "Jump"_name);
static_assert(""_name.IsNone());
static_assert(Name().IsNone());
static_assert("Jump"_name != "jump"_name);
static_assert(sizeof(Name) == 8);
static_assert(std::is_trivially_copyable_v<Name>);

TEST_CASE("HashText matches the xxHash64 reference values", "[Strings][Hash]")
{
    for (const HashVector& Vector : Vectors)
    {
        INFO("Text: " << Vector.Text);
        CHECK(HashText(Vector.Text) == Vector.Hash);
    }
}

TEST_CASE("HashText gives the same value at compile time and at runtime", "[Strings][Hash]")
{
    constexpr uint64 CompileTime = HashText("abcdefghijklmnopqrstuvwxyz0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ");
    const String Runtime = "abcdefghijklmnopqrstuvwxyz0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    CHECK(HashText(Runtime) == CompileTime);

    // A view that does not start at the start of a buffer reads the same bytes.
    const String Padded = "xx" + Runtime;
    CHECK(HashText(StringView(Padded).substr(2)) == CompileTime);
}

TEST_CASE("HashText gives no collisions over similar names", "[Strings][Hash]")
{
    std::unordered_set<uint64> Seen;
    char Buffer[32] = {};
    for (int Index = 0; Index < 200000; ++Index)
    {
        const int Length = std::snprintf(Buffer, sizeof(Buffer), "Enemy_%06d", Index);
        CHECK(Seen.insert(HashText(StringView(Buffer, static_cast<std::size_t>(Length)))).second);
    }
}

TEST_CASE("Name is None for empty text", "[Strings][Name]")
{
    CHECK(Name().IsNone());
    CHECK(Name("").IsNone());
    CHECK(Name(String()).IsNone());
    CHECK(Name().ToView().empty());
    CHECK(Name().ToString() == "None");
}

TEST_CASE("A literal Name has its text before any code makes it", "[Strings][Name]")
{
    CHECK(OnlyInThisFile.ToView() == "NameTests.OnlyInThisFile");

    // This literal is used only here, inside the test, so only static
    // initialization can have put its text in the table.
    CHECK(NameTextFor("NameTests.OnlyInsideATest"_name) == "NameTests.OnlyInsideATest");
}

TEST_CASE("A Name made from runtime text adds its text", "[Strings][Name]")
{
    const String Text = String("NameTests.Runtime.") + std::to_string(12345);
    CHECK(Detail::FindNameText(HashText(Text)).empty());

    const Name Made(Text);
    CHECK(Made.GetHash() == HashText(Text));
    CHECK(Made.ToView() == Text);
    CHECK(Made.ToString() == Text);

    // The table keeps its own copy of the text.
    const StringView Stored = Made.ToView();
    CHECK(Stored.data() != Text.data());
}

TEST_CASE("Names compare by hash, the same as text made at runtime", "[Strings][Name]")
{
    const String Typed = "Jump";
    CHECK(Name(Typed) == "Jump"_name);
    CHECK(Name(Typed) != "jump"_name);
    CHECK(std::hash<Name>{}("Jump"_name) == static_cast<std::size_t>(HashText("Jump")));

    const std::set<Name> Ordered = {"b"_name, "a"_name, "c"_name};
    uint64 Previous = 0;
    for (const Name Each : Ordered)
    {
        CHECK(Each.GetHash() > Previous);
        Previous = Each.GetHash();
    }
}

TEST_CASE("Name::FromHash keeps the hash, and shows it when the text is unknown", "[Strings][Name]")
{
    const Name Known = Name::FromHash("Jump"_name.GetHash());
    CHECK(Known == "Jump"_name);
    CHECK(Known.ToView() == "Jump");

    const Name Unknown = Name::FromHash(0x0123456789ABCDEFull);
    CHECK(Unknown.ToView().empty());
    CHECK(Unknown.ToString() == "#0123456789abcdef");
}

TEST_CASE("The name table reports a collision and keeps the first text", "[Strings][Name]")
{
    // Two different texts cannot be made to collide on purpose, so this uses a hash
    // that is not the real hash of either text.
    const uint64 Hash = 0x0F1E2D3C4B5A6978ull;
    CHECK(Detail::RegisterNameText(Hash, "NameTests.First") == Detail::NameRegisterResult::Added);
    CHECK(Detail::RegisterNameText(Hash, "NameTests.First") == Detail::NameRegisterResult::AlreadyPresent);
    CHECK(Detail::RegisterNameText(Hash, "NameTests.Second") == Detail::NameRegisterResult::Collision);
    CHECK(Detail::FindNameText(Hash) == "NameTests.First");

    // Hash 0 is None, so no text can use it.
    CHECK(Detail::RegisterNameText(0, "NameTests.Zero") == Detail::NameRegisterResult::Collision);
}

TEST_CASE("Names can be made from many threads at once", "[Strings][Name]")
{
    constexpr int ThreadCount = 8;
    constexpr int NamesPerThread = 2000;

    const NameTableStats Before = GetNameTableStats();
    std::atomic<int> Mismatches{0};
    std::vector<std::thread> Threads;
    for (int ThreadIndex = 0; ThreadIndex < ThreadCount; ++ThreadIndex)
    {
        Threads.emplace_back(
            [ThreadIndex, &Mismatches]()
            {
                for (int Index = 0; Index < NamesPerThread; ++Index)
                {
                    // Every thread makes the shared names and its own names, and reads
                    // them back while the other threads add more.
                    const String Shared = "NameTests.Shared." + std::to_string(Index);
                    const String Own = "NameTests.Thread" + std::to_string(ThreadIndex) + "." + std::to_string(Index);
                    if (Name(Shared).ToView() != Shared || Name(Own).ToView() != Own)
                    {
                        Mismatches.fetch_add(1);
                    }
                }
            });
    }
    for (std::thread& Thread : Threads)
    {
        Thread.join();
    }

    CHECK(Mismatches.load() == 0);
    const NameTableStats After = GetNameTableStats();
    CHECK(After.Count - Before.Count == NamesPerThread + ThreadCount * NamesPerThread);
    CHECK(After.ReservedBytes >= After.TextBytes);
}

TEST_CASE("A very long text gets stored whole", "[Strings][Name]")
{
    const String Long(200 * 1024, 'x');
    const Name Made(Long);
    CHECK(Made.ToView() == Long);
}

TEST_CASE("Name formats as its text in log messages", "[Strings][Name]")
{
    CHECK(fmt::format("{}", "Jump"_name) == "Jump");
    CHECK(fmt::format("[{}]", Name()) == "[None]");
    CHECK(fmt::format("{}", Name::FromHash(0x0123456789ABCDEFull)) == "#0123456789abcdef");
}

TEST_CASE("Paths round trip UTF-8 text", "[Strings][String]")
{
    // "Levels/Ünïcödé.json" as UTF-8 bytes
    const String Text = "Levels/\xC3\x9Cn\xC3\xAF"
                        "c\xC3\xB6"
                        "d\xC3\xA9.json";
    CHECK(PathToUtf8(PathFromUtf8(Text)) == Text);
    CHECK(PathToUtf8(PathFromUtf8(Text).filename()) == Text.substr(7));
}

TEST_CASE("DisplayText resolves its source", "[Strings][DisplayText]")
{
    CHECK(DisplayText().IsEmpty());
    CHECK(DisplayText().Resolve().empty());
    CHECK(DisplayText::AsInvariant("Player One").Resolve() == "Player One");

    // Until string tables exist, a table entry resolves to its key.
    CHECK(DisplayText::FromTable("UI"_name, "Menu.Start"_name).Resolve() == "Menu.Start");
}

TEST_CASE("DisplayText::Format replaces named arguments", "[Strings][DisplayText]")
{
    const DisplayText Pattern = DisplayText::AsInvariant("{Count} enemies left, {Name}! {Missing} {{x}}");
    const DisplayText Text =
        DisplayText::Format(Pattern, {{"Count"_name, 3}, {"Name"_name, DisplayText::AsInvariant("Ben")}});
    CHECK(Text.Resolve() == "3 enemies left, Ben! {Missing} {x}");

    const DisplayText Ratio = DisplayText::Format(DisplayText::AsInvariant("{Value}"), {{"Value"_name, 0.5}});
    CHECK(Ratio.Resolve() == "0.5");

    // The argument keeps its source, so the result is made again on each Resolve.
    const DisplayText Nested = DisplayText::Format(DisplayText::AsInvariant("<{Inner}>"), {{"Inner"_name, Text}});
    CHECK(Nested.Resolve() == "<3 enemies left, Ben! {Missing} {x}>");
    CHECK(fmt::format("{}", Nested) == "<3 enemies left, Ben! {Missing} {x}>");
}

TEST_CASE("Name::WithoutText looks up text without adding it", "[Strings][Name]")
{
    const String Text = "NameTests.LookupOnly." + std::to_string(777);
    const Name Lookup = Name::WithoutText(Text);
    CHECK(Lookup.GetHash() == HashText(Text));
    CHECK(Lookup.ToView().empty());
    CHECK(Detail::FindNameText(HashText(Text)).empty());

    // It finds a Name that was made with its text.
    CHECK(Name::WithoutText("Jump") == "Jump"_name);
    CHECK(Name::WithoutText("").IsNone());
    static_assert(Name::WithoutText("Jump") == "Jump"_name);
}
