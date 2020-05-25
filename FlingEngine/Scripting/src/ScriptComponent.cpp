#if WITH_LUA
#include "pch.h"
#include "ScriptComponent.h"

namespace Fling
{
	ScriptComponent::ScriptComponent(const std::string t_FilePath)
	{
		//Convert the filepath into a GUID
		m_ScriptFile = File::Create(HS(t_FilePath.c_str())).get();
	}
}
#endif
