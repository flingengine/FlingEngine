#include "Camera.h"

namespace Fling
{
    void Camera::Update()
    {
        m_projectionMatrix = glm::prespective(
            m_fieldOfView, 
            width, 
            height, 
            m_nearPlane, 
            m_farplane);

        m_viewMatrix = glm::lookAt(m_position, m_position + m_front, m_up);
    }
}



