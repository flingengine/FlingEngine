#pragma once

#include "Platform.h"
#include "FlingTypes.h"

namespace Fling
{
	/**
	* Base class that represents a loaded resource in the engine
	*/
	class Resource
	{
		friend class ResourceManager;

	public:
        Resource(Fling::Guid t_ID)
            : m_Guid(t_ID)
        { }
		
        Resource() = default;

        virtual ~Resource() = default;
    
    protected:

        Fling::Guid m_Guid;
	};
}	// namespace Fling