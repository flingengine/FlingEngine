#pragma once

#include "FlingMath.h"
#include "Strings/String.h"

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
		bool LoadFromFile(StringView path);
		bool SaveToFile(StringView path) const;

		bool Contains(StringView key) const;
		std::vector<std::string> Keys() const;
		std::size_t Size() const;

		/** Missing keys return a null Json (does not insert). */
		Json Get(StringView key) const;
		Json At(std::size_t index) const;
		void Set(StringView key, const Json& value);
		void PushBack(const Json& value);

		std::string GetString(StringView key, StringView def = {}) const;
		float GetFloat(StringView key, float def = 0.f) const;
		bool GetBool(StringView key, bool def = false) const;
		int GetInt(StringView key, int def = 0) const;

		glm::vec3 GetVec3(StringView key, const glm::vec3& def = {}) const;
		glm::vec4 GetVec4(StringView key, const glm::vec4& def = {}) const;

		void Set(StringView key, const glm::vec3& v);
		void Set(StringView key, const glm::vec4& v);
		/** Keeps a literal from picking the bool overload. */
		void Set(StringView key, const char* v);
		void Set(StringView key, StringView v);
		void Set(StringView key, float v);
		void Set(StringView key, bool v);
		void Set(StringView key, int v);

		bool IsNull() const;
		bool IsObject() const;
		bool IsArray() const;

	private:
		struct Impl;
		std::unique_ptr<Impl> m_Impl;

		void EnsureObject();
	};
}	// namespace Fling
