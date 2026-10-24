#include <catch2/catch_all.hpp>

#include "Physics.h"
#include "Components/Transform.h"

TEST_CASE("Physics Step moves entities by their velocity", "[physics]")
{
	using namespace Fling;

	entt::registry reg;

	const entt::entity moving = reg.create();
	reg.assign<Transform>(moving);
	reg.assign<RigidBody>(moving).m_Velocity = glm::vec3(1.0f, -2.0f, 0.5f);

	// No RigidBody: Step must not move it
	const entt::entity still = reg.create();
	reg.assign<Transform>(still).SetPos(glm::vec3(3.0f, 3.0f, 3.0f));

	Physics::Step(reg, 2.0f);

	const glm::vec3 movedPos = reg.get<Transform>(moving).GetPos();
	REQUIRE(movedPos.x == Catch::Approx(2.0f));
	REQUIRE(movedPos.y == Catch::Approx(-4.0f));
	REQUIRE(movedPos.z == Catch::Approx(1.0f));

	const glm::vec3 stillPos = reg.get<Transform>(still).GetPos();
	REQUIRE(stillPos.x == Catch::Approx(3.0f));
	REQUIRE(stillPos.y == Catch::Approx(3.0f));
	REQUIRE(stillPos.z == Catch::Approx(3.0f));
}
