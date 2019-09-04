#pragma once
#include "pch.h"

namespace Fling
{
	constexper float DEFAULT_NEAR_PLANE = 1.0f;
	constexper float DEFAULT_FAR_PLANE = 1000.0f;
	constexper float DEFAULT_FOV = 45.0f;
	constexper float DEFAULT_MOUSE_SENSITIVITY = 2.5f;
	constexper float DEFAULT_ZOOM = 45.0f;
	constexper float DEFAULT_SPEED = 2.5f;

	class Camera
	{
	public:
		Camera() :
			m_nearPlane(DEFAULT_NEAR_PLANE),
			m_farPlane(DEFAULT_FAR_PLANE),
			m_fieldOfView(glm::radians(DEFAULT_FOV)),
			m_position(glm::vec3(0.0f)),
			m_up(glm::vec3(0.0f, 1.0f, 0.0f)),
			m_yaw(-90.0f),
			m_pitch(0.0f),
			m_front(glm::vec3(0.0f, 0.0f, -1.0f),
			m_speed(DEFAULT_SPEED),
			m_mouseSentivity(DEFAULT_MOUSE_SENSITIVITY),
			m_zoom(DEFAULT_ZOOM))
		{
			UpdateCameraVectors();
		}


		Camera(
			glm::vec3 position,
			glm::vec3 up,
			float yaw,
			float pitch) :
			m_nearPlane(DEFAULT_NEAR_PLANE),
			m_farPlane(DEFAULT_FAR_PLANE),
			m_fieldOfView(glm::radians(DEFAULT_FOV)),
			m_front(glm::vec3(0.0f, 0.0f, -1.0f)), 
			m_speed(DEFAULT_SPEED), 
			m_mouseSentivity(DEFAULT_MOUSE_SENSITIVITY)
		{
			m_position = position;
			m_up = up;
			m_yaw = yaw;
			m_pitch = pitch;
			UpdateCameraVectors();
		}

		~Camera() {};

		void UpdateViewPrespective();

		const float& GetNearPlane() { return m_nearPlane; }
		void SetNearPlane(const float& nearPlane) { m_nearPlane = nearPlane; }

		const float& GetFarPlane() { return m_farPlane; }
		void SetFarPlane(const float& farPlane) { m_farPlane = farPlane; }

		const float& GetFieldOfView() { return m_fieldOfView; }
		void SetFieldOfView(const float& fieldOfView) { m_fieldOfView = fieldOfView; }

		const glm::vec3& GetPosition() { return m_position; }
		const glm::vec3& GetCameraSpeed() { return m_speed; }

		const glm::mat4& GetViewMatrix() {return m_viewMatrix; }
		const glm::mat4& GetProjectionMatrix() { return m_ProjectionMatrix; }

	private:
		glm::mat4 m_viewMatrix;
		glm::mat4 m_projectionMatrix;

		glm::vec3 m_position;
		glm::vec3 m_front;
		glm::vec3 m_up;
		glm::vec3 m_right;

		float m_height, m_width;
		float m_yaw, m_pitch;
		float m_nearPlane;
		float m_farPlane;
		float m_fieldOfView;
		float m_speed;	
		float m_mouseSentivity;
		float m_zoom;

		void UpdateCameraVectors();
	};


} //namespace fling