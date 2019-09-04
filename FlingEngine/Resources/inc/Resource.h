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
        explicit Resource(Fling::Guid t_ID)
            : m_Guid(t_ID)
        { }
		
        virtual ~Resource() = default;

		Fling::Guid_Handle GetHUIDHandle() const {return m_Guid; }

		std::string GetGuidString() const { return m_Guid.data(); }

    protected:

        Fling::Guid m_Guid;
	};
}	// namespace Fling