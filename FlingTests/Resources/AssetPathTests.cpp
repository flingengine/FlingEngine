#include "AssetPath.h"
#include "File.h"
#include "Logger.h"
#include "ResourceManager.h"

#include <catch2/catch_all.hpp>
#include <filesystem>
#include <fstream>
#include <unordered_set>

using namespace Fling;

namespace
{
    void InitLogger()
    {
        Logger::Get().Init();
        // Keep startup log lines off the console so a passing run stays quiet.
        Logger::GetCurrentConsole()->set_level(spdlog::level::off);
        Logger::GetCurrentLogFile()->set_level(spdlog::level::off);
    }
} // namespace

TEST_CASE("AssetPath makes one canonical form", "[resource][AssetPath]")
{
    CHECK(AssetPath::Canonicalize("Materials/Default.mat") == "Materials/Default.mat");
    CHECK(AssetPath::Canonicalize("Materials\\Default.mat") == "Materials/Default.mat");
    CHECK(AssetPath::Canonicalize("./Materials/Default.mat") == "Materials/Default.mat");
    CHECK(AssetPath::Canonicalize("/Materials/Default.mat") == "Materials/Default.mat");
    CHECK(AssetPath::Canonicalize("Materials//Default.mat") == "Materials/Default.mat");
    CHECK(AssetPath::Canonicalize("Materials/./Default.mat") == "Materials/Default.mat");
    CHECK(AssetPath::Canonicalize("Models/../Materials/Default.mat") == "Materials/Default.mat");
    CHECK(AssetPath::Canonicalize("Materials/") == "Materials");
    CHECK(AssetPath::Canonicalize("../Shared/Icon.png") == "../Shared/Icon.png");
    CHECK(AssetPath::Canonicalize("") == "");
    CHECK(AssetPath::Canonicalize("./") == "");

    // Case is kept.
    CHECK(AssetPath::Canonicalize("Materials/default.MAT") == "Materials/default.MAT");
}

TEST_CASE("Paths with the same canonical form have the same AssetId", "[resource][AssetPath]")
{
    const AssetPath A("Materials/Default.mat");
    const AssetPath B("./Materials\\Default.mat");
    const AssetPath Other("Materials/Bronze.mat");

    CHECK(A.GetText() == "Materials/Default.mat");
    CHECK(A.GetId() == B.GetId());
    CHECK(A.GetId() != Other.GetId());
    CHECK(!A.GetId().IsNone());

    // The id is the Name of the canonical path, so it is the same in every process.
    CHECK(A.GetId().GetValue() == HashText("Materials/Default.mat"));
    CHECK(AssetId::FromValue(A.GetId().GetValue()) == A.GetId());

    // The empty path has no id.
    CHECK(AssetPath().IsEmpty());
    CHECK(AssetPath().GetId().IsNone());
    CHECK(AssetPath("./").GetId().IsNone());

    std::unordered_set<AssetId> Ids = {A.GetId(), B.GetId(), Other.GetId()};
    CHECK(Ids.size() == 2);
}

TEST_CASE("AssetId and AssetPath log as the path text", "[resource][AssetPath]")
{
    const AssetPath Path("Levels/PhysicsTest.json");
    CHECK(fmt::format("{}", Path) == "Levels/PhysicsTest.json");
    CHECK(fmt::format("{}", Path.GetId()) == "Levels/PhysicsTest.json");
    CHECK(fmt::format("{}", AssetId()) == "None");
}

TEST_CASE("ResourceManager loads each asset once, keyed by AssetId", "[resource][AssetPath]")
{
    InitLogger();
    ResourceManager::Get().Init();

    const std::shared_ptr<File> First = File::Create(AssetPath("TestFile.txt"));
    const std::shared_ptr<File> Second = File::Create(AssetPath("./TestFile.txt"));

    REQUIRE(First);
    CHECK(First == Second);
    CHECK(First->GetAssetPath().GetText() == "TestFile.txt");
    CHECK(First->GetAssetId() == AssetPath("TestFile.txt").GetId());
    CHECK(ResourceManager::Get().IsLoaded(First->GetAssetId()));
    CHECK(ResourceManager::Get().GetResource(First->GetAssetId()) == First);
    CHECK(!ResourceManager::Get().IsLoaded(AssetPath("NotLoaded.txt").GetId()));

    ResourceManager::Get().Shutdown();
}

TEST_CASE("AssetPath finds a case that does not match the files on disk", "[resource][AssetPath]")
{
    const std::filesystem::path Root = std::filesystem::temp_directory_path() / "FlingAssetCaseTests";
    std::filesystem::remove_all(Root);
    std::filesystem::create_directories(Root / "Textures" / "Brick");
    std::ofstream(Root / "Textures" / "Brick" / "Albedo.png") << "x";

    String OnDisk;
    CHECK(AssetPath("Textures/Brick/Albedo.png").MatchesCaseOnDisk(Root, &OnDisk));
    CHECK(OnDisk.empty());

    CHECK(!AssetPath("textures/brick/albedo.PNG").MatchesCaseOnDisk(Root, &OnDisk));
    CHECK(OnDisk == "Textures/Brick/Albedo.png");

    // A part that does not exist with any case is not a mismatch.
    CHECK(AssetPath("Textures/Missing/Albedo.png").MatchesCaseOnDisk(Root));
    CHECK(!AssetPath("textures/Missing.png").MatchesCaseOnDisk(Root, &OnDisk));
    CHECK(OnDisk == "Textures/Missing.png");

    std::filesystem::remove_all(Root);
}
