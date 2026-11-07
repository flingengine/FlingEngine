#pragma once

#include "CoreAPI.h"
#include "FlingIntegers.h"
#include "spdlog/fmt/fmt.h"
#include "Strings/Hash.h"
#include "Strings/String.h"

#include <compare>
#include <cstddef>
#include <functional>
#include <type_traits>

namespace Fling
{
    namespace Detail
    {
        /** The result of adding a text to the name table. */
        enum class NameRegisterResult : uint8
        {
            /** The text was not in the table, and is now. */
            Added,
            /** The same text was already in the table. */
            AlreadyPresent,
            /** A different text has the same hash. The table keeps the first text. */
            Collision,
        };

        /**
         * Adds the text of a hash to the name table. Name calls this. Tests call it
         * directly to make a collision on purpose.
         */
        CORE_API NameRegisterResult RegisterNameText(uint64 t_Hash, StringView t_Text);

        /** The text of a hash, or an empty view if the table does not have it. */
        CORE_API StringView FindNameText(uint64 t_Hash);

        /** Reports a collision. In builds without FLING_SHIPPING it stops the program. */
        CORE_API void ReportNameCollision(uint64 t_Hash, StringView t_Text);

        /** Adds the text of a literal name, and reports a collision. Returns true. */
        CORE_API bool RegisterLiteralName(uint64 t_Hash, StringView t_Text);

        /** A string literal as a template argument, for operator""_name. */
        template <std::size_t N>
        struct NameLiteral
        {
            char Text[N]{};

            constexpr NameLiteral(const char (&t_Text)[N])
            {
                for (std::size_t Index = 0; Index < N; ++Index)
                {
                    Text[Index] = t_Text[Index];
                }
            }

            constexpr StringView View() const { return StringView(Text, N - 1); }
        };

        /**
         * Adds the text of one literal to the name table at static initialization,
         * before main. operator""_name uses it, so every literal name in the program
         * is in the table before any code runs.
         */
        template <NameLiteral Literal>
        struct NameLiteralRegistrar
        {
            static inline const bool Registered = RegisterLiteralName(HashText(Literal.View()), Literal.View());
        };
    } // namespace Detail

    /** The size of the name table, for memory stats. */
    struct NameTableStats
    {
        /** The number of distinct texts in the table. */
        std::size_t Count = 0;
        /** The bytes of text stored. */
        std::size_t TextBytes = 0;
        /** The bytes of text storage allocated. */
        std::size_t ReservedBytes = 0;
    };

    /** The size of the name table now. */
    CORE_API NameTableStats GetNameTableStats();

    /**
     * An immutable identifier: the 64-bit hash (HashText) of UTF-8 text.
     *
     * A Name holds only its hash, so it is the same value in every process, on every
     * platform, and in every build. Saved files, snapshots, checksums, and network
     * messages can hold it as it is. Compare, order, and hash are integer operations.
     *
     * A process-wide table gives the text of each hash, for logs and the editor.
     * Literal names are added to the table before main. A Name made from text at
     * runtime adds its text when it is made. The hash is never shown to users.
     *
     * Names are case-sensitive and compare the exact bytes. The empty text is None.
     * Use Name for symbols that people write (actions, tags, keys, bones), not for the
     * identity of an object, and not for text that the player sees.
     */
    class Name
    {
    public:
        /** None. Its hash is 0. */
        constexpr Name() = default;

        /**
         * Hashes the text. At runtime, also adds the text to the name table. Do not do
         * this in a tick: make names at load or setup time, or use a literal.
         */
        constexpr explicit Name(const StringView t_Text) : m_Hash(t_Text.empty() ? 0 : HashText(t_Text))
        {
            if (!std::is_constant_evaluated() && m_Hash != 0)
            {
                Register(t_Text);
            }
        }

        /**
         * The Name of t_Text, without adding the text to the name table. Use it to look
         * up text that may not be a name (user input, a key read from a file), so that
         * text that is not found does not stay in the table.
         */
        static constexpr Name WithoutText(const StringView t_Text) { return FromHash(t_Text.empty() ? 0 : HashText(t_Text)); }

        /**
         * The Name of a hash read from binary data. It does not add text: the data
         * that held the hash also holds a table of its texts.
         */
        static constexpr Name FromHash(const uint64 t_Hash)
        {
            Name Result;
            Result.m_Hash = t_Hash;
            return Result;
        }

        constexpr uint64 GetHash() const { return m_Hash; }
        constexpr bool IsNone() const { return m_Hash == 0; }

        /**
         * The text, from the name table. Empty for None, and for a hash whose text is
         * not in the table. The view is valid until the process exits.
         */
        StringView ToView() const { return m_Hash == 0 ? StringView() : Detail::FindNameText(m_Hash); }

        /** The text for logs: "None" for None, and "#<hash>" when the text is not known. */
        CORE_API String ToString() const;

        friend constexpr bool operator==(Name, Name) = default;

        /** Orders by hash: the same on every machine, but not alphabetical. */
        // clang-format off: the repo style is C++17, which does not know operator<=>.
        friend constexpr std::strong_ordering operator<=>(Name, Name) = default;
        // clang-format on

    private:
        CORE_API void Register(StringView t_Text) const;

        uint64 m_Hash = 0;
    };

    inline namespace NameLiterals
    {
        /**
         * A Name made at compile time: "Jump"_name. Its text is added to the name
         * table before main.
         */
        template <Detail::NameLiteral Literal>
        consteval Name operator""_name()
        {
            if constexpr (Literal.View().empty())
            {
                return Name();
            }
            else
            {
                (void)&Detail::NameLiteralRegistrar<Literal>::Registered;
                return Name::FromHash(HashText(Literal.View()));
            }
        }
    } // namespace NameLiterals
} // namespace Fling

template <>
struct std::hash<Fling::Name>
{
    std::size_t operator()(const Fling::Name t_Name) const noexcept
    {
        return static_cast<std::size_t>(t_Name.GetHash());
    }
};

/** Logs a Name as its text. */
template <>
struct fmt::formatter<Fling::Name> : fmt::formatter<fmt::string_view>
{
    auto format(const Fling::Name t_Name, fmt::format_context& t_Context) const
    {
        const Fling::StringView Text = t_Name.ToView();
        if (!Text.empty())
        {
            return fmt::formatter<fmt::string_view>::format(fmt::string_view(Text.data(), Text.size()), t_Context);
        }
        const Fling::String Fallback = t_Name.ToString();
        return fmt::formatter<fmt::string_view>::format(fmt::string_view(Fallback.data(), Fallback.size()), t_Context);
    }
};
