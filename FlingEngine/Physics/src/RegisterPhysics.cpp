#include "Ecs/ModuleRegistration.h"
#include "Physics.h"
#include "PhysicsComponents.h"
#include "PhysicsTypes.h"
#include "Systems/PhysicsStepSystem.h"

#include <cstddef>
#include <vector>

// The Physics module's register file: every Physics component type and system is
// listed here, and only this file includes the system headers.

/** The contacts as one field, so the state hash sees them: [a, b, x, y, z, impulse] per contact. */
template <>
struct Fling::FieldTraits<std::vector<Fling::ContactEvent>>
{
    static constexpr FieldKind Kind = FieldKind::Custom;

    static void Save(const std::vector<ContactEvent>& t_Contacts, Json& t_Object, const StringView t_Key)
    {
        Json List = Json::Array();
        for (const ContactEvent& Contact : t_Contacts)
        {
            Json Item = Json::Object();
            Item.SetUInt64("a", Contact.m_A.ToBits());
            Item.SetUInt64("b", Contact.m_B.ToBits());
            Item.Set("point", Contact.m_Point);
            Item.Set("impulse", Contact.m_Impulse);
            List.PushBack(Item);
        }
        t_Object.Set(t_Key, List);
    }

    static bool Load(std::vector<ContactEvent>& t_Contacts, const Json& t_Object, const StringView t_Key)
    {
        if (!t_Object.Contains(t_Key))
        {
            return false;
        }
        const Json List = t_Object.Get(t_Key);
        t_Contacts.clear();
        for (std::size_t i = 0; i < List.Size(); ++i)
        {
            const Json Item = List.At(i);
            t_Contacts.push_back(ContactEvent{Entity::FromBits(Item.GetUInt64("a")),
                                              Entity::FromBits(Item.GetUInt64("b")),
                                              Item.GetVec3("point"),
                                              Item.GetFloat("impulse")});
        }
        return true;
    }
};

namespace Fling
{
    void RegisterPhysics(ModuleRegistration& t_Modules)
    {
        t_Modules.Component<RigidBody>(
            [](TypeBuilder<RigidBody>& t_Type)
            {
                t_Type.Category("Physics");
                t_Type.Field("mass", &RigidBody::m_Mass).Range(0.0f, 10000.0f).Tooltip("0 makes a static body");
                t_Type.Field("friction", &RigidBody::m_Friction).Range(0.0f, 1.0f);
                t_Type.Field("rollingFriction", &RigidBody::m_RollingFriction).Range(0.0f, 1.0f);
                t_Type.Field("restitution", &RigidBody::m_Restitution).Range(0.0f, 1.0f);
                t_Type.Field("linearDamping", &RigidBody::m_LinearDamping).Range(0.0f, 1.0f);
                t_Type.Field("angularDamping", &RigidBody::m_AngularDamping).Range(0.0f, 1.0f);
                t_Type.Field("velocity", &RigidBody::m_Velocity);
                t_Type.Field("angularVelocity", &RigidBody::m_AngularVelocity);
            });

        t_Modules.Component<Collider>(
            [](TypeBuilder<Collider>& t_Type)
            {
                t_Type.Category("Physics");
                t_Type.Field("shape", &Collider::m_Shape)
                    .Enum<ColliderShape>({{"box", ColliderShape::Box}, {"sphere", ColliderShape::Sphere}});
                t_Type.Field("halfExtents", &Collider::m_HalfExtents);
                t_Type.Field("radius", &Collider::m_Radius);
            });

        t_Modules.Component<PhysicsBody>();
        t_Modules.Component<PhysicsSettings>();
        t_Modules.Component<PhysicsCommands>();
        t_Modules.Component<ContactEvents>([](TypeBuilder<ContactEvents>& t_Type)
                                           { t_Type.Field("contacts", &ContactEvents::m_Contacts); });
        t_Modules.Component<PhysicsWorld>();

        t_Modules.System<PhysicsStepSystem>();
    }
} // namespace Fling
