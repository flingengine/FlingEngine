#include "FirstPersonCamera.h"
#include "Input.h"

namespace Fling 
{
    FirstPersonCamera::FirstPersonCamera(float& width, float& height) :
        m_nearPlane(0.1f),
        m_farPlane(1000.0f),
        m_fieldOfView(glm::radians(45.0f),
        m_sensitivity(1.0f))
    {
        m_aspectRatio = width / height;
    }


    void FirstPersonCamera::Update()
    {
        
    }

    void FirstPersonCamera::UpdateViewMatrix()
    {
        glm::mat4 rotMatrix = glm::mat4(1.0f);
        glm::mat4 transMatrix;

        rotMatrix = glm::rotate(rotMatrix, glm::radians(m_rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
		rotMatrix = glm::rotate(rotMatrix, glm::radians(m_rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
		rotMatrix = glm::rotate(rotMatrix, glm::radians(m_rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));

        transMatrix = glm::translate(glm::mat4(1.0f), m_position);

        m_viewMatrix = rotMatrix * transMatrix;
    }

    void FirstPersonCamera::UpdateProjectionMatrix()
    {
        m_projectionMatrix = glm::perspective(m_fieldOfView, m_aspectRatio, m_nearPlane, m_farPlane);
    }
}