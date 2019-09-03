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

		const float& GetNearPlane() { return m_nearPlane; }
		void SetNearPlane(const float& nearPlane) { m_nearPlane = nearPlane; }

		const float& GetFarPlane() { return m_farPlane; }
		void SetFarPlane(const float& farPlane) { m_farPlane = farPlane; }


	private:
		float m_nearPlane;
		float m_farPlane;
		float m_fieldOfView;

		glm::vec3 m_position;
		glm::vec3 m_rotation;
		glm::vec3 m_velocity;

		glm::mat4 m_viewMatrix;
		glm::mat4 m_projectionMatrix;

	};


} //namespace fling