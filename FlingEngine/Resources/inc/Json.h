#pragma once

#include "FlingMath.h"

#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace Fling
{
	/**
	 * Engine JSON document wrapper.
	 * 
	 * Just a wrapper for nlohmann/json.hpp right now.
	 */
	class Json
	{
	public:
		Json();
		Json(const Json& other);
		Json(Json&& other) noexcept;
		Json& operator=(const Json& other);
		Json& operator=(Json&& other) noexcept;
		~Json();

		static Json Parse(std::string_view text);
		static Json Object();
		static Json Array();

		std::string Dump(int indent = 4) const;
		bool LoadFromFile(const std::string& path);
		bool SaveToFile(const std::string& path) const;

		bool Contains(const std::string& key) const;
		std::vector<std::string> Keys() const;
		std::size_t Size() const;

		/** Missing keys return a null Json (does not insert). */
		Json Get(const std::string& key) const;
		Json At(std::size_t index) const;
		void Set(const std::string& key, const Json& value);
		void PushBack(const Json& value);

		std::string GetString(const std::string& key, const std::string& def = {}) const;
		float GetFloat(const std::string& key, float def = 0.f) const;
		bool GetBool(const std::string& key, bool def = false) const;
		int GetInt(const std::string& key, int def = 0) const;

		glm::vec3 GetVec3(const std::string& key, const glm::vec3& def = {}) const;
		glm::vec4 GetVec4(const std::string& key, const glm::vec4& def = {}) const;

		void Set(const std::string& key, const glm::vec3& v);
		void Set(const std::string& key, const glm::vec4& v);
		void Set(const std::string& key, const char* v);
		void Set(const std::string& key, const std::string& v);
		void Set(const std::string& key, float v);
		void Set(const std::string& key, bool v);
		void Set(const std::string& key, int v);

		bool IsNull() const;
		bool IsObject() const;
		bool IsArray() const;

	private:
		struct Impl;
		std::unique_ptr<Impl> m_Impl;

		void EnsureObject();
	};
}	// namespace Fling
