#pragma once

#include "Singleton.hpp"
#include "Resource.h"
#include "FlingTypes.h" // Guid

#include <fstream>
#include <vector>
#include <map>

namespace Fling
{
	/**
	 * @brief The resource manager handles loading of files off disk. Every Resource type
	 * has a Guid. This Guid functions as both the file path (relative to the ASSETS directory)
	 * as well as a hashed string for easy passing around of information. Each resource is only 
	 * ever loaded into memory ONCE.
	 * 
	 * @see Fling::Guid
	 * @see Fling::Guid_Handle
	 * @see Fling::Resource
	 */
	class ResourceManager : public Singleton<ResourceManager>
	{
	public:

		virtual void Init() override;

		virtual void Shutdown() override;

		template<class T, class ...ARGS>
		static std::shared_ptr<T> LoadResource(Guid t_ID, ARGS&& ... args)
		{
			return ResourceManager::Get().LoadResourceImpl<T>(t_ID, std::forward<ARGS>(args)...);
		}

		template <class T>
		std::shared_ptr<T> GetResourceOfType(Guid_Handle t_ID) const;

		/**
		 * @brief Get the already loaded resouce with this Guid. Returns nullptr if not loaded yet. 
		 * 
		 * @param t_ID 	Guid of the resource (a hashed string handle)
		 * @return std::shared_ptr<Resource> Pointer to the resource
		 */
		std::shared_ptr<Resource> GetResource(Guid_Handle t_ID) const;

		/**
		* Check if there is a resource with this ID loaded or not
		* @return	If the resource ID is loaded or not
		*/
		bool IsLoaded(Guid_Handle t_ID) const;

	private:

		template<class T, class ...ARGS>
		std::shared_ptr<T> LoadResourceImpl(Guid t_ID, ARGS&& ... args);

		typedef std::map<Fling::Guid_Handle, std::shared_ptr<Resource>>::iterator ResourceMapIt;
		typedef std::map<Fling::Guid_Handle, std::shared_ptr<Resource>>::const_iterator ResourceMapConstIt;
        
		///** Map of currently loaded resources */
		std::map<Fling::Guid_Handle, std::shared_ptr<Resource>> m_ResourceMap;
	};


	template<class T, class ...ARGS>
	inline std::shared_ptr<T> ResourceManager::LoadResourceImpl(Guid t_ID, ARGS&& ... args)
	{
		// If this resource exists already then just return that
		if (std::shared_ptr<T> Existing = GetResourceOfType<T>(t_ID))
		{
			return Existing;
		}

		// Create a new resource of type T and return it
		// Every resource type has an explict CTOR whose first arg has to be an ID
		std::shared_ptr<Resource> NewResource = std::make_shared<T>(t_ID, std::forward<ARGS>(args)...);

		// Keep track of this resource in the map
		m_ResourceMap[t_ID] = NewResource;
		return std::static_pointer_cast<T>( NewResource );
	}

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