#pragma once

#include "Input.h"

namespace Fling
{
	template<auto Candidate, typename Type>
	void Input::BindKeyPress(const std::string& t_KeyName, Type& t_Instance)
	{
		// Make a new delegate if we have to
		if(m_KeyDownMap.find(t_KeyName) == m_KeyDownMap.end())
		{
            entt::delegate<void()> delegate {};
			m_KeyDownMap.insert( KeyDownMapPair(t_KeyName, delegate) );
		}

        // Bind the function
        m_KeyDownMap[t_KeyName].connect<Candidate>(t_Instance);
	}
}   // namespace Fling