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
        { 
			m_HumanReadableName = m_Guid.data();
		}
		
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
		const std::string& GetGuidString() const { return m_HumanReadableName; }

        /**
         * @brief   Returns the full file path that is relative to the assets path based on the GUID of this resource. 
         */
        std::string GetFilepathReleativeToAssets() const;

    protected:

        Fling::Guid m_Guid;

		std::string m_HumanReadableName;
	};
}	// namespace Fling