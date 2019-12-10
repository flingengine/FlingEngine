#include "Rigidbody.h"

namespace Fling
{
    namespace Components
    {
        void Rigidbody::SetLinearFactor(const btVector3& t_LinearFactor)
        {
            m_LinearFactor = t_LinearFactor;

            if (m_Rigidbody) {
                m_Rigidbody->setLinearFactor(m_LinearFactor);
            }
        }

        void Rigidbody::SetAngularFactor(const btVector3& t_AngularFactor)
        {
            m_AngularFactor = t_AngularFactor;

            if (m_Rigidbody) {
                m_Rigidbody->setAngularFactor(m_AngularFactor);
            }
        }

        void Rigidbody::SetLinearVelocity(const btVector3& t_LinearVelocity)
        {
            m_LinearVelocity = t_LinearVelocity;

            if (m_Rigidbody) {
                m_Rigidbody->setAngularVelocity(m_LinearVelocity);
            }
        }

        void Rigidbody::SetAngularVelocity(const btVector3& t_AngularVelocity)
        {
            m_AngularVelocity = t_AngularVelocity;

            if (m_Rigidbody) {
                m_Rigidbody->setAngularVelocity(m_AngularVelocity);
            }
        }

        void Rigidbody::SetMass(float t_Mass)
        {
            m_Mass = t_Mass;

            //Recalculate mass
            if (!m_Rigidbody)
            {
                return;
            }

            bool isDynamic = m_Mass != 0.0f;
            btVector3 localIntertia;

            if (m_Collider && isDynamic)
            {
                m_Collider->calculateLocalInertia(m_Mass, localIntertia);
            }

            m_LocalInertia = localIntertia;
            m_Rigidbody->setMassProps(m_Mass, localIntertia);
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
        void Rigidbody::SetCollisionShape(std::unique_ptr<btCollisionShape> t_Collider)
        {
            if (t_Collider)
            {
                m_Collider = std::move(t_Collider);
            }
        }
    }
}

