#pragma once

#include "Ecs/Component.h"
#include "Ecs/detail/NativeStorage.h"
#include "FlingTypes.h"
#include "Strings/String.h"

#include <algorithm>
#include <vector>

namespace Fling
{
    /**
     * The Simulation component types that the game thread copies to the main thread's
     * mirror each published tick. The world makes it from Schedule::Published (World::Init);
     * tests build it by hand. Each list is sorted and has no type twice.
     *
     * - Tables: Table, Tag, Enableable, and Sparse types. The mirror gets each value,
     *   and loses the type when the simulation entity does not have it.
     * - Singletons: Singleton types (SimTime). Queues are not published: they are
     *   consumed state, so a frame system that reads one stops World::Init (CanPublish).
     *
     * Presentation types are never published: the frame systems make them on the mirror
     * (a Presentation singleton is made for the system that writes it).
     *
     * - Blended: the Table types a frame system reads with ReadBlend<T>
     *   (PublishedType::IsBlended), a subset of Tables. Each packet carries them for the
     *   latest two ticks (TickPacket::Previous), and the frame side blends between those
     *   (FrameBlend).
     */
    struct PublishedSet
    {
        std::vector<Detail::ComponentId> Tables;
        std::vector<Detail::ComponentId> Singletons;
        std::vector<Detail::ComponentId> Blended;

        /** Add T, and for a Table type every type in its Requires list, to the list its kind belongs to. */
        template <typename T>
        PublishedSet& Add()
        {
            static_assert(Component<T>, "Only Fling::Component types are published.");
            static_assert(ComponentInfoOf<T>.Kind != ComponentKind::Queue,
                          "A queue is consumed state and is not published. Publish a summary that a fixed system "
                          "writes, or send it through the outbox.");
            static_assert(std::is_copy_constructible_v<T> && std::is_copy_assignable_v<T>,
                          "A published type is copied to the mirror storage, so it must be copyable.");
            if constexpr (IsSingletonComponent<T>)
            {
                Insert(Singletons, Detail::ComponentIdOf<T>());
            }
            else
            {
                Insert(Tables, Detail::ComponentIdOf<T>());
                AddRequired(RequiredOf<T>{});
            }
            return *this;
        }

        /** Add T (Add<T>), and hold its values per tick for a blend too. */
        template <typename T>
        PublishedSet& AddBlended()
        {
            static_assert(Component<T> && ComponentInfoOf<T>.Kind == ComponentKind::Table,
                          "Only a Table type is held for a blend.");
            Add<T>();
            Insert(Blended, Detail::ComponentIdOf<T>());
            return *this;
        }

        /** Hold a Table type that is in Tables already for a blend (by id, for the schedule). */
        void KeepBlendedById(const Detail::ComponentId id)
        {
            if (std::binary_search(Tables.begin(), Tables.end(), id))
            {
                Insert(Blended, id);
            }
        }

        /** Add a type by id (the schedule has ids, not types). Returns false, and adds nothing, if it cannot be
         * published. */
        bool AddById(const Detail::ComponentId id, String* error = nullptr)
        {
            if (!CanPublish(id, error))
            {
                return false;
            }
            const Detail::ComponentOps& ops = Detail::GetComponentOps(id);
            Insert(ops.Kind == ComponentKind::Singleton ? Singletons : Tables, id);
            return true;
        }

        /**
         * True if values of the type can be copied to another storage: it is registered,
         * copyable, and not a queue. The reason goes to error when it is not.
         */
        static bool CanPublish(const Detail::ComponentId id, String* error = nullptr)
        {
            if (id >= Detail::MaxComponentTypes ||
                Detail::g_ComponentOps[id].load(std::memory_order_acquire) == nullptr)
            {
                SetError(error, "the type is not registered");
                return false;
            }
            const Detail::ComponentOps& ops = Detail::GetComponentOps(id);
            if (ops.Kind == ComponentKind::Queue)
            {
                SetError(error, String(ops.Name) + " is a queue: queues are consumed state and are not published");
                return false;
            }
            if (!ops.Copyable())
            {
                SetError(error,
                         String(ops.Name) + " cannot be copied: make it copyable, or publish a copyable "
                                            "summary that a fixed system writes");
                return false;
            }
            return true;
        }

        /** The bytes of one value of the type in a packet (0 for a tag), for the cost model. */
        static size_t ValueBytes(const Detail::ComponentId id) { return Detail::GetComponentOps(id).Size; }

        bool Contains(const Detail::ComponentId id) const
        {
            return std::binary_search(Tables.begin(), Tables.end(), id) ||
                   std::binary_search(Singletons.begin(), Singletons.end(), id);
        }

    private:
        static void Insert(std::vector<Detail::ComponentId>& list, const Detail::ComponentId id)
        {
            const auto at = std::lower_bound(list.begin(), list.end(), id);
            if (at == list.end() || *at != id)
            {
                list.insert(at, id);
            }
        }

        template <typename... Rs>
        void AddRequired(TypeList<Rs...>)
        {
            (Add<Rs>(), ...);
        }

        static void SetError(String* error, const StringView text)
        {
            if (error != nullptr)
            {
                *error = String(text);
            }
        }
    };
} // namespace Fling
