#pragma once

#include "FlingMath.h"
#include "Strings/String.h"

#include <cstdint>
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

		/** Whole numbers. A value that is not a whole number, or does not fit, gives def. */
		int64_t GetInt64(StringView key, int64_t def = 0) const;
		uint64_t GetUInt64(StringView key, uint64_t def = 0) const;
		double GetDouble(StringView key, double def = 0.0) const;

		glm::vec2 GetVec2(StringView key, const glm::vec2& def = {}) const;
		glm::vec3 GetVec3(StringView key, const glm::vec3& def = {}) const;
		glm::vec4 GetVec4(StringView key, const glm::vec4& def = {}) const;

		/** A quaternion saved as [x, y, z, w]. */
		glm::quat GetQuat(StringView key, const glm::quat& def = glm::quat(1.f, 0.f, 0.f, 0.f)) const;

		void Set(StringView key, const glm::vec2& v);

		void Set(StringView key, const glm::vec3& v);
		void Set(StringView key, const glm::vec4& v);
		/** Keeps a literal from picking the bool overload. */
		void Set(StringView key, const char* v);
		void Set(StringView key, StringView v);
		void Set(StringView key, float v);
		void Set(StringView key, bool v);
		void Set(StringView key, int v);
		void SetInt64(StringView key, int64_t v);
		void SetUInt64(StringView key, uint64_t v);
		void SetDouble(StringView key, double v);
		void SetQuat(StringView key, const glm::quat& v);

		/** Remove a key from an object. Does nothing if the key is missing. */
		void Remove(StringView key);

		/** True if the key holds a string. */
		bool IsString(StringView key) const;

		bool IsNull() const;
		bool IsObject() const;
		bool IsArray() const;

	private:
		struct Impl;
		std::unique_ptr<Impl> m_Impl;

		void EnsureObject();
	};
}	// namespace Fling
