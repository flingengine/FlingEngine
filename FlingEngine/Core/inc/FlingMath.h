#pragma once

// GLM 
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES  // Force GLM to use a version of vec2 and mat4 that have specific alignments
#define GLM_ENABLE_EXPERIMENTAL	// So that we can hash our vertecies 

#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/hash.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtx/transform.hpp> 
#include <glm/gtc/random.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "btBulletDynamicsCommon.h"

namespace Fling
{
    namespace MathConversions
    {
        glm::vec3 bulletToGlm(const btVector3& v);

        glm::quat bulletToGlm(const btQuaternion& q);

        btVector3 glmToBullet(const glm::vec3& v);

        btQuaternion glmToBullet(const glm::quat& q);

        btMatrix3x3 glmToBullet(const glm::mat3& m);

        // btTransform does not contain a full 4x4 matrix, so this transform is lossy.
        // Affine transformations are OK but perspective transformations are not.
        btTransform glmToBullet(const glm::mat4& m);

        btTransform glmToBullet(const glm::vec3& t_Position, const glm::vec3& t_Rotation);
    }
}