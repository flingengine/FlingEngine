#include "Ecs/Publish/TickPacket.h"
#include "MathUtils.h"

#include <cassert>
#include <cstring>
#include <new>
#include <utility>

namespace Fling
{
    namespace
    {
        constexpr size_t MinColumnCapacity = 16;

        std::align_val_t BufferAlignment(const Detail::ComponentOps& ops)
        {
            return std::align_val_t{Math::Max(ops.Align, alignof(std::max_align_t))};
        }
    } // namespace

    PacketColumn::PacketColumn(PacketColumn&& other) noexcept
    {
        *this = std::move(other);
    }

    PacketColumn& PacketColumn::operator=(PacketColumn&& other) noexcept
    {
        if (this != &other)
        {
            Release();
            m_Ops = std::exchange(other.m_Ops, nullptr);
            m_Id = std::exchange(other.m_Id, Detail::InvalidComponentId);
            m_Size = std::exchange(other.m_Size, 0);
            m_Data = std::exchange(other.m_Data, nullptr);
            m_Count = std::exchange(other.m_Count, 0);
            m_Constructed = std::exchange(other.m_Constructed, 0);
            m_Capacity = std::exchange(other.m_Capacity, 0);
            m_Reallocations = std::exchange(other.m_Reallocations, 0);
        }
        return *this;
    }

    PacketColumn::~PacketColumn()
    {
        Release();
    }

    void PacketColumn::Release()
    {
        if (m_Data == nullptr)
        {
            return;
        }
        if (!m_Ops->Trivial)
        {
            for (size_t i = 0; i < m_Constructed; ++i)
            {
                m_Ops->Destroy(m_Data + i * m_Size);
            }
        }
        ::operator delete(m_Data, BufferAlignment(*m_Ops));
        m_Data = nullptr;
        m_Count = 0;
        m_Constructed = 0;
        m_Capacity = 0;
    }

    void PacketColumn::Begin(const Detail::ComponentId id)
    {
        if (id != m_Id)
        {
            Release();
            m_Id = id;
            m_Ops = &Detail::GetComponentOps(id);
            m_Size = m_Ops->Size;
            assert(m_Ops->Copyable() && "A published type must be copyable");
        }
        m_Count = 0;
    }

    void PacketColumn::Reserve(const size_t count)
    {
        if (count <= m_Capacity)
        {
            return;
        }
        const size_t capacity = Math::Max(count, Math::Max(m_Capacity * 2, MinColumnCapacity));
        auto* data = static_cast<std::byte*>(::operator new(capacity * m_Size, BufferAlignment(*m_Ops)));
        if (m_Data != nullptr)
        {
            if (m_Ops->Trivial)
            {
                std::memcpy(data, m_Data, m_Count * m_Size);
            }
            else
            {
                for (size_t i = 0; i < m_Constructed; ++i)
                {
                    m_Ops->MoveConstruct(data + i * m_Size, m_Data + i * m_Size);
                    m_Ops->Destroy(m_Data + i * m_Size);
                }
            }
            ::operator delete(m_Data, BufferAlignment(*m_Ops));
        }
        m_Data = data;
        m_Capacity = capacity;
        ++m_Reallocations;
    }

    void PacketColumn::Append(const std::byte* src, const size_t count)
    {
        assert(m_Ops != nullptr && m_Size != 0 && "Append before Begin, or for a type with no data");
        Reserve(m_Count + count);
        std::byte* dst = m_Data + m_Count * m_Size;
        if (m_Ops->Trivial)
        {
            std::memcpy(dst, src, count * m_Size);
        }
        else
        {
            for (size_t i = 0; i < count; ++i)
            {
                const size_t index = m_Count + i;
                if (index < m_Constructed)
                {
                    // Into the value that is there, so a string or vector keeps its capacity
                    m_Ops->CopyAssign(dst + i * m_Size, src + i * m_Size);
                }
                else
                {
                    m_Ops->CopyConstruct(dst + i * m_Size, src + i * m_Size);
                    ++m_Constructed;
                }
            }
        }
        m_Count += count;
    }

    size_t TickPacket::PayloadBytes() const
    {
        size_t bytes = sizeof(TickPacketHeader) + Alive.size() * sizeof(Entity);
        const auto addTables = [&bytes](const std::vector<PacketTable>& tables)
        {
            for (const PacketTable& table : tables)
            {
                bytes += table.Rows.size() * sizeof(Entity) + table.Values.Bytes() + table.Enabled.size();
            }
        };
        addTables(Tables);
        addTables(Previous);
        for (const PacketSingleton& singleton : Singletons)
        {
            bytes += singleton.Value.Bytes();
        }
        return bytes;
    }

    size_t TickPacket::CapacityBytes() const
    {
        size_t bytes = sizeof(TickPacket) + Alive.capacity() * sizeof(Entity);
        const auto addTables = [&bytes](const std::vector<PacketTable>& tables)
        {
            bytes += tables.capacity() * sizeof(PacketTable);
            for (const PacketTable& table : tables)
            {
                bytes +=
                    table.Rows.capacity() * sizeof(Entity) + table.Values.CapacityBytes() + table.Enabled.capacity();
            }
        };
        addTables(Tables);
        addTables(Previous);
        bytes += Singletons.capacity() * sizeof(PacketSingleton);
        for (const PacketSingleton& singleton : Singletons)
        {
            bytes += singleton.Value.CapacityBytes();
        }
        return bytes;
    }

    uint64 TickPacket::ColumnReallocations() const
    {
        uint64 count = 0;
        for (const std::vector<PacketTable>* tables : {&Tables, &Previous})
        {
            for (const PacketTable& table : *tables)
            {
                count += table.Values.Reallocations();
            }
        }
        for (const PacketSingleton& singleton : Singletons)
        {
            count += singleton.Value.Reallocations();
        }
        return count;
    }
} // namespace Fling
