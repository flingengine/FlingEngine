#pragma once

#include "JsonArchive.h"
#include "FlingMath.h"

#include <ostream>

namespace Fling
{
	struct Transform
	{
		glm::mat4 GetWorldMatrix() const;

		static void CalculateWorldMatrix(Transform& t_Trans);

		bool operator==(const Transform &other) const;
		bool operator!=(const Transform &other) const;
		friend std::ostream& operator << (std::ostream& t_OutStream, const Fling::Transform& t_Transform);

		void Serialize(JsonArchive& Ar);

		inline const glm::vec3& GetPos() const { return m_Pos; }
		inline const glm::vec3& GetScale() const { return m_Scale; }
		inline const glm::vec3& GetRotation() const { return m_Rotation; }
		inline const glm::mat4& GetWorldMat() const { return m_worldMat; }

		void SetPos(const glm::vec3& t_Pos);
		void SetScale(const glm::vec3& t_Scale);
		void SetRotation(const glm::vec3& t_Rot);

	//private:
		glm::vec3 m_Pos { 0.0f, 0.0f, 0.0f };
		glm::vec3 m_Rotation { 0.0f, 0.0f, 0.0f };
		glm::vec3 m_Scale { 1.0f, 1.0f, 1.0f };
		glm::mat4 m_worldMat {};
	};
}	// namespace Fling
