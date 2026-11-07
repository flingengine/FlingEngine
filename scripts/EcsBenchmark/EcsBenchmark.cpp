// ECS benchmark: the same workloads on EnTT 3.1 (BACKEND_ENTT) or Fling's native storage. See run.sh.
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <malloc.h>
#include <new>
#include <string>
#include <vector>

// ---- Heap accounting: live bytes through every operator new/delete ----
static size_t g_Live = 0;
static void* Track(void* p) { if (p) g_Live += malloc_usable_size(p); return p; }
static void Untrack(void* p) { if (p) g_Live -= malloc_usable_size(p); }
void* operator new(size_t n) { void* p = Track(std::malloc(n ? n : 1)); if (!p) throw std::bad_alloc(); return p; }
void* operator new[](size_t n) { return operator new(n); }
void* operator new(size_t n, std::align_val_t a) { void* p = nullptr; if (posix_memalign(&p, std::max<size_t>(sizeof(void*), (size_t)a), n ? n : 1)) throw std::bad_alloc(); return Track(p); }
void* operator new[](size_t n, std::align_val_t a) { return operator new(n, a); }
void operator delete(void* p) noexcept { Untrack(p); std::free(p); }
void operator delete[](void* p) noexcept { operator delete(p); }
void operator delete(void* p, size_t) noexcept { operator delete(p); }
void operator delete[](void* p, size_t) noexcept { operator delete(p); }
void operator delete(void* p, std::align_val_t) noexcept { operator delete(p); }
void operator delete[](void* p, std::align_val_t) noexcept { operator delete(p); }
void operator delete(void* p, size_t, std::align_val_t) noexcept { operator delete(p); }
void operator delete[](void* p, size_t, std::align_val_t) noexcept { operator delete(p); }

#if BACKEND_ENTT
#include <entt/entity/registry.hpp>
#define COMPONENT_INFO(name, ...)
#else
#include "Ecs/EntityStorage.h"
#include "Ecs/SystemContext.h"
#define COMPONENT_INFO(name, ...) static constexpr Fling::ComponentInfo Component{.Name = name, ##__VA_ARGS__};
#endif

struct Position { COMPONENT_INFO("Bench.Position") float X = 0, Y = 0, Z = 0; };
struct Velocity { COMPONENT_INFO("Bench.Velocity") float X = 1, Y = 1, Z = 1; };
struct Health { COMPONENT_INFO("Bench.Health") int Value = 100; };
struct TagA { COMPONENT_INFO("Bench.TagA", .Kind = Fling::ComponentKind::Tag) };
struct TagB { COMPONENT_INFO("Bench.TagB", .Kind = Fling::ComponentKind::Tag) };
struct TagC { COMPONENT_INFO("Bench.TagC", .Kind = Fling::ComponentKind::Tag) };

// ---- Backend adapter ----
#if BACKEND_ENTT
static const char* BackendName = "entt";
using World = entt::registry;
using Ent = entt::entity;
static Ent Create(World& w) { return w.create(); }
static void Destroy(World& w, Ent e) { w.destroy(e); }
template <typename T, typename... A> static void Add(World& w, Ent e, A&&... a) { w.assign<T>(e, std::forward<A>(a)...); }
template <typename T> static void Remove(World& w, Ent e) { w.remove<T>(e); }
template <typename Fn> static void Each1(World& w, Fn fn) { w.view<Position>().each([&](Position& p) { fn(p); }); }
template <typename Fn> static void Each2(World& w, Fn fn) { w.view<Position, Velocity>().each([&](Position& p, Velocity& v) { fn(p, v); }); }
static void Reserve(World& w, size_t n) { w.reserve<Position>(n); w.reserve<Velocity>(n); w.reserve(n); }
#else
#if FLING_RANGE_FOR
static const char* BackendName = "fling-rangefor";
#else
static const char* BackendName = "fling-each";
#endif
using World = Fling::EntityStorage;
using Ent = Fling::Entity;
static Ent Create(World& w) { return w.Create(); }
static void Destroy(World& w, Ent e) { w.Destroy(e); }
template <typename T, typename... A> static void Add(World& w, Ent e, A&&... a) { w.Add<T>(e, std::forward<A>(a)...); }
template <typename T> static void Remove(World& w, Ent e) { w.Remove<T>(e); }
// The path that systems use: QueryRange walks chunks over raw column pointers
#if FLING_RANGE_FOR
template <typename Fn> static void Each1(World& w, Fn fn)
{
    for (auto [e, p] : Fling::QueryRange<Fling::TypeList<Position>, Fling::TypeList<>, Fling::TypeList<>>(w.Native())) { (void)e; fn(p); }
}
template <typename Fn> static void Each2(World& w, Fn fn)
{
    for (auto [e, p, v] : Fling::QueryRange<Fling::TypeList<Position, Velocity>, Fling::TypeList<>, Fling::TypeList<>>(w.Native())) { (void)e; fn(p, v); }
}
#else
template <typename Fn> static void Each1(World& w, Fn fn)
{
    Fling::QueryRange<Fling::TypeList<Position>, Fling::TypeList<>, Fling::TypeList<>>(w.Native()).Each([&](Position& p) { fn(p); });
}
template <typename Fn> static void Each2(World& w, Fn fn)
{
    Fling::QueryRange<Fling::TypeList<Position, Velocity>, Fling::TypeList<>, Fling::TypeList<>>(w.Native()).Each([&](Position& p, Velocity& v) { fn(p, v); });
}
#endif
static void Reserve(World&, size_t) {}
#endif

