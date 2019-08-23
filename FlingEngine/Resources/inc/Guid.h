#pragma once

#include "pch.h"

#define INVALID_GUID	0

namespace Fling
{
	/**
	 * A globally unique ID for resources in the Fling Engine.
	 */
	struct FLING_API Guid
	{
		Guid(UINT32 X) : ID(X) {}

		/** The ID used for this resource */
		UINT32 ID;

		static Guid NewGuid();

		/**
		 * Check if this ID is valid
		 * @return	true if the GUID is not the static invalid
		 */
		bool IsValid() const;

		/**
		 * Make this GUID invalid
		 */
		void Invalidate();

		std::string ToString() const;

		friend bool operator==(const Guid& X, const Guid& Y)
		{
			return (X.ID == Y.ID);
		}

		friend bool operator!=(const Guid& X, const Guid& Y)
		{
			return (X.ID != Y.ID);
		}

		friend bool operator<(const Guid& X, const Guid& Y)
		{
			return (X.ID < Y.ID);
		}

	protected:
		Guid()
			: ID(INVALID_GUID)
		{ }

	};
}	// namespace Fling