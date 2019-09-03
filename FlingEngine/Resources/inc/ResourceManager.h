#pragma once

#include "Singleton.hpp"
#include "Resource.h"
#include "FlingTypes.h" // Guid

#include <fstream>
#include <vector>
#include <map>

namespace Fling
{
	class ResourceManager : public Singleton<ResourceManager>
	{
	public:

		virtual void Init() override;

		virtual void Shutdown() override;

		template<class T, class ...ARGS>
		std::shared_ptr<T> LoadResource(Guid t_ID, ARGS&& ... args)
		{
            // If this resource exists already then just return that
            if (std::shared_ptr<T> Existing = GetResourceOfType<T>(t_ID))
            {
                return Existing;
            }

            // Create a new resource of type T and return it
            std::shared_ptr<Resource> NewResource = std::make_shared<T>(t_ID, std::forward<ARGS>(args)...);
            //NewResource->m_Guid = t_ID;

            // Keep track of this resource in the map
            m_ResourceMap[t_ID] = NewResource;
            return std::static_pointer_cast<T>( NewResource );
		}

		template <class T>
		std::shared_ptr<T> GetResourceOfType(Guid_Handle t_ID) const;

		std::shared_ptr<Resource> GetResource(Guid_Handle t_ID) const;

		/**
		* Check if there is a resource with this ID loaded or not
		* @return	If the resource ID is loaded or not
		*/
		bool IsLoaded(Guid_Handle t_ID) const;

		// #TODO Make this a file resource
		/**
		* Read in the given file name and place it's contents into a vector
		* of char's
		*
		* @param 	Name of the file to read in
		*
		* @return   A vector of char's that are the contents of the file
		*/
		static std::vector<char> ReadFile(const std::string& t_Filename);

	private:

		typedef std::map<Fling::Guid_Handle, std::shared_ptr<Resource>>::iterator ResourceMapIt;
		typedef std::map<Fling::Guid_Handle, std::shared_ptr<Resource>>::const_iterator ResourceMapConstIt;
        
		///** Map of currently loaded resources */
		std::map<Fling::Guid_Handle, std::shared_ptr<Resource>> m_ResourceMap;
	};

	template<class T>
	inline std::shared_ptr<T> ResourceManager::GetResourceOfType(Guid_Handle t_ID) const
	{
		if (std::shared_ptr<Resource> Res = GetResource(t_ID))
		{
			return std::static_pointer_cast<T>(Res);
		}
		return nullptr;
	}
}	// namespace Fling