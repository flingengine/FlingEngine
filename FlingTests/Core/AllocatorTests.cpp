#include "Allocators/BlockAllocator.h"
#include "Allocators/LinearArena.h"
#include "Allocators/SlotPool.h"
#include "Allocators/VirtualMemory.h"

#include <catch2/catch_all.hpp>
#include <cstdint>
#include <cstring>
#include <limits>
#include <set>
#include <vector>

using namespace Fling;

namespace
{
    bool IsAligned(const void* address, size_t alignment)
    {
        return (reinterpret_cast<uintptr_t>(address) & (alignment - 1)) == 0;
    }
} // namespace

TEST_CASE("VirtualMemory gives aligned pages that can be written", "[allocators]")
{
    const size_t page = VirtualMemory::PageSize();
    REQUIRE(page >= 4096);
    for (const size_t alignment : {page, size_t(64 * 1024), size_t(256 * 1024), VirtualMemory::LargePageBytes})
    {
        void* memory = VirtualMemory::Allocate(3 * page, alignment);
        REQUIRE(memory != nullptr);
        CHECK(IsAligned(memory, alignment));
        std::memset(memory, 0xAB, 3 * page);
        VirtualMemory::Free(memory, 3 * page, alignment);
    }
}

TEST_CASE("VirtualMemory pages are zero, and many aligned blocks can live and die in any order", "[allocators]")
{
    // The aligned path takes more address space than it gives out. Every block must still be usable to its
    // last byte, must not overlap another one, and must go back whole.
    const size_t bytes = 96 * 1024;
    const size_t alignment = 128 * 1024;
    std::vector<unsigned char*> blocks;
    for (int i = 0; i < 64; ++i)
    {
        auto* block = static_cast<unsigned char*>(VirtualMemory::Allocate(bytes, alignment));
        REQUIRE(block != nullptr);
        REQUIRE(IsAligned(block, alignment));
        REQUIRE(block[0] == 0);
        REQUIRE(block[bytes - 1] == 0);
        std::memset(block, i + 1, bytes);
        blocks.push_back(block);
    }
    for (size_t i = 0; i < blocks.size(); ++i)
    {
        REQUIRE(blocks[i][0] == i + 1);
        REQUIRE(blocks[i][bytes - 1] == i + 1);
    }
    // Every other one first, then the rest
    for (size_t i = 0; i < blocks.size(); i += 2)
    {
        VirtualMemory::Free(blocks[i], bytes, alignment);
    }
    for (size_t i = 1; i < blocks.size(); i += 2)
    {
        REQUIRE(blocks[i][bytes / 2] == i + 1);
        VirtualMemory::Free(blocks[i], bytes, alignment);
    }

    // A size that cannot be is refused, not wrapped around to a small one
    REQUIRE(VirtualMemory::Allocate(std::numeric_limits<size_t>::max() - 100, alignment) == nullptr);
}

TEST_CASE("BlockAllocator rounds to size classes and aligns blocks to their size", "[allocators]")
{
    CHECK(BlockAllocator::BlockSize(1) == BlockAllocator::MinBlockBytes);
    CHECK(BlockAllocator::BlockSize(1024) == 1024);
    CHECK(BlockAllocator::BlockSize(1025) == 2048);
    CHECK(BlockAllocator::BlockSize(BlockAllocator::MaxBlockBytes) == BlockAllocator::MaxBlockBytes);
    CHECK(BlockAllocator::BlockSize(BlockAllocator::MaxBlockBytes + 1) > BlockAllocator::MaxBlockBytes);

    BlockAllocator blocks;
    std::vector<std::pair<void*, size_t>> live;
    for (size_t bytes = 100; bytes <= 300 * 1024; bytes = bytes * 3 / 2)
    {
        void* block = blocks.Allocate(bytes);
        REQUIRE(block != nullptr);
        CHECK(IsAligned(block, BlockAllocator::BlockAlignment(bytes)));
        std::memset(block, 0xCD, bytes);
        live.emplace_back(block, bytes);
    }

    // No two live blocks overlap.
    for (size_t i = 0; i < live.size(); ++i)
    {
        const auto a = reinterpret_cast<uintptr_t>(live[i].first);
        for (size_t j = i + 1; j < live.size(); ++j)
        {
            const auto b = reinterpret_cast<uintptr_t>(live[j].first);
            const bool apart = a + BlockAllocator::BlockSize(live[i].second) <= b ||
                               b + BlockAllocator::BlockSize(live[j].second) <= a;
            CHECK(apart);
        }
    }

    CHECK(blocks.GetStats().LargeBlocks > 0);
    for (const auto& [block, bytes] : live)
    {
        blocks.Free(block, bytes);
    }
    CHECK(blocks.GetStats().UsedBytes == 0);
    CHECK(blocks.GetStats().LargeBlocks == 0);
}

