#include "Profiler.h"

#if WITH_PROFILER

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <memory>
#include <mutex>
#include <vector>

namespace Fling
{
    namespace
    {
        /** One scope's numbers on one thread. One writer (the thread), so a load and a store is enough. */
        struct Slot
        {
            std::atomic<int64> TotalNs{0};
            std::atomic<int64> MaxNs{0};
            std::atomic<uint64> Calls{0};
        };

        struct ThreadTable
        {
            std::array<Slot, Profiler::MaxScopes> Slots;
        };

        std::atomic<bool> g_Enabled{false};
        std::mutex g_Mutex;
        std::vector<const char*> g_Names;
        std::vector<std::unique_ptr<ThreadTable>> g_Tables;

        ThreadTable& LocalTable()
        {
            thread_local ThreadTable* Table = nullptr;
            if (Table == nullptr)
            {
                auto Made = std::make_unique<ThreadTable>();
                Table = Made.get();
                std::lock_guard<std::mutex> Lock(g_Mutex);
                g_Tables.push_back(std::move(Made));
            }
            return *Table;
        }
    } // namespace

    bool Profiler::IsEnabled()
    {
        return g_Enabled.load(std::memory_order_relaxed);
    }

    void Profiler::SetEnabled(const bool t_On)
    {
        g_Enabled.store(t_On, std::memory_order_relaxed);
    }

    uint32 Profiler::Register(const char* t_Name)
    {
        std::lock_guard<std::mutex> Lock(g_Mutex);
        for (uint32 i = 0; i < g_Names.size(); ++i)
        {
            if (std::strcmp(g_Names[i], t_Name) == 0)
            {
                return i;
            }
        }
        if (g_Names.size() >= MaxScopes)
        {
            return MaxScopes - 1;
        }
        g_Names.push_back(t_Name);
        return static_cast<uint32>(g_Names.size() - 1);
    }

    void Profiler::Record(const uint32 t_Id, const int64 t_Nanoseconds)
    {
        Slot& S = LocalTable().Slots[t_Id];
        S.TotalNs.store(S.TotalNs.load(std::memory_order_relaxed) + t_Nanoseconds, std::memory_order_relaxed);
        S.Calls.store(S.Calls.load(std::memory_order_relaxed) + 1, std::memory_order_relaxed);
        if (t_Nanoseconds > S.MaxNs.load(std::memory_order_relaxed))
        {
            S.MaxNs.store(t_Nanoseconds, std::memory_order_relaxed);
        }
    }

    void Profiler::Reset()
    {
        std::lock_guard<std::mutex> Lock(g_Mutex);
        for (auto& Table : g_Tables)
        {
            for (Slot& S : Table->Slots)
            {
                S.TotalNs.store(0, std::memory_order_relaxed);
                S.MaxNs.store(0, std::memory_order_relaxed);
                S.Calls.store(0, std::memory_order_relaxed);
            }
        }
    }

    String Profiler::Report(const double t_WallSeconds)
    {
        struct Row
        {
            const char* Name = nullptr;
            int64 TotalNs = 0;
            int64 MaxNs = 0;
            uint64 Calls = 0;
        };
        std::vector<Row> Rows;
        {
            std::lock_guard<std::mutex> Lock(g_Mutex);
            for (uint32 i = 0; i < g_Names.size(); ++i)
            {
                Row R;
                R.Name = g_Names[i];
                for (const auto& Table : g_Tables)
                {
                    const Slot& S = Table->Slots[i];
                    R.TotalNs += S.TotalNs.load(std::memory_order_relaxed);
                    R.MaxNs = std::max<int64>(R.MaxNs, S.MaxNs.load(std::memory_order_relaxed));
                    R.Calls += S.Calls.load(std::memory_order_relaxed);
                }
                if (R.Calls > 0)
                {
                    Rows.push_back(R);
                }
            }
        }
        std::sort(Rows.begin(), Rows.end(), [](const Row& A, const Row& B) { return A.TotalNs > B.TotalNs; });

        String Out = "Profile (ms; times add up across threads)\n";
        char Line[256];
        std::snprintf(
            Line, sizeof(Line), "%-40s %10s %10s %10s %10s %7s\n", "scope", "calls", "total", "avg", "max", "wall%");
        Out += Line;
        for (const Row& R : Rows)
        {
            const double TotalMs = static_cast<double>(R.TotalNs) * 1e-6;
            const double Share = t_WallSeconds > 0.0 ? TotalMs / (t_WallSeconds * 1000.0) * 100.0 : 0.0;
            std::snprintf(Line,
                          sizeof(Line),
                          "%-40s %10llu %10.2f %10.4f %10.4f %6.1f%%\n",
                          R.Name,
                          static_cast<unsigned long long>(R.Calls),
                          TotalMs,
                          TotalMs / static_cast<double>(R.Calls),
                          static_cast<double>(R.MaxNs) * 1e-6,
                          Share);
            Out += Line;
        }
        return Out;
    }
} // namespace Fling

#endif // WITH_PROFILER
