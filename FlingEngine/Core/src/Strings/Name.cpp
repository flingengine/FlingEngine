#include "FlingIntegers.h"
#include "Strings/Name.h"
#include "Threading/Mutex.h"
#include "Threading/ThreadSafety.h"

#include <array>
#include <atomic>
#include <cinttypes>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <unordered_map>
#include <vector>

namespace Fling
{
    namespace
    {
        /** The hash is already well mixed, so the map uses it as it is. */
        struct IdentityHash
        {
            std::size_t operator()(const uint64 t_Hash) const noexcept
            {
                return static_cast<std::size_t>(t_Hash ^ (t_Hash >> 32));
            }
        };

        /**
         * Stores each text once, in blocks that never move, so a view of a stored text
         * stays valid until the process exits.
         */
        class TextArena
        {
        public:
            StringView Store(const StringView t_Text)
            {
                ScopedLock Lock(m_Mutex);
                if (t_Text.size() > BlockSize - m_Used || m_Blocks.empty())
                {
                    const std::size_t Size = t_Text.size() > BlockSize ? t_Text.size() : BlockSize;
                    m_Blocks.push_back(std::make_unique<char[]>(Size));
                    m_Reserved += Size;
                    m_Used = t_Text.size() > BlockSize ? BlockSize : 0;
                    if (t_Text.size() > BlockSize)
                    {
                        // A text larger than a block gets its own block. The current
                        // block is full, so the next text starts a new block.
                        char* const Dest = m_Blocks.back().get();
                        std::memcpy(Dest, t_Text.data(), t_Text.size());
                        return StringView(Dest, t_Text.size());
                    }
                }

                char* const Dest = m_Blocks.back().get() + m_Used;
                std::memcpy(Dest, t_Text.data(), t_Text.size());
                m_Used += t_Text.size();
                return StringView(Dest, t_Text.size());
            }

            std::size_t GetReservedBytes()
            {
                ScopedLock Lock(m_Mutex);
                return m_Reserved;
            }

        private:
            static constexpr std::size_t BlockSize = 64 * 1024;

            Mutex m_Mutex;
            std::vector<std::unique_ptr<char[]>> m_Blocks FLING_GUARDED_BY(m_Mutex);
            std::size_t m_Used FLING_GUARDED_BY(m_Mutex) = 0;
            std::size_t m_Reserved FLING_GUARDED_BY(m_Mutex) = 0;
        };

        struct NameShard
        {
            SharedMutex Mutex;
            std::unordered_map<uint64, StringView, IdentityHash> Texts FLING_GUARDED_BY(Mutex);
        };

        /**
         * The process-wide table from hash to text. It is split into shards by hash,
         * so that threads that add different names rarely wait for each other.
         */
        class NameTable
        {
        public:
            Detail::NameRegisterResult Register(const uint64 t_Hash, const StringView t_Text)
            {
                if (t_Hash == 0)
                {
                    // 0 is None. A text that hashes to 0 cannot have a Name.
                    return Detail::NameRegisterResult::Collision;
                }

                NameShard& Shard = GetShard(t_Hash);
                {
                    ReadScopedLock Lock(Shard.Mutex);
                    const auto Found = Shard.Texts.find(t_Hash);
                    if (Found != Shard.Texts.end())
                    {
                        return Found->second == t_Text ? Detail::NameRegisterResult::AlreadyPresent
                                                       : Detail::NameRegisterResult::Collision;
                    }
                }

                WriteScopedLock Lock(Shard.Mutex);
                const auto Found = Shard.Texts.find(t_Hash);
                if (Found != Shard.Texts.end())
                {
                    return Found->second == t_Text ? Detail::NameRegisterResult::AlreadyPresent
                                                   : Detail::NameRegisterResult::Collision;
                }

                Shard.Texts.emplace(t_Hash, m_Arena.Store(t_Text));
                m_Count.fetch_add(1, std::memory_order_relaxed);
                m_TextBytes.fetch_add(t_Text.size(), std::memory_order_relaxed);
                return Detail::NameRegisterResult::Added;
            }

            StringView Find(const uint64 t_Hash)
            {
                NameShard& Shard = GetShard(t_Hash);
                ReadScopedLock Lock(Shard.Mutex);
                const auto Found = Shard.Texts.find(t_Hash);
                return Found != Shard.Texts.end() ? Found->second : StringView();
            }

            NameTableStats GetStats()
            {
                NameTableStats Stats;
                Stats.Count = m_Count.load(std::memory_order_relaxed);
                Stats.TextBytes = m_TextBytes.load(std::memory_order_relaxed);
                Stats.ReservedBytes = m_Arena.GetReservedBytes();
                return Stats;
            }

        private:
            static constexpr std::size_t ShardCount = 64;

            NameShard& GetShard(const uint64 t_Hash) { return m_Shards[t_Hash >> 58]; }

            std::array<NameShard, ShardCount> m_Shards;
            TextArena m_Arena;
            std::atomic<std::size_t> m_Count{0};
            std::atomic<std::size_t> m_TextBytes{0};
        };

        /**
         * The table. It is made on first use, so names work in static initializers in
         * any order. It is never destroyed, so names also work in static destructors.
         */
        NameTable& GetTable()
        {
            static NameTable* const Table = new NameTable();
            return *Table;
        }
    } // namespace

    namespace Detail
    {
        NameRegisterResult RegisterNameText(const uint64 t_Hash, const StringView t_Text)
        {
            return GetTable().Register(t_Hash, t_Text);
        }

        StringView FindNameText(const uint64 t_Hash)
        {
            return GetTable().Find(t_Hash);
        }

        void ReportNameCollision(const uint64 t_Hash, const StringView t_Text)
        {
            // The logger may not be ready (this can run before main), so write to stderr.
            const StringView Existing = FindNameText(t_Hash);
            std::fprintf(stderr,
                         "Name collision: '%.*s' and '%.*s' have the same hash %016" PRIx64 "\n",
                         static_cast<int>(t_Text.size()),
                         t_Text.data(),
                         static_cast<int>(Existing.size()),
                         Existing.data(),
                         t_Hash);
#ifndef FLING_SHIPPING
            std::abort();
#endif
        }

        bool RegisterLiteralName(const uint64 t_Hash, const StringView t_Text)
        {
            if (RegisterNameText(t_Hash, t_Text) == NameRegisterResult::Collision)
            {
                ReportNameCollision(t_Hash, t_Text);
            }
            return true;
        }
    } // namespace Detail

    NameTableStats GetNameTableStats()
    {
        return GetTable().GetStats();
    }

    String Name::ToString() const
    {
        if (m_Hash == 0)
        {
            return "None";
        }

        const StringView Text = Detail::FindNameText(m_Hash);
        if (!Text.empty())
        {
            return String(Text);
        }

        char Buffer[24] = {};
        std::snprintf(Buffer, sizeof(Buffer), "#%016" PRIx64, m_Hash);
        return Buffer;
    }

    void Name::Register(const StringView t_Text) const
    {
        if (Detail::RegisterNameText(m_Hash, t_Text) == Detail::NameRegisterResult::Collision)
        {
            Detail::ReportNameCollision(m_Hash, t_Text);
        }
    }
} // namespace Fling
