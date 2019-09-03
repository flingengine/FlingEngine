#pragma once

#include "Platform.h"
#include "FlingTypes.h"

namespace Fling
{
	/**
	* Base class that represents a loaded resource in the engine
	*/
	class FLING_API Resource
	{
		friend class ResourceManager;

	public:
		Resource() = default;
		virtual ~Resource() = default;

	protected:

		/** GUID of this resource */
		Guid m_GUID;
	};
}	// namespace Fling