#pragma once

#include "Platform.h"
#include "Guid.h"

namespace Fling
{
	/**
	* Base class that represents a loaded resource in the engine
	*/
	class FLING_API Resource
	{
	public:
		Resource() = default;
		virtual ~Resource() = default;

	private:

		/** GUID of this resource */
		Guid GUID;
	};
}	// namespace Fling