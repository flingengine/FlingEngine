#pragma once

#include "PhysicsAPI.h"
#include "FlingMath.h"
#include <entt/entity/registry.hpp>

namespace Fling
{
	/** Linear velocity of an entity, in world units per second. */
	struct RigidBody
	{
		glm::vec3 m_Velocity { 0.0f, 0.0f, 0.0f };
	};

	namespace Physics
	{
		/**
		 * Move every entity that has a Transform and a RigidBody by its velocity.
		 * This is a stub: there are no forces, collisions, or rotation yet.
		 */
		PHYSICS_API void Step(entt::registry& t_Reg, float t_DeltaTime);
	}	// namespace Physics
}	// namespace Fling
