#pragma once
#include "pch.h"

namespace Fling
{
	class Camera
	{
	public:
		Camera() :
			m_nearPlane{1.0f},
			m_farPlane{1000.0f},
			m_fieldOfView{glm::radians(45.0f)}
		{

		}

		~Camera() {};

		void Update();

		const float& GetNearPlane() { return m_nearPlane; }
		void SetNearPlane(const float& nearPlane) { m_nearPlane = nearPlane; }

		const float& GetFarPlane() { return m_farPlane; }
		void SetFarPlane(const float& farPlane) { m_farPlane = farPlane; }

		const float& GetFieldOfView() { return m_fieldOfView; }
		void SetFieldOfView(const float& fieldOfView) { m_fieldOfView = fieldOfView; }

		const glm::vec3& GetPosition() { return m_position; }
		const glm::vec3& GetRotation() { return m_rotation; }
		const glm::vec3& GetVelocity() { return m_velocity; }

		const glm::mat4& GetViewMatrix() {return m_viewMatrix; }
		const glm::mat4& GetProjectionMatrix() { return m_ProjectionMatrix; }

	private:
		float height, width;
		float m_nearPlane;
		float m_farPlane;
		float m_fieldOfView;

		glm::vec3 m_position;
		glm::vec3 m_rotation;
		glm::vec3 m_velocity;
		glm::vec3 m_front;
		glm::vec3 m_up;

		glm::mat4 m_viewMatrix;
		glm::mat4 m_projectionMatrix;

	};


} //namespace fling