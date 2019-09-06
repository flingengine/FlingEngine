#include "FirstPersonCamera.h"
#include "Input/Input.h"

namespace Fling 
{
    FirstPersonCamera::FirstPersonCamera(float width, float height) :
        m_sensitivity(1.0f)
    {
		m_nearPlane = 0.1f;
		m_farPlane = 1000.0f;
		m_fieldOfView= glm::radians(45.0f);
        m_aspectRatio = width / height;
    }


	void FirstPersonCamera::Update(float dt)
    {

        glm::vec3 cameraFront;

        cameraFront.x = -cos(glm::radians(m_rotation.x)) * sin(glm::radians(m_rotation.y));
		cameraFront.y = sin(glm::radians(m_rotation.x));
		cameraFront.z = cos(glm::radians(m_rotation.x)) * cos(glm::radians(m_rotation.y));
		cameraFront = glm::normalize(cameraFront);

        float moveSpeed = m_speed * dt;

        if(Input::IsKeyDown(KeyNames::FL_KEY_W))
				m_position += cameraFront * moveSpeed;

        if(Input::IsKeyDown(KeyNames::FL_KEY_A))
				m_position -= cameraFront * moveSpeed;

        if(Input::IsKeyDown(KeyNames::FL_KEY_S))
                m_position -= glm::normalize(glm::cross(cameraFront, glm::vec3(0.0f, 1.0f, 0.0f))) * moveSpeed;

        if(Input::IsKeyDown(KeyNames::FL_KEY_D))
                m_position += glm::normalize(glm::cross(cameraFront, glm::vec3(0.0f, 1.0f, 0.0f))) * moveSpeed;

        UpdateViewMatrix();
        UpdateProjectionMatrix();
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

} //namespace Fling 