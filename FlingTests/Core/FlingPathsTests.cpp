#include "FlingPaths.h"
#include "Strings/String.h"

#include <catch2/catch_all.hpp>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <system_error>

using Fling::FlingPaths;
using Fling::String;

namespace
{
    String ReadAll(const std::filesystem::path& t_Path)
    {
        std::ifstream File(t_Path, std::ios::binary);
        return String(std::istreambuf_iterator<char>(File), std::istreambuf_iterator<char>());
    }
} // namespace

TEST_CASE("FlingPaths::SaveFile makes the folders and replaces the file", "[FlingPaths]")
{
    const std::filesystem::path Root = std::filesystem::temp_directory_path() / "FlingPathsSaveFileTest";
    std::error_code Ignored;
    std::filesystem::remove_all(Root, Ignored);
    const std::filesystem::path File = Root / "a" / "b" / "out.bin";

    String Error;
    // Binary: a zero byte and a newline are written as is
    const String Bytes("one\0two\n", 8);
    REQUIRE(FlingPaths::SaveFile(Fling::PathToUtf8(File), Bytes, Error));
    REQUIRE(Error.empty());
    REQUIRE(ReadAll(File) == Bytes);

    // Replaced, not appended to
    REQUIRE(FlingPaths::SaveFile(Fling::PathToUtf8(File), "short", Error));
    REQUIRE(ReadAll(File) == "short");

    std::filesystem::remove_all(Root, Ignored);
}

TEST_CASE("FlingPaths::SaveFile says why it failed", "[FlingPaths]")
{
    const std::filesystem::path Root = std::filesystem::temp_directory_path() / "FlingPathsSaveFileFail";
    std::error_code Ignored;
    std::filesystem::remove_all(Root, Ignored);
    std::filesystem::create_directories(Root / "folder", Ignored);

    // The path is a folder, so it cannot be opened as a file
    String Error;
    REQUIRE_FALSE(FlingPaths::SaveFile(Fling::PathToUtf8(Root / "folder"), "x", Error));
    REQUIRE_FALSE(Error.empty());

    std::filesystem::remove_all(Root, Ignored);
}
