#include "Physics.h"
#include "Components/Transform.h"

namespace Fling
{
	namespace Physics
	{
		void Step(entt::registry& t_Reg, float t_DeltaTime)
		{
			auto View = t_Reg.view<Transform, RigidBody>();
			for (entt::entity Entity : View)
			{
				Transform& Trans = View.get<Transform>(Entity);
				const RigidBody& Body = View.get<RigidBody>(Entity);
				Trans.SetPos(Trans.GetPos() + Body.m_Velocity * t_DeltaTime);
			}
		}
	}	// namespace Physics
}	// namespace Fling
