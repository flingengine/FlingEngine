#include "Rigidbody.h"

namespace Fling
{
    namespace Components
    {
        void Rigidbody::SetLinearFactor(const glm::vec3& t_LinearFactor)
        {
            m_LinearFactor = t_LinearFactor;

            if (m_Rigidbody) {
                m_Rigidbody->setLinearFactor(MathConversions::glmToBullet(m_LinearFactor));
            }
        }

        void Rigidbody::SetAngularFactor(const glm::vec3& t_AngularFactor)
        {
            m_AngularFactor = t_AngularFactor;

            if (m_Rigidbody) {
                m_Rigidbody->setAngularFactor(MathConversions::glmToBullet(m_AngularFactor));
            }
        }

        void Rigidbody::SetLinearVelocity(const glm::vec3& t_LinearVelocity)
        {
            m_LinearVelocity = t_LinearVelocity;

            if (m_Rigidbody) {
                m_Rigidbody->setAngularVelocity(MathConversions::glmToBullet(m_LinearVelocity));
            }
        }

        void Rigidbody::SetAngularVelocity(const glm::vec3& t_AngularVelocity)
        {
            m_AngularVelocity = t_AngularVelocity;

            if (m_Rigidbody) {
                m_Rigidbody->setAngularVelocity(MathConversions::glmToBullet(m_AngularVelocity));
            }
        }

        void Rigidbody::SetFritcion(float t_Friction)
        {
            m_Friction = t_Friction;

            if (m_Rigidbody) {
                m_Rigidbody->setFriction(m_Friction);
            }
        }

        void Rigidbody::SetFrictionRolling(float t_FrictionRolling)
        {
            m_FrictionRolling = t_FrictionRolling;

            if (m_Rigidbody) {
                m_Rigidbody->setRollingFriction(m_FrictionRolling);
            }
        }

        void Rigidbody::SetFritcionSpinning(float t_FrictionSpinning)
        {
            m_FrictionSpinning = t_FrictionSpinning;

            if (m_Rigidbody) {
                m_Rigidbody->setSpinningFriction(m_FrictionSpinning);
            }
        }
    }
}

