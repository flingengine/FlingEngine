#include "FlingWarnings.h"
#include "Json.h"
#include "Logger.h"
#include "Strings/String.h"

#include <cstdint>
#include <fstream>

// nlohmann/json v3.7 includes <ciso646>, which libstdc++ flags with a #warning under C++20
F_WARNING_PUSH
F_CLANG_DISABLE_WARNING("-W#warnings")
F_GCC_DISABLE_WARNING("-Wcpp")
#include <nlohmann/json.hpp>
F_WARNING_POP

namespace Fling
{
    struct Json::Impl
    {
        nlohmann::json data;
    };

    Json::Json() : m_Impl(std::make_unique<Impl>())
    {
    }

    Json::Json(const Json& other) : m_Impl(std::make_unique<Impl>(*other.m_Impl))
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

    Json Json::Parse(StringView text)
    {
        Json result;
        try
        {
            result.m_Impl->data = nlohmann::json::parse(String(text));
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

    String Json::Dump(int indent /* = 4 */) const
    {
        return m_Impl->data.dump(indent);
    }

    bool Json::LoadFromFile(StringView path)
    {
        std::ifstream ifs(PathFromUtf8(path));
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

    bool Json::SaveToFile(StringView path) const
    {
        std::ofstream ofs(PathFromUtf8(path));
        if (!ofs.is_open())
        {
            F_LOG_ERROR("Failed to save JSON file {}", path);
            return false;
        }

        ofs << Dump(4) << std::endl;
        return true;
    }

    bool Json::Contains(StringView key) const
    {
        if (!m_Impl->data.is_object())
        {
            return false;
        }
        return m_Impl->data.find(String(key)) != m_Impl->data.end();
    }

    std::vector<String> Json::Keys() const
    {
        std::vector<String> keys;
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

    Json Json::Get(StringView key) const
    {
        Json result;
        if (!Contains(key))
        {
            return result;
        }

        result.m_Impl->data = m_Impl->data.at(String(key));
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

    void Json::Set(StringView key, const Json& value)
    {
        EnsureObject();
        m_Impl->data[String(key)] = value.m_Impl->data;
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

    String Json::GetString(StringView key, StringView def /* = {} */) const
    {
        if (!Contains(key) || !m_Impl->data.at(String(key)).is_string())
        {
            return String(def);
        }
        return m_Impl->data.at(String(key)).get<String>();
    }

    float Json::GetFloat(StringView key, float def /* = 0.f */) const
    {
        if (!Contains(key) || !m_Impl->data.at(String(key)).is_number())
        {
            return def;
        }
        return m_Impl->data.at(String(key)).get<float>();
    }

    bool Json::GetBool(StringView key, bool def /* = false */) const
    {
        if (!Contains(key) || !m_Impl->data.at(String(key)).is_boolean())
        {
            return def;
        }
        return m_Impl->data.at(String(key)).get<bool>();
    }

    int Json::GetInt(StringView key, int def /* = 0 */) const
    {
        if (!Contains(key) || !m_Impl->data.at(String(key)).is_number())
        {
            return def;
        }

        const nlohmann::json& value = m_Impl->data.at(String(key));
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

    glm::vec3 Json::GetVec3(StringView key, const glm::vec3& def /* = {} */) const
    {
        if (!Contains(key))
        {
            return def;
        }
        return ReadVec3(m_Impl->data.at(String(key)), def);
    }

    glm::vec4 Json::GetVec4(StringView key, const glm::vec4& def /* = {} */) const
    {
        if (!Contains(key))
        {
            return def;
        }
        return ReadVec4(m_Impl->data.at(String(key)), def);
    }

    int64 Json::GetInt64(StringView key, int64 def /* = 0 */) const
    {
        if (!Contains(key) || !m_Impl->data.at(String(key)).is_number_integer())
        {
            return def;
        }
        const nlohmann::json& value = m_Impl->data.at(String(key));
        if (value.is_number_unsigned() && value.get<uint64>() > static_cast<uint64>(INT64_MAX))
        {
            return def;
        }
        return value.get<int64>();
    }

    uint64 Json::GetUInt64(StringView key, uint64 def /* = 0 */) const
    {
        if (!Contains(key) || !m_Impl->data.at(String(key)).is_number_integer())
        {
            return def;
        }
        const nlohmann::json& value = m_Impl->data.at(String(key));
        if (!value.is_number_unsigned() && value.get<int64>() < 0)
        {
            return def;
        }
        return value.get<uint64>();
    }

    double Json::GetDouble(StringView key, double def /* = 0.0 */) const
    {
        if (!Contains(key) || !m_Impl->data.at(String(key)).is_number())
        {
            return def;
        }
        return m_Impl->data.at(String(key)).get<double>();
    }

    glm::vec2 Json::GetVec2(StringView key, const glm::vec2& def /* = {} */) const
    {
        if (!Contains(key))
        {
            return def;
        }
        const nlohmann::json& value = m_Impl->data.at(String(key));
        if (!value.is_array() || value.size() < 2 || !value.at(0).is_number() || !value.at(1).is_number())
        {
            return def;
        }
        return glm::vec2(value.at(0).get<float>(), value.at(1).get<float>());
    }

    glm::quat Json::GetQuat(StringView key, const glm::quat& def) const
    {
        if (!Contains(key))
        {
            return def;
        }
        const nlohmann::json& value = m_Impl->data.at(String(key));
        if (!value.is_array() || value.size() < 4)
        {
            return def;
        }
        for (std::size_t i = 0; i < 4; ++i)
        {
            if (!value.at(i).is_number())
            {
                return def;
            }
        }
        // glm::quat takes w first
        return glm::quat(
            value.at(3).get<float>(), value.at(0).get<float>(), value.at(1).get<float>(), value.at(2).get<float>());
    }

    void Json::Set(StringView key, const glm::vec2& v)
    {
        EnsureObject();
        m_Impl->data[String(key)] = nlohmann::json::array({v.x, v.y});
    }

    void Json::SetInt64(StringView key, int64 v)
    {
        EnsureObject();
        m_Impl->data[String(key)] = v;
    }

    void Json::SetUInt64(StringView key, uint64 v)
    {
        EnsureObject();
        m_Impl->data[String(key)] = v;
    }

    void Json::SetDouble(StringView key, double v)
    {
        EnsureObject();
        m_Impl->data[String(key)] = v;
    }

    void Json::SetQuat(StringView key, const glm::quat& v)
    {
        EnsureObject();
        m_Impl->data[String(key)] = nlohmann::json::array({v.x, v.y, v.z, v.w});
    }

    void Json::Remove(StringView key)
    {
        if (m_Impl->data.is_object())
        {
            m_Impl->data.erase(String(key));
        }
    }

    bool Json::IsString(StringView key) const
    {
        return Contains(key) && m_Impl->data.at(String(key)).is_string();
    }

    void Json::Set(StringView key, const glm::vec3& v)
    {
        EnsureObject();
        m_Impl->data[String(key)] = nlohmann::json::array({v.x, v.y, v.z});
    }

    void Json::Set(StringView key, const glm::vec4& v)
    {
        EnsureObject();
        m_Impl->data[String(key)] = nlohmann::json::array({v.x, v.y, v.z, v.w});
    }

    void Json::Set(StringView key, const char* v)
    {
        EnsureObject();
        m_Impl->data[String(key)] = v ? v : "";
    }

    void Json::Set(StringView key, StringView v)
    {
        EnsureObject();
        m_Impl->data[String(key)] = String(v);
    }

    void Json::Set(StringView key, float v)
    {
        EnsureObject();
        m_Impl->data[String(key)] = v;
    }

    void Json::Set(StringView key, bool v)
    {
        EnsureObject();
        m_Impl->data[String(key)] = v;
    }

    void Json::Set(StringView key, int v)
    {
        EnsureObject();
        m_Impl->data[String(key)] = v;
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
} // namespace Fling
