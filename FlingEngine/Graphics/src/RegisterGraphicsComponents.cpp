#include "pch.h"
#include "RegisterGraphicsComponents.h"
#include "ComponentTypeRegistry.h"
#include "MeshRenderer.h"
#include "Lighting/DirectionalLight.hpp"
#include "Lighting/PointLight.hpp"

namespace Fling
{
	void RegisterGraphicsComponents()
	{
		ComponentTypeRegistry& registry = ComponentTypeRegistry::Get();
		registry.Register<MeshRenderer>("MeshRenderer");
		registry.Register<DirectionalLight>("DirectionalLight");
		registry.Register<PointLight>("PointLight");
	}
}	// namespace Fling
