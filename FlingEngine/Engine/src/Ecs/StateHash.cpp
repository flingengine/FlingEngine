#include "Ecs/Reflection.h"
#include "Ecs/StateHash.h"
#include "Ecs/TypeRegistry.h"
#include "Strings/Name.h"
#include "Strings/String.h"

#include <algorithm>
#include <cstring>
#include <vector>

namespace Fling
{
    namespace
    {
        /** FNV-1a over bytes, with a final mix. Order matters, which is what a state hash wants. */
        class Hasher
        {
        public:
            void Bytes(const void* t_Data, const size_t t_Size)
            {
                const unsigned char* Data = static_cast<const unsigned char*>(t_Data);
                for (size_t i = 0; i < t_Size; ++i)
                {
                    m_Value ^= Data[i];
                    m_Value *= 0x100000001B3ull;
                }
            }

            void U64(const uint64 t_Value) { Bytes(&t_Value, sizeof(t_Value)); }

            uint64 Finish() const
            {
                uint64 Value = m_Value;
                Value ^= Value >> 33;
                Value *= 0xFF51AFD7ED558CCDull;
                Value ^= Value >> 33;
                Value *= 0xC4CEB9FE1A85EC53ull;
                Value ^= Value >> 33;
                return Value;
            }

        private:
            uint64 m_Value = 0xCBF29CE484222325ull;
        };

        // The kinds hashed as their whole object have no padding bytes, and their members are in one order on every
        // platform (x, y, z, w). Not glm::quat: see FieldKind::Quat below.
        static_assert(sizeof(glm::vec2) == 2 * sizeof(float) && sizeof(glm::vec4) == 4 * sizeof(float));

        /** Hashes the value of a field, never a padding byte, so two equal values hash the same. */
        void HashField(Hasher& t_Hash, const FieldDesc& t_Field, const void* t_Object)
        {
            const void* Address = t_Field.Address(t_Object);
            switch (t_Field.Kind)
            {
            case FieldKind::Bool:
                t_Hash.U64(*static_cast<const bool*>(Address) ? 1 : 0);
                break;
            case FieldKind::Vec3:
            {
                // By value, never as bytes: x, y and z, whatever sizeof(glm::vec3) is in this build
                const glm::vec3& Value = *static_cast<const glm::vec3*>(Address);
                const float Components[3] = {Value.x, Value.y, Value.z};
                t_Hash.Bytes(Components, sizeof(Components));
                break;
            }
            case FieldKind::Quat:
            {
                // By value, in the order x, y, z, w. GLM stores a quaternion as w, x, y, z when the compiler has
                // Microsoft language extensions (MSVC, clang-cl) and as x, y, z, w otherwise, so its bytes differ.
                const glm::quat& Value = *static_cast<const glm::quat*>(Address);
                const float Components[4] = {Value.x, Value.y, Value.z, Value.w};
                t_Hash.Bytes(Components, sizeof(Components));
                break;
            }
            case FieldKind::Entity:
                t_Hash.U64(static_cast<const Entity*>(Address)->ToBits());
                break;
            case FieldKind::String:
            {
                const String& Text = *static_cast<const String*>(Address);
                t_Hash.U64(Text.size());
                t_Hash.Bytes(Text.data(), Text.size());
                break;
            }
            case FieldKind::Name:
                t_Hash.U64(static_cast<const Name*>(Address)->GetHash());
                break;
            case FieldKind::Custom:
            {
                // Its saved form: the only view of the value that reflection has
                Json Saved = Json::Object();
                t_Field.CustomSave(Address, Saved, t_Field.Name);
                const String Text = Saved.Dump(-1);
                t_Hash.U64(Text.size());
                t_Hash.Bytes(Text.data(), Text.size());
                break;
            }
            default:
                // Integers, floats, enums, vec2 and vec4: every byte is part of the value
                t_Hash.Bytes(Address, t_Field.Size);
                break;
            }
        }

        void HashObject(Hasher& t_Hash, const TypeDesc& t_Type, const void* t_Object)
        {
            // Every field: Transient says only that a level does not save it
            for (const FieldDesc& Field : t_Type.Fields)
            {
                HashField(t_Hash, Field, t_Object);
            }
        }
    } // namespace

    uint64 HashSimulationState(const EntityStorage& t_Storage, const TypeRegistry& t_Types)
    {
        std::vector<Entity> Entities;
        Entities.reserve(t_Storage.EntityCount());
        t_Storage.EachEntity([&](const Entity t_Entity) { Entities.push_back(t_Entity); });
        std::sort(Entities.begin(), Entities.end());

        std::vector<const TypeDesc*> Types;
        for (const TypeDesc& Type : t_Types.All())
        {
            if (Type.IsRollbackState())
            {
                Types.push_back(&Type);
            }
        }
        std::sort(Types.begin(), Types.end(), [](const TypeDesc* a, const TypeDesc* b) { return a->Hash < b->Hash; });

        Hasher Hash;
        for (const TypeDesc* Type : Types)
        {
            Hash.U64(Type->Hash);
            if (Type->IsSingleton())
            {
                const void* Object = Type->GetSingletonConst ? Type->GetSingletonConst(t_Storage) : nullptr;
                Hash.U64(Object != nullptr ? 1 : 0);
                if (Object != nullptr)
                {
                    HashObject(Hash, *Type, Object);
                }
                continue;
            }
            if (Type->Has == nullptr)
            {
                continue;
            }
            for (const Entity Each : Entities)
            {
                if (!Type->Has(t_Storage, Each))
                {
                    continue;
                }
                Hash.U64(Each.ToBits());
                if (!Type->IsTag() && Type->GetConst != nullptr)
                {
                    HashObject(Hash, *Type, Type->GetConst(t_Storage, Each));
                }
            }
        }
        return Hash.Finish();
    }
} // namespace Fling
