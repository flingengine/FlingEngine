#include "Components/AppRequests.h"
#include "Components/EntityGuid.h"
#include "Components/InputState.h"
#include "Components/Name.hpp"
#include "Components/Transform.h"
#include "Ecs/ModuleRegistration.h"
#include "Ecs/RegisterEngine.h"
#include "Systems/InputSystem.h"

// The Engine module's register file: every Engine component type and system is
// listed here, and only this file includes the system headers.

namespace Fling
{
    void RegisterEngine(ModuleRegistration& t_Modules)
    {
        t_Modules.Component<Transform>(
            [](TypeBuilder<Transform>& t_Type)
            {
                // The world matrix and the dirty flag are derived, not saved
                t_Type.Category("Core").Partial();
                t_Type.Field("position", &Transform::m_Pos);
                t_Type.Field("rotation", &Transform::m_Rotation).Tooltip("Euler angles in degrees");
                t_Type.Field("scale", &Transform::m_Scale);
                t_Type.AfterLoad(
                    [](Transform& t_Trans)
                    {
                        Transform::CalculateWorldMatrix(t_Trans);
                        // The GPU copy is still out of date, so keep the dirty flag set
                        t_Trans.MarkDirty();
                    });
            });

        t_Modules.Component<NameComponent>([](TypeBuilder<NameComponent>& t_Type)
                                           { t_Type.Category("Core").Field("Name", &NameComponent::Name); });

        t_Modules.Component<EntityGuid>([](TypeBuilder<EntityGuid>& t_Type)
                                        { t_Type.Category("Core").Field("value", &EntityGuid::Value).ReadOnly(); });

        t_Modules.Component<AppRequests>();
        t_Modules.Component<InputState>();

        t_Modules.System<InputSystem>();
    }
} // namespace Fling
