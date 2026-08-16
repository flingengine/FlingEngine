#pragma once

#include "Json.h"

#include <string>
#include <type_traits>
#include <utility>

namespace Fling
{
	template<typename T>
	struct NamedRef
	{
		const char* Name;
		T& Value;
	};

	template<typename T>
	NamedRef<T> MakeNVP(const char* name, T& value)
	{
		return { name, value };
	}

#define FLING_FIELD(Member) ::Fling::MakeNVP(#Member, Member)

	/**
	 * Bidirectional JSON archive. 
	 * 
	 * TODO: Make a base class archive to use for binary serialization as well, 
	 * thus opening us up to "cooked" level files :) 
	 */
	class JsonArchive
	{
	public:
		enum class Mode
		{
			Saving,
			Loading
		};

		JsonArchive(Json& doc, Mode mode)
			: m_Doc(doc)
			, m_Mode(mode)
		{
		}

		bool IsSaving() const { return m_Mode == Mode::Saving; }
		bool IsLoading() const { return m_Mode == Mode::Loading; }

		template<typename T>
		JsonArchive& operator<<(NamedRef<T> field);

	private:
		Json& m_Doc;
		Mode m_Mode;
	};

	template<typename T>
	JsonArchive& JsonArchive::operator<<(NamedRef<T> field)
	{
		if constexpr (std::is_same_v<T, float>)
		{
			if (IsSaving())
			{
				m_Doc.Set(field.Name, field.Value);
			}
			else
			{
				field.Value = m_Doc.GetFloat(field.Name, field.Value);
			}
		}
		else if constexpr (std::is_same_v<T, int>)
		{
			if (IsSaving())
			{
				m_Doc.Set(field.Name, field.Value);
			}
			else
			{
				field.Value = m_Doc.GetInt(field.Name, field.Value);
			}
		}
		else if constexpr (std::is_same_v<T, bool>)
		{
			if (IsSaving())
			{
				m_Doc.Set(field.Name, field.Value);
			}
			else
			{
				field.Value = m_Doc.GetBool(field.Name, field.Value);
			}
		}
		else if constexpr (std::is_same_v<T, std::string>)
		{
			if (IsSaving())
			{
				m_Doc.Set(field.Name, field.Value);
			}
			else
			{
				field.Value = m_Doc.GetString(field.Name, field.Value);
			}
		}
		else if constexpr (std::is_same_v<T, glm::vec3>)
		{
			if (IsSaving())
			{
				m_Doc.Set(field.Name, field.Value);
			}
			else
			{
				field.Value = m_Doc.GetVec3(field.Name, field.Value);
			}
		}
		else if constexpr (std::is_same_v<T, glm::vec4>)
		{
			if (IsSaving())
			{
				m_Doc.Set(field.Name, field.Value);
			}
			else
			{
				field.Value = m_Doc.GetVec4(field.Name, field.Value);
			}
		}
		else
		{
			static_assert(!sizeof(T), "Unsupported JsonArchive field type");
		}

		return *this;
	}

	template<typename T, typename = void>
	struct HasSerialize : std::false_type {};

	template<typename T>
	struct HasSerialize<T, std::void_t<decltype(std::declval<T&>().Serialize(std::declval<JsonArchive&>()))>>
		: std::true_type {};
}	// namespace Fling
