#include "FirstPersonCamera.h"
#include "FlingMath.h"
#include "pch.h"

namespace Fling 
{
    FirstPersonCamera::FirstPersonCamera(float aspectRatio) :
		m_front(glm::vec3(-1.0f))
    {
		m_nearPlane   =   0.1f;
		m_farPlane    =   1000.0f;
		m_speed       =   10.0f;
        m_aspectRatio =   aspectRatio;
		m_fieldOfView =   glm::radians(45.0f);
		m_position    =   glm::vec3(-6.0f, 0.0f, 2.0f);
		m_rotation    =   glm::vec3(0.0f, 0.0f, 0.0f);
		m_worldUp     =   glm::vec3(0.0f, 0.0f, 1.0f);
    }

	FirstPersonCamera::FirstPersonCamera(
		glm::vec3 position,
		glm::vec3 rotation,
		float speed,
		float aspectRatio) :
		m_front(glm::vec3(-1.0f))
	{
		m_position     = position;
		m_rotation     = rotation;
		m_speed        = speed;
		m_aspectRatio  = aspectRatio;

		m_nearPlane    = 0.1f;
		m_farPlane     = 1000.0f;
		m_worldUp      = glm::vec3(0.0f, 0.0f, 1.0f);
		m_fieldOfView  = glm::radians(45.0f);
	}

	FirstPersonCamera::FirstPersonCamera(
		glm::vec3 front, glm::vec3 up, 
		glm::vec3 position, glm::vec3 rotation, 
		float speed, 
		float nearPlane, float farPlane, 
		float fov, 
		float aspectRatio) :
		m_front(front)
	{
		m_worldUp     = up;
		m_position    = position;
		m_rotation    = rotation;
		m_speed       = speed;
		m_nearPlane   = nearPlane;
		m_farPlane    = farPlane;
		m_fieldOfView = glm::radians(fov);
		m_aspectRatio = aspectRatio;
	}


	void FirstPersonCamera::Update(float dt)
    {
        float moveSpeed = m_speed * dt;

		//translation
		//forward
        if(Input::IsKeyHeld(KeyNames::FL_KEY_W))
		{
			m_position += m_front * moveSpeed;
		}

		//backward
        if(Input::IsKeyHeld(KeyNames::FL_KEY_S))
		{
			m_position -= m_front * moveSpeed;
		}

		//left
        if(Input::IsKeyHeld(KeyNames::FL_KEY_A))
		{
			m_position -= m_right* moveSpeed;
		}

		//right
        if(Input::IsKeyHeld(KeyNames::FL_KEY_D))
		{
			m_position += m_right * moveSpeed;
		}

		//Rotation
		//Rotate left
		if (Input::IsKeyHeld(KeyNames::FL_KEY_Q))
		{
			m_rotation.x += moveSpeed;
		}

		//Rotate right
		if (Input::IsKeyHeld(KeyNames::FL_KEY_E))
		{
			m_rotation.x -= moveSpeed;
		}

		//Rotate up
		if (Input::IsKeyHeld(KeyNames::FL_KEY_Z))
		{
			m_rotation.y += moveSpeed;
		}

		//Rotate down
		if (Input::IsKeyHeld(KeyNames::FL_KEY_X))
		{
			m_rotation.y -= moveSpeed;
		}


		//Pitch constraints
		if (m_rotation.y > 89.0f)
		{
			m_rotation.y = 89.0f;
		}
		if (m_rotation.y < -89.0f)
		{ 
			m_rotation.y = -89.0f;
		}
		
		UpdateCameraVectors();
		UpdateViewMatrix();
		UpdateProjectionMatrix();
    }

	void FirstPersonCamera::UpdateViewMatrix()
    {
		m_viewMatrix = glm::lookAt(m_position, m_position + m_front, m_up);
    }

	void FirstPersonCamera::UpdateProjectionMatrix()
    {
        m_projectionMatrix = glm::perspective(m_fieldOfView, m_aspectRatio, m_nearPlane, m_farPlane);
    }

	void Fling::FirstPersonCamera::UpdateCameraVectors()
	{
		// Calculate the new Front vector
		glm::vec3 front;
		front.x = cos(glm::radians(m_rotation.x)) * cos(glm::radians(m_rotation.y));
		front.y = sin(glm::radians(m_rotation.x));
		front.z = sin(glm::radians(m_rotation.y)) * cos(glm::radians(m_rotation.x));
		m_front = glm::normalize(front);
		// Also re-calculate the Right and Up vector
		m_right = glm::normalize(glm::cross(m_front, m_worldUp));  
		m_up = glm::normalize(glm::cross(m_right, m_front));
	}

} //namespace Fling 