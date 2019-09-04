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

		/**
		 * @brief 	Get GUID handle (just an int) for this resources guid. Use this to pass around 
		 * 			to different functions instead of the whole GUID
		 * 
		 * @return Fling::Guid_Handle 
		 */
		Fling::Guid_Handle GetGuidHandle() const { return m_Guid; }

		/**
		 * @brief Get the human-readable string representation of this GUID
		 * 
		 * @return std::string 
		 */
		std::string GetGuidString() const { return m_Guid.data(); }

    protected:

        Fling::Guid m_Guid;
	};
}	// namespace Fling