using Clock = std::chrono::steady_clock;
static double NsPerEntity(Clock::time_point a, Clock::time_point b, size_t n)
{
    return std::chrono::duration<double, std::nano>(b - a).count() / double(n);
}

static volatile float g_Sink = 0;

int main(int argc, char** argv)
{
    const size_t N = argc > 1 ? std::strtoull(argv[1], nullptr, 10) : 1000000;
    const int Reps = argc > 2 ? std::atoi(argv[2]) : 7;

    // Median of Reps for each workload
    std::vector<double> create, iter1, iter2, iterFrag, add, remove, destroy;
    double bytesPerEntity = 0, bytesAfterChurn = 0;

    for (int rep = 0; rep < Reps; ++rep)
    {
        const size_t liveBefore = g_Live;
        World* w = new World();
        std::vector<Ent> ents(N);

        auto t0 = Clock::now();
        for (size_t i = 0; i < N; ++i)
        {
            ents[i] = Create(*w);
            Add<Position>(*w, ents[i], Position{float(i), 0, 0});
            Add<Velocity>(*w, ents[i]);
        }
        auto t1 = Clock::now();
        create.push_back(NsPerEntity(t0, t1, N));
        bytesPerEntity = double(g_Live - liveBefore - N * sizeof(Ent)) / double(N);

        t0 = Clock::now();
        float sum = 0;
        Each1(*w, [&](Position& p) { sum += p.X; });
        t1 = Clock::now();
        g_Sink = sum;
        iter1.push_back(NsPerEntity(t0, t1, N));

        t0 = Clock::now();
        Each2(*w, [&](Position& p, Velocity& v) { p.X += v.X * 0.016f; p.Y += v.Y * 0.016f; p.Z += v.Z * 0.016f; });
        t1 = Clock::now();
        iter2.push_back(NsPerEntity(t0, t1, N));

        t0 = Clock::now();
        for (size_t i = 0; i < N; ++i) { Add<Health>(*w, ents[i]); }
        t1 = Clock::now();
        add.push_back(NsPerEntity(t0, t1, N));

        t0 = Clock::now();
        for (size_t i = 0; i < N; ++i) { Remove<Health>(*w, ents[i]); }
        t1 = Clock::now();
        remove.push_back(NsPerEntity(t0, t1, N));
        bytesAfterChurn = double(g_Live - liveBefore - N * sizeof(Ent)) / double(N);

        // Fragment: tags split the entities over 8 archetypes (EnTT: 3 more pools)
        for (size_t i = 0; i < N; ++i)
        {
            if (i & 1) Add<TagA>(*w, ents[i]);
            if (i & 2) Add<TagB>(*w, ents[i]);
            if (i & 4) Add<TagC>(*w, ents[i]);
        }
        t0 = Clock::now();
        Each2(*w, [&](Position& p, Velocity& v) { p.X += v.X * 0.016f; p.Y += v.Y * 0.016f; p.Z += v.Z * 0.016f; });
        t1 = Clock::now();
        iterFrag.push_back(NsPerEntity(t0, t1, N));

        t0 = Clock::now();
        for (size_t i = 0; i < N; ++i) { Destroy(*w, ents[i]); }
        t1 = Clock::now();
        destroy.push_back(NsPerEntity(t0, t1, N));

        delete w;
    }

    auto median = [](std::vector<double> v) { std::sort(v.begin(), v.end()); return v[v.size() / 2]; };
    std::printf("%s N=%zu  create %.2f  iter1 %.3f  iter2 %.3f  iter2frag %.3f  add %.2f  remove %.2f  destroy %.2f  (ns/entity)  heap %.1f B/entity  after-churn %.1f B/entity\n",
                BackendName, N, median(create), median(iter1), median(iter2), median(iterFrag), median(add), median(remove), median(destroy),
                bytesPerEntity, bytesAfterChurn);
    return 0;
}
