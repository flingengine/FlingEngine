#include "Ecs/ModuleRegistration.h"
#include "Physics.h"
#include "PhysicsComponents.h"
#include "PhysicsTypes.h"
#include "Systems/PhysicsStepSystem.h"

// The Physics module's register file: every Physics component type and system is
// listed here, and only this file includes the system headers.

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
        t_Modules.Component<ContactEvents>();
        t_Modules.Component<PhysicsWorld>();

        t_Modules.System<PhysicsStepSystem>();
    }
} // namespace Fling
