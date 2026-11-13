#include "Components/AppRequests.h"
#include "Components/EntityGuid.h"
#include "Components/InputSnapshot.h"
#include "Components/InputSource.h"
#include "Components/Name.hpp"
#include "Components/Transform.h"
#include "Ecs/ModuleRegistration.h"
#include "Ecs/RegisterEngine.h"
#include "Systems/InputSystem.h"
#include "Systems/TransformSystem.h"
#include "TransformUtil.h"

// The Engine module's register file: every Engine component type and system is
// listed here, and only this file includes the system headers.

namespace Fling
{
    void RegisterEngine(ModuleRegistration& t_Modules)
    {
        t_Modules.Component<Transform>(
            [](TypeBuilder<Transform>& t_Type)
            {
                t_Type.Category("Core").Version(2);
                t_Type.Field("position", &Transform::Position);
                t_Type.Field("rotation", &Transform::Rotation).Tooltip("A quaternion, saved as [x, y, z, w]");
                t_Type.Field("scale", &Transform::Scale);

                // Version 1 saved the rotation as Euler angles in degrees
                t_Type.Upgrade(1,
                               [](Json& t_Data)
                               {
                                   if (t_Data.Contains("rotation"))
                                   {
                                       const glm::vec3 Degrees = t_Data.GetVec3("rotation");
                                       t_Data.SetQuat("rotation", TransformUtil::FromEulerDegrees(Degrees));
                                   }
                               });
            });

        t_Modules.Component<Parent>([](TypeBuilder<Parent>& t_Type)
                                    { t_Type.Category("Core").Field("value", &Parent::Value).ReadOnly(); });
        t_Modules.Component<Children>();
        t_Modules.Component<LocalToWorld>();
        t_Modules.Component<TransformWork>();

        t_Modules.Component<NameComponent>([](TypeBuilder<NameComponent>& t_Type)
                                           { t_Type.Category("Core").Field("Name", &NameComponent::Name); });

        t_Modules.Component<EntityGuid>([](TypeBuilder<EntityGuid>& t_Type)
                                        { t_Type.Category("Core").Field("value", &EntityGuid::Value).ReadOnly(); });

        t_Modules.Component<AppRequests>();
        t_Modules.Component<InputSnapshot>();
        t_Modules.Component<InputSource>();

        t_Modules.System<InputSystem>();
        t_Modules.System<TransformSystem>();
    }
} // namespace Fling
