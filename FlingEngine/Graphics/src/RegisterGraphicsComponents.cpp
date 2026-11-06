#include "ComponentTypeRegistry.h"
#include "Lighting/DirectionalLight.hpp"
#include "Lighting/PointLight.hpp"
#include "MeshRenderer.h"
#include "RegisterGraphicsComponents.h"

namespace Fling
{
	void RegisterGraphicsComponents()
	{
		ComponentTypeRegistry& registry = ComponentTypeRegistry::Get();
		registry.Register<MeshRenderer>();
		registry.Register<DirectionalLight>();
		registry.Register<PointLight>();
	}
}	// namespace Fling
