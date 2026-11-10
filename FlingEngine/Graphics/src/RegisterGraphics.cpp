#include "Ecs/ModuleRegistration.h"
#include "Lighting/DirectionalLight.hpp"
#include "Lighting/PointLight.hpp"
#include "MeshRenderer.h"
#include "RegisterGraphics.h"
#include "RenderSnapshot.h"
#include "Systems/PublishSnapshotSystem.h"

// The Graphics module's register file: every Graphics component type and system is
// listed here, and only this file includes the system headers.

namespace Fling
{
    void RegisterGraphics(ModuleRegistration& t_Modules)
    {
        t_Modules.Component<MeshRenderer>(
            [](TypeBuilder<MeshRenderer>& t_Type)
            {
                t_Type.Category("Rendering");
                t_Type.Field("mesh", &MeshRenderer::Mesh).Tooltip("A model, relative to the assets folder");
                t_Type.Field("material", &MeshRenderer::Material).Tooltip("Empty uses the default material");
            });

        t_Modules.Component<DirectionalLight>(
            [](TypeBuilder<DirectionalLight>& t_Type)
            {
                t_Type.Category("Lighting");
                t_Type.Field("color", &DirectionalLight::DiffuseColor).Color();
                t_Type.Field("direction", &DirectionalLight::Direction);
                t_Type.Field("intensity", &DirectionalLight::Intensity).Range(0.0f, 100.0f);
            });

        t_Modules.Component<PointLight>(
            [](TypeBuilder<PointLight>& t_Type)
            {
                // The position comes from the Transform each frame
                t_Type.Category("Lighting").Partial();
                t_Type.Field("color", &PointLight::DiffuseColor).Color();
                t_Type.Field("range", &PointLight::Range).Range(0.0f, 1000.0f);
                t_Type.Field("intensity", &PointLight::Intensity).Range(0.0f, 1000.0f);
            });

        t_Modules.Component<RenderSnapshot>();

        t_Modules.System<PublishSnapshotSystem>();
    }
} // namespace Fling
