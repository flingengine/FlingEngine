#include "Ecs/ModuleRegistration.h"
#include "Lighting/DirectionalLight.hpp"
#include "Lighting/PointLight.hpp"
#include "MeshRenderer.h"
#include "RegisterGraphics.h"

// The Graphics module's register file: every Graphics component type is listed here.

namespace Fling
{
    void RegisterGraphics(ModuleRegistration& t_Modules)
    {
        // The saved data is the two asset paths. The model and material pointers come
        // from the resource manager on load.
        t_Modules.Component<MeshRenderer>(
            [](TypeBuilder<MeshRenderer>& t_Type)
            {
                t_Type.Category("Rendering");
                t_Type.Serialize(
                    [](const MeshRenderer& t_Mesh, Json& t_Out)
                    {
                        t_Out.Set("mesh", t_Mesh.GetModelGuidString());
                        t_Out.Set("material", t_Mesh.GetMaterialGuidString());
                    },
                    [](MeshRenderer& t_Mesh, const Json& t_In)
                    {
                        const std::string MeshPath = t_In.GetString("mesh");
                        const std::string MaterialPath = t_In.GetString("material");
                        if (!MeshPath.empty())
                        {
                            t_Mesh.LoadModelFromPath(MeshPath);
                        }
                        if (!MaterialPath.empty())
                        {
                            t_Mesh.LoadMaterialFromPath(MaterialPath);
                        }
                    });
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

        t_Modules.Component<DeferredRenderTag>();
        t_Modules.Component<DebugRenderTag>();
    }
} // namespace Fling
