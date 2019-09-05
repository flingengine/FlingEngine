#include "Camera.h"

namespace Fling
{
    void Camera::UpdateViewPrespective()
    {
        m_projectionMatrix = glm::perspective(
            m_fieldOfView, 
            m_width / m_height, 
            m_nearPlane, 
            m_nearPlane);

        m_viewMatrix = glm::lookAt(m_position, m_position + m_front, m_up);
    }

    void Camera::UpdateCameraVectors()
    {
        // Calculate the new Front vector
        glm::vec3 front;
        front.x = cos(glm::radians(m_yaw)) * cos(glm::radians(m_pitch));
        front.y = sin(glm::radians(m_pitch));
        front.z = sin(glm::radians(m_yaw)) * cos(glm::radians(m_pitch));
        m_front = glm::normalize(m_front);
        // Also re-calculate the Right and Up vector
        m_right = glm::normalize(glm::cross(m_front, glm::vec3(0.0f, 1.0f, 0.0f))); 
        m_up    = glm::normalize(glm::cross(m_right, m_front));
    }
}



