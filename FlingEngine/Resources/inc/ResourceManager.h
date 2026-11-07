#pragma once

#include "AssetPath.h"
#include "Resource.h"
#include "Singleton.hpp"

#include <map>
#include <memory>

namespace Fling
{
	/**
	 * The resource manager handles loading of files off disk. Every Resource has an
	 * AssetPath, and is keyed by the AssetId of that path. Each resource is only ever
	 * loaded into memory ONCE.
	 *
	 * @see Fling::AssetPath
	 * @see Fling::Resource
	 */
	class ResourceManager : public Singleton<ResourceManager>
	{
	public:

		virtual void Init() override;

		virtual void Shutdown() override;

		template<class T, class ...ARGS>
		static std::shared_ptr<T> LoadResource(const AssetPath& t_Path, ARGS&& ... args)
		{
			return ResourceManager::Get().LoadResourceImpl<T>(t_Path, std::forward<ARGS>(args)...);
		}

		template <class T>
		std::shared_ptr<T> GetResourceOfType(AssetId t_ID) const;

		/**
		 * Get the already loaded resouce with this id. Returns nullptr if not loaded yet.
		 *
		 * @param t_ID 	Id of the resource
		 * @return std::shared_ptr<Resource> Pointer to the resource
		 */
		std::shared_ptr<Resource> GetResource(AssetId t_ID) const;

		/**
		* Check if there is a resource with this ID loaded or not
		* @return	If the resource ID is loaded or not
		*/
		bool IsLoaded(AssetId t_ID) const;

	private:

		template<class T, class ...ARGS>
		std::shared_ptr<T> LoadResourceImpl(const AssetPath& t_Path, ARGS&& ... args);

		typedef std::map<Fling::AssetId, std::shared_ptr<Resource>>::iterator ResourceMapIt;
		typedef std::map<Fling::AssetId, std::shared_ptr<Resource>>::const_iterator ResourceMapConstIt;
        
		///** Map of currently loaded resources */
		std::map<Fling::AssetId, std::shared_ptr<Resource>> m_ResourceMap;
	};


	template<class T, class ...ARGS>
	inline std::shared_ptr<T> ResourceManager::LoadResourceImpl(const AssetPath& t_Path, ARGS&& ... args)
	{
		// If this resource exists already then just return that
		const AssetId ID = t_Path.GetId();
		if (std::shared_ptr<T> Existing = GetResourceOfType<T>(ID))
		{
			return Existing;
		}

		// Create a new resource of type T and return it
		// Every resource type has an explict CTOR whose first arg has to be its path
		std::shared_ptr<Resource> NewResource = std::make_shared<T>(t_Path, std::forward<ARGS>(args)...);

		// Keep track of this resource in the map
		m_ResourceMap[ID] = NewResource;
		return std::static_pointer_cast<T>( NewResource );
	}

	template<class T>
	inline std::shared_ptr<T> ResourceManager::GetResourceOfType(AssetId t_ID) const
	{
		if (std::shared_ptr<Resource> Res = GetResource(t_ID))
		{
			return std::static_pointer_cast<T>(Res);
		}
		return nullptr;
	}
}	// namespace Fling