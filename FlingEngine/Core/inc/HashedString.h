#pragma once

#include <cstddef>
#include <cstdint>

namespace Fling
{
    /**
     * A string and its 32-bit FNV-1a hash, made at compile time when the string is a literal.
     *
     * The hash is the same as the one EnTT 3.1 makes, so saved hashes stay valid.
     * The string is not copied: the object keeps the pointer it was given, so the
     * characters must stay alive as long as the object does.
     */
    class HashedString
    {
    public:
        using HashType = std::uint32_t;

        static constexpr HashType Offset = 2166136261u;
        static constexpr HashType Prime = 16777619u;

        /** The hash of a null-terminated string. A null pointer has the hash of an empty string. */
        static constexpr HashType ToValue(const char* Str) noexcept
        {
            HashType Partial = Offset;
            if (Str != nullptr)
            {
                for (; *Str != 0; ++Str)
                {
                    Partial = (Partial ^ static_cast<HashType>(static_cast<unsigned char>(*Str))) * Prime;
                }
            }
            return Partial;
        }

        /** The hash of the first Size characters of Str. */
        static constexpr HashType ToValue(const char* Str, std::size_t Size) noexcept
        {
            HashType Partial = Offset;
            for (std::size_t i = 0; i < Size; ++i)
            {
                Partial = (Partial ^ static_cast<HashType>(static_cast<unsigned char>(Str[i]))) * Prime;
            }
            return Partial;
        }

        constexpr HashedString() noexcept = default;

        /** From a string literal. Not explicit, so a literal converts to a Guid. */
        template <std::size_t N>
        constexpr HashedString(const char (&Str)[N]) noexcept : m_Str{Str}, m_Hash{ToValue(Str)}
        {
        }

        /**
         * Wraps a pointer, so that a literal prefers the array constructor over the
         * pointer constructor.
         */
        struct ConstWrapper
        {
            // Not explicit on purpose: a pointer converts to it.
            constexpr ConstWrapper(const char* InStr) noexcept : Str{InStr} {}

            const char* Str;
        };

        /** From a pointer to a null-terminated string that outlives this object. */
        explicit constexpr HashedString(ConstWrapper Wrapper) noexcept
            : m_Str{Wrapper.Str}, m_Hash{ToValue(Wrapper.Str)}
        {
        }

        constexpr const char* data() const noexcept { return m_Str; }

        constexpr HashType value() const noexcept { return m_Hash; }

        constexpr operator const char*() const noexcept { return m_Str; }

        constexpr operator HashType() const noexcept { return m_Hash; }

        constexpr bool operator==(const HashedString& Other) const noexcept { return m_Hash == Other.m_Hash; }

        constexpr bool operator!=(const HashedString& Other) const noexcept { return m_Hash != Other.m_Hash; }

    private:
        const char* m_Str = nullptr;
        HashType m_Hash = 0;
    };
} // namespace Fling
