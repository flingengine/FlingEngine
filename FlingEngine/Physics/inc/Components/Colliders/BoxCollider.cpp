#include "BoxCollider.h"

namespace Fling
{
    namespace Components
    {
        bool BoxCollider::operator==(const BoxCollider& other) const
        {
            return m_BoxCollider == other.m_BoxCollider && m_BtBoxCollider == other.m_BtBoxCollider;
        }

        bool BoxCollider::operator!=(const BoxCollider& other) const
        {
            return !(*this == other);
        }

        void BoxCollider::SetBoxCollider(const glm::vec3& t_BoxCollider)
        {
            m_BoxCollider = t_BoxCollider;
            m_BtBoxCollider = std::make_unique<btBoxShape>(MathConversions::glmToBullet(m_BoxCollider));
        }

        std::ostream& operator<<(std::ostream& t_Outstream, Fling::Components::BoxCollider& t_BoxCollider)
        {
            t_Outstream << "Pos (" << t_BoxCollider.m_BoxCollider.x << "," << t_BoxCollider.m_BoxCollider.y << "," << t_BoxCollider.m_BoxCollider.z << ")";
            return t_Outstream;
        }
    }
}