TEST_CASE("BlockAllocator uses a freed block again, and Trim gives empty slabs back", "[allocators]")
{
    BlockAllocator blocks;
    void* first = blocks.Allocate(4096);
    blocks.Free(first, 4096);
    void* second = blocks.Allocate(4096);
    CHECK(second == first);

    // Fill more than one slab of 1 KiB blocks, then free them all.
    std::vector<void*> many;
    for (int i = 0; i < 200; ++i)
    {
        many.push_back(blocks.Allocate(1000));
    }
    const std::set<void*> unique(many.begin(), many.end());
    CHECK(unique.size() == many.size());
    const BlockAllocator::Stats full = blocks.GetStats();
    CHECK(full.Slabs >= 3);
    CHECK(full.UsedBytes == 200 * 1024 + 4096);

    for (void* block : many)
    {
        blocks.Free(block, 1000);
    }
    // Pages stay until Trim.
    CHECK(blocks.GetStats().ReservedBytes == full.ReservedBytes);
    CHECK(blocks.Trim() > 0);
    const BlockAllocator::Stats trimmed = blocks.GetStats();
    CHECK(trimmed.Slabs == 1); // The slab with the 4 KiB block that is still in use
    CHECK(trimmed.UsedBytes == 4096);

    blocks.Free(second, 4096);
    blocks.Trim();
    CHECK(blocks.GetStats().ReservedBytes == 0);
}

TEST_CASE("LinearArena aligns, keeps its pages on Reset, and takes big requests", "[allocators]")
{
    LinearArena arena;
    void* a = arena.Allocate(3, 1);
    void* b = arena.Allocate(16, 64);
    CHECK(IsAligned(b, 64));
    CHECK(b != a);
    void* big = arena.Allocate(LinearArena::PageBytes * 2, 16);
    REQUIRE(big != nullptr);
    std::memset(big, 0, LinearArena::PageBytes * 2);

    const size_t reserved = arena.ReservedBytes();
    CHECK(reserved >= LinearArena::PageBytes * 3);
    arena.Reset();
    CHECK(arena.UsedBytes() == 0);
    CHECK(arena.ReservedBytes() == reserved);

    // After Reset the same requests fit in the pages it kept.
    arena.Allocate(3, 1);
    arena.Allocate(16, 64);
    arena.Allocate(LinearArena::PageBytes * 2, 16);
    CHECK(arena.ReservedBytes() == reserved);

    LinearArena moved(std::move(arena));
    CHECK(moved.ReservedBytes() == reserved);
    moved.Release();
    CHECK(moved.ReservedBytes() == 0);
}

TEST_CASE("LinearArena takes an alignment that is more than a page, and gives the page back", "[allocators]")
{
    LinearArena arena;
    const size_t alignment = 256 * 1024;
    auto* wide = static_cast<unsigned char*>(arena.Allocate(1000, alignment));
    REQUIRE(IsAligned(wide, alignment));
    std::memset(wide, 0x5A, 1000);
    // A small request after it does not move it
    auto* small = static_cast<unsigned char*>(arena.Allocate(8, 8));
    REQUIRE((small < wide || small >= wide + 1000));
    REQUIRE(wide[999] == 0x5A);
    arena.Release();
    REQUIRE(arena.ReservedBytes() == 0);
}

TEST_CASE("SlotPool gives aligned, stable slots and uses freed ones again", "[allocators]")
{
    BlockAllocator blocks;
    SlotPool pool(blocks, 24, 16);
    std::vector<void*> slots;
    for (int i = 0; i < 2000; ++i)
    {
        void* slot = pool.Allocate();
        CHECK(IsAligned(slot, 16));
        std::memset(slot, i & 0xFF, 24);
        slots.push_back(slot);
    }
    CHECK(pool.UsedSlots() == 2000);
    const std::set<void*> unique(slots.begin(), slots.end());
    CHECK(unique.size() == slots.size());

    const size_t reserved = blocks.GetStats().ReservedBytes;
    void* freed = slots[1234];
    pool.Free(freed);
    CHECK(pool.UsedSlots() == 1999);
    CHECK(pool.Allocate() == freed);
    CHECK(blocks.GetStats().ReservedBytes == reserved);
}
