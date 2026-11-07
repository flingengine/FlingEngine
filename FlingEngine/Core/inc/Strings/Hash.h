#pragma once

#include "FlingIntegers.h"
#include "Strings/String.h"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>

namespace Fling
{
    namespace Detail
    {
        inline constexpr uint64 XxPrime1 = 0x9E3779B185EBCA87ull;
        inline constexpr uint64 XxPrime2 = 0xC2B2AE3D27D4EB4Full;
        inline constexpr uint64 XxPrime3 = 0x165667B19E3779F9ull;
        inline constexpr uint64 XxPrime4 = 0x85EBCA77C2B2AE63ull;
        inline constexpr uint64 XxPrime5 = 0x27D4EB2F165667C5ull;

        /** Reads 8 bytes as a little-endian value, on any platform. */
        constexpr uint64 XxRead64(const char* t_Bytes)
        {
            if (!std::is_constant_evaluated() && std::endian::native == std::endian::little)
            {
                uint64 Value = 0;
                std::memcpy(&Value, t_Bytes, sizeof(Value));
                return Value;
            }

            uint64 Value = 0;
            for (int Index = 0; Index < 8; ++Index)
            {
                Value |= static_cast<uint64>(static_cast<unsigned char>(t_Bytes[Index])) << (8 * Index);
            }
            return Value;
        }

        /** Reads 4 bytes as a little-endian value, on any platform. */
        constexpr uint64 XxRead32(const char* t_Bytes)
        {
            if (!std::is_constant_evaluated() && std::endian::native == std::endian::little)
            {
                uint32 Value = 0;
                std::memcpy(&Value, t_Bytes, sizeof(Value));
                return Value;
            }

            uint64 Value = 0;
            for (int Index = 0; Index < 4; ++Index)
            {
                Value |= static_cast<uint64>(static_cast<unsigned char>(t_Bytes[Index])) << (8 * Index);
            }
            return Value;
        }

        constexpr uint64 XxRound(uint64 t_Acc, const uint64 t_Input)
        {
            t_Acc += t_Input * XxPrime2;
            t_Acc = std::rotl(t_Acc, 31);
            return t_Acc * XxPrime1;
        }

        constexpr uint64 XxMergeRound(uint64 t_Acc, const uint64 t_Value)
        {
            t_Acc ^= XxRound(0, t_Value);
            return t_Acc * XxPrime1 + XxPrime4;
        }
    } // namespace Detail

    /**
     * The xxHash64 hash of the bytes of a text, with seed 0.
     *
     * This is the one hash function for identifiers that are saved, sent, or compared
     * between processes. The result is the same on every compiler and platform, and it
     * can be computed at compile time. Never change the algorithm: saved files hold
     * its values.
     */
    constexpr uint64 HashText(const StringView t_Text)
    {
        const char* Bytes = t_Text.data();
        const std::size_t Length = t_Text.size();
        const char* const End = Bytes + Length;

        uint64 Hash = 0;
        if (Length >= 32)
        {
            uint64 V1 = Detail::XxPrime1 + Detail::XxPrime2;
            uint64 V2 = Detail::XxPrime2;
            uint64 V3 = 0;
            uint64 V4 = 0 - Detail::XxPrime1;

            const char* const Limit = End - 32;
            do
            {
                V1 = Detail::XxRound(V1, Detail::XxRead64(Bytes));
                V2 = Detail::XxRound(V2, Detail::XxRead64(Bytes + 8));
                V3 = Detail::XxRound(V3, Detail::XxRead64(Bytes + 16));
                V4 = Detail::XxRound(V4, Detail::XxRead64(Bytes + 24));
                Bytes += 32;
            } while (Bytes <= Limit);

            Hash = std::rotl(V1, 1) + std::rotl(V2, 7) + std::rotl(V3, 12) + std::rotl(V4, 18);
            Hash = Detail::XxMergeRound(Hash, V1);
            Hash = Detail::XxMergeRound(Hash, V2);
            Hash = Detail::XxMergeRound(Hash, V3);
            Hash = Detail::XxMergeRound(Hash, V4);
        }
        else
        {
            Hash = Detail::XxPrime5;
        }

        Hash += static_cast<uint64>(Length);

        while (End - Bytes >= 8)
        {
            Hash ^= Detail::XxRound(0, Detail::XxRead64(Bytes));
            Hash = std::rotl(Hash, 27) * Detail::XxPrime1 + Detail::XxPrime4;
            Bytes += 8;
        }

        if (End - Bytes >= 4)
        {
            Hash ^= Detail::XxRead32(Bytes) * Detail::XxPrime1;
            Hash = std::rotl(Hash, 23) * Detail::XxPrime2 + Detail::XxPrime3;
            Bytes += 4;
        }

        while (Bytes < End)
        {
            Hash ^= static_cast<uint64>(static_cast<unsigned char>(*Bytes)) * Detail::XxPrime5;
            Hash = std::rotl(Hash, 11) * Detail::XxPrime1;
            ++Bytes;
        }

        Hash ^= Hash >> 33;
        Hash *= Detail::XxPrime2;
        Hash ^= Hash >> 29;
        Hash *= Detail::XxPrime3;
        Hash ^= Hash >> 32;
        return Hash;
    }
} // namespace Fling
