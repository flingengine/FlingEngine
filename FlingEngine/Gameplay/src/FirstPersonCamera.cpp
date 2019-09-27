#include "pch.h"

#include "FirstPersonCamera.h"
#include "FlingMath.h"

namespace Fling 
{
    FirstPersonCamera::FirstPersonCamera(float aspectRatio, float t_MoveSpeed, float t_RotSpeed) 
		: m_front(glm::vec3(-1.0f))
		, m_RotationSpeed(t_RotSpeed)
    {
		m_nearPlane = 0.1f;
		m_farPlane = 1000.0f;
		m_speed = t_MoveSpeed;
		m_aspectRatio = aspectRatio;
		m_fieldOfView = glm::radians(45.0f);
		m_position = glm::vec3(0.0f, 0.0f, 5.f);
		m_rotation = glm::vec3(0.0f, 0.0f, 0.0f);
		m_worldUp = glm::vec3(0.0f, 0.0f, 1.0f);

		m_yaw = 0.0f, m_pitch = 0.0f;

		UpdateProjectionMatrix();
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

		UpdateProjectionMatrix();
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

		UpdateProjectionMatrix();
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
			m_position -= m_right * moveSpeed;
		}

		//right
		if(Input::IsKeyHeld(KeyNames::FL_KEY_D))
		{
			m_position += m_right * moveSpeed;
		}

		// Move world up with Q
		if(Input::IsKeyHeld(KeyNames::FL_KEY_Q))
		{
			m_position += m_worldUp * moveSpeed;
		}

		// Move world down with E
		if(Input::IsKeyHeld(KeyNames::FL_KEY_E))
		{
			m_position += -m_worldUp * moveSpeed;
		}
		

		//Rotation
		// Check if we should rotate
		m_IsRotating = Input::IsMouseDown(KeyNames::FL_MOUSE_BUTTON_2);
		MousePos CurMousePos = Input::GetMousePos();
		if (m_IsRotating)
		{
			float RotSpeed = dt * m_RotationSpeed * .001f;

			float MouseDeltaX = CurMousePos.X - m_PrevMousePos.X;
			float MouseDeltaY = CurMousePos.Y - m_PrevMousePos.Y;

			// Invert x rotation because it otherwise it's south paw
			m_yaw += RotSpeed * -MouseDeltaX;	
			m_pitch += RotSpeed * MouseDeltaY;

			m_pitch = glm::clamp(m_pitch, -MAX_PITCH, MAX_PITCH);
		}
		
		// Keep track of the mouse position
		m_PrevMousePos = CurMousePos;
		
		UpdateCameraVectors();
		UpdateViewMatrix();
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
		glm::mat4x4 rotation = glm::eulerAngleYX(m_yaw, m_pitch);
		m_front = rotation * glm::vec4(0.f, 0.f, -1.f, 0.f);
		m_up = rotation * glm::vec4(0.f, 1.f, 0.f, 0.f);
		m_right = glm::cross(m_front, m_up);
	}

} //namespace Fling 