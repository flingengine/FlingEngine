#include "pch.h"
#include "Json.h"

#include <fstream>
#include <nlohmann/json.hpp>

namespace Fling
{
	struct Json::Impl
	{
		nlohmann::json data;
	};

	Json::Json()
		: m_Impl(std::make_unique<Impl>())
	{
	}

	Json::Json(const Json& other)
		: m_Impl(std::make_unique<Impl>(*other.m_Impl))
	{
	}

	Json::Json(Json&& other) noexcept = default;

	Json& Json::operator=(const Json& other)
	{
		if (this != &other)
		{
			*m_Impl = *other.m_Impl;
		}
		return *this;
	}

	Json& Json::operator=(Json&& other) noexcept = default;

	Json::~Json() = default;

	Json Json::Parse(std::string_view text)
	{
		Json result;
		try
		{
			result.m_Impl->data = nlohmann::json::parse(std::string(text));
		}
		catch (const std::exception& e)
		{
			F_LOG_ERROR("JSON parse failed: {}", e.what());
			result.m_Impl->data = nullptr;
		}
		return result;
	}

	Json Json::Object()
	{
		Json result;
		result.m_Impl->data = nlohmann::json::object();
		return result;
	}

	Json Json::Array()
	{
		Json result;
		result.m_Impl->data = nlohmann::json::array();
		return result;
	}

	std::string Json::Dump(int indent /* = 4 */) const
	{
		return m_Impl->data.dump(indent);
	}

	bool Json::LoadFromFile(const std::string& path)
	{
		std::ifstream ifs(path);
		if (!ifs.is_open())
		{
			F_LOG_ERROR("Failed to load JSON file {}", path);
			return false;
		}

		try
		{
			ifs >> m_Impl->data;
			return true;
		}
		catch (const std::exception& e)
		{
			F_LOG_ERROR("Failed to parse JSON file {}: {}", path, e.what());
			m_Impl->data = nullptr;
			return false;
		}
	}

	bool Json::SaveToFile(const std::string& path) const
	{
		std::ofstream ofs(path);
		if (!ofs.is_open())
		{
			F_LOG_ERROR("Failed to save JSON file {}", path);
			return false;
		}

		ofs << Dump(4) << std::endl;
		return true;
	}

	bool Json::Contains(const std::string& key) const
	{
		if (!m_Impl->data.is_object())
		{
			return false;
		}
		return m_Impl->data.find(key) != m_Impl->data.end();
	}

	std::vector<std::string> Json::Keys() const
	{
		std::vector<std::string> keys;
		if (!m_Impl->data.is_object())
		{
			return keys;
		}

		keys.reserve(m_Impl->data.size());
		for (auto it = m_Impl->data.begin(); it != m_Impl->data.end(); ++it)
		{
			keys.push_back(it.key());
		}
		return keys;
	}

	std::size_t Json::Size() const
	{
		return m_Impl->data.size();
	}

	Json Json::Get(const std::string& key) const
	{
		Json result;
		if (!Contains(key))
		{
			return result;
		}

		result.m_Impl->data = m_Impl->data.at(key);
		return result;
	}

	Json Json::At(std::size_t index) const
	{
		Json result;
		if (!m_Impl->data.is_array() || index >= m_Impl->data.size())
		{
			return result;
		}

		result.m_Impl->data = m_Impl->data.at(index);
		return result;
	}

	void Json::EnsureObject()
	{
		if (!m_Impl->data.is_object())
		{
			m_Impl->data = nlohmann::json::object();
		}
	}

	void Json::Set(const std::string& key, const Json& value)
	{
		EnsureObject();
		m_Impl->data[key] = value.m_Impl->data;
	}

	void Json::PushBack(const Json& value)
	{
		if (m_Impl->data.is_null())
		{
			m_Impl->data = nlohmann::json::array();
		}

		if (!m_Impl->data.is_array())
		{
			F_LOG_ERROR("Json::PushBack called on a non-array value");
			return;
		}

		m_Impl->data.push_back(value.m_Impl->data);
	}

