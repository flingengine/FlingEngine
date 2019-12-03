#include "pch.h"
#include "FlingMath.h"

namespace Fling
{
    namespace MathConversions
    {
        glm::vec3 bulletToGlm(const btVector3& v)
        {
            return glm::vec3(v.getX(), v.getY(), v.getZ());
        }

        glm::quat bulletToGlm(const btQuaternion& q)
        {
            return glm::quat(q.getW(), q.getX(), q.getY(), q.getZ());
        }

        btVector3 glmToBullet(const glm::vec3& v)
        {
            return btVector3(v.x, v.y, v.z);
        }

        btQuaternion glmToBullet(const glm::quat& q)
        {
            return btQuaternion(q.x, q.y, q.z, q.w); 
        }

        btMatrix3x3 glmToBullet(const glm::mat3& m)
        {
            return btMatrix3x3(m[0][0], m[1][0], m[2][0], m[0][1], m[1][1], m[2][1], m[0][2], m[1][2], m[2][2]);
        }

        btTransform glmToBullet(const glm::mat4& m)
        {
            glm::mat3 m3(m);
            return btTransform(glmToBullet(m3), glmToBullet(glm::vec3(m[3][0], m[3][1], m[3][2])));
        }
    }
}
