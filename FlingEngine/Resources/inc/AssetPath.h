#pragma once

#include "FlingIntegers.h"
#include "spdlog/fmt/fmt.h"
#include "Strings/Name.h"
#include "Strings/String.h"

#include <compare>
#include <cstddef>
#include <filesystem>
#include <functional>

namespace Fling
{
    class AssetPath;

    /**
     * The identity of an asset: a typed wrapper around the Name of its canonical path.
     *
     * Only AssetPath makes one. Code must not depend on how the id is made, so that it
     * can later become an id that the tools assign, which survives a rename. An AssetId
     * cannot be mixed up with a Name or another kind of id.
     */
    class AssetId
    {
    public:
        /** No asset. */
        constexpr AssetId() = default;

        constexpr bool IsNone() const { return m_Name.IsNone(); }

        /** The value to write to binary data, and to read back with FromValue. */
        constexpr uint64 GetValue() const { return m_Name.GetHash(); }

        /** The AssetId of a value read from binary data. */
        static constexpr AssetId FromValue(const uint64 t_Value) { return AssetId(Name::FromHash(t_Value)); }

        /** The asset path for logs: the text if it is known, otherwise "#<hash>". */
        String ToString() const { return m_Name.ToString(); }

        friend constexpr bool operator==(AssetId, AssetId) = default;

        // clang-format off: the repo style is C++17, which does not know operator<=>.
        friend constexpr std::strong_ordering operator<=>(AssetId, AssetId) = default;
        // clang-format on

    private:
        friend class AssetPath;

        constexpr explicit AssetId(const Name t_Name) : m_Name(t_Name) {}

        Name m_Name;
    };

    /**
     * The path of an asset, relative to its root folder (the assets folder, or the
     * compiled shader folder for shaders), in one canonical form:
     *
     * - Forward slashes only, and no repeated slashes.
     * - No leading "./" or "/", and no "." segments. A ".." segment removes the segment
     *   before it.
     * - Case is kept.
     *
     * So "Materials\\Default.mat", "./Materials/Default.mat", and
     * "Materials//Default.mat" are the same path, with the same AssetId.
     */
    class AssetPath
    {
    public:
        /** The empty path. Its id is None. */
        AssetPath() = default;

        /** Makes the canonical form of t_Text, and its id. */
        explicit AssetPath(StringView t_Text);

        /** The canonical path text. */
        const String& GetText() const { return m_Text; }

        AssetId GetId() const { return m_Id; }

        bool IsEmpty() const { return m_Text.empty(); }

        /** The canonical form of t_Text. */
        static String Canonicalize(StringView t_Text);

        /**
         * Checks that the case of each part of this path matches the files under t_Root.
         * Windows does not care about case in file names and Linux does, so a path with
         * the wrong case loads on Windows and fails on Linux. Returns false if a part
         * has a different case on disk, and sets t_OutOnDisk to the path with the case of
         * the disk. A part that does not exist is not a mismatch.
         */
        bool MatchesCaseOnDisk(const std::filesystem::path& t_Root, String* t_OutOnDisk = nullptr) const;

    private:
        String m_Text;
        AssetId m_Id;
    };
} // namespace Fling

template <>
struct std::hash<Fling::AssetId>
{
    std::size_t operator()(const Fling::AssetId t_Id) const noexcept
    {
        return static_cast<std::size_t>(t_Id.GetValue());
    }
};

/** Logs an AssetId as its path. */
template <>
struct fmt::formatter<Fling::AssetId> : fmt::formatter<fmt::string_view>
{
    auto format(const Fling::AssetId t_Id, fmt::format_context& t_Context) const
    {
        const Fling::String Text = t_Id.ToString();
        return fmt::formatter<fmt::string_view>::format(fmt::string_view(Text.data(), Text.size()), t_Context);
    }
};

/** Logs an AssetPath as its text. */
template <>
struct fmt::formatter<Fling::AssetPath> : fmt::formatter<fmt::string_view>
{
    auto format(const Fling::AssetPath& t_Path, fmt::format_context& t_Context) const
    {
        const Fling::String& Text = t_Path.GetText();
        return fmt::formatter<fmt::string_view>::format(fmt::string_view(Text.data(), Text.size()), t_Context);
    }
};