	std::string Json::GetString(const std::string& key, const std::string& def /* = {} */) const
	{
		if (!Contains(key) || !m_Impl->data.at(key).is_string())
		{
			return def;
		}
		return m_Impl->data.at(key).get<std::string>();
	}

	float Json::GetFloat(const std::string& key, float def /* = 0.f */) const
	{
		if (!Contains(key) || !m_Impl->data.at(key).is_number())
		{
			return def;
		}
		return m_Impl->data.at(key).get<float>();
	}

	bool Json::GetBool(const std::string& key, bool def /* = false */) const
	{
		if (!Contains(key) || !m_Impl->data.at(key).is_boolean())
		{
			return def;
		}
		return m_Impl->data.at(key).get<bool>();
	}

	int Json::GetInt(const std::string& key, int def /* = 0 */) const
	{
		if (!Contains(key) || !m_Impl->data.at(key).is_number())
		{
			return def;
		}

		const nlohmann::json& value = m_Impl->data.at(key);
		if (value.is_number_integer())
		{
			return value.get<int>();
		}
		return static_cast<int>(value.get<double>());
	}

	static glm::vec3 ReadVec3(const nlohmann::json& value, const glm::vec3& def)
	{
		if (!value.is_array() || value.size() < 3)
		{
			return def;
		}
		if (!value.at(0).is_number() || !value.at(1).is_number() || !value.at(2).is_number())
		{
			return def;
		}
		return glm::vec3(value.at(0).get<float>(), value.at(1).get<float>(), value.at(2).get<float>());
	}

	static glm::vec4 ReadVec4(const nlohmann::json& value, const glm::vec4& def)
	{
		if (!value.is_array() || value.size() < 3)
		{
			return def;
		}
		if (!value.at(0).is_number() || !value.at(1).is_number() || !value.at(2).is_number())
		{
			return def;
		}

		const float w = (value.size() >= 4 && value.at(3).is_number()) ? value.at(3).get<float>() : 1.0f;
		return glm::vec4(value.at(0).get<float>(), value.at(1).get<float>(), value.at(2).get<float>(), w);
	}

	glm::vec3 Json::GetVec3(const std::string& key, const glm::vec3& def /* = {} */) const
	{
		if (!Contains(key))
		{
			return def;
		}
		return ReadVec3(m_Impl->data.at(key), def);
	}

	glm::vec4 Json::GetVec4(const std::string& key, const glm::vec4& def /* = {} */) const
	{
		if (!Contains(key))
		{
			return def;
		}
		return ReadVec4(m_Impl->data.at(key), def);
	}

	void Json::Set(const std::string& key, const glm::vec3& v)
	{
		EnsureObject();
		m_Impl->data[key] = nlohmann::json::array({ v.x, v.y, v.z });
	}

	void Json::Set(const std::string& key, const glm::vec4& v)
	{
		EnsureObject();
		m_Impl->data[key] = nlohmann::json::array({ v.x, v.y, v.z, v.w });
	}

	void Json::Set(const std::string& key, const char* v)
	{
		EnsureObject();
		m_Impl->data[key] = v ? v : "";
	}

	void Json::Set(const std::string& key, const std::string& v)
	{
		EnsureObject();
		m_Impl->data[key] = v;
	}

	void Json::Set(const std::string& key, float v)
	{
		EnsureObject();
		m_Impl->data[key] = v;
	}

	void Json::Set(const std::string& key, bool v)
	{
		EnsureObject();
		m_Impl->data[key] = v;
	}

	void Json::Set(const std::string& key, int v)
	{
		EnsureObject();
		m_Impl->data[key] = v;
	}

	bool Json::IsNull() const
	{
		return m_Impl->data.is_null();
	}

	bool Json::IsObject() const
	{
		return m_Impl->data.is_object();
	}

	bool Json::IsArray() const
	{
		return m_Impl->data.is_array();
	}
}	// namespace Fling
