#pragma once

#include "Singleton.hpp"
#include "Guid.h"
#include "Resource.h"

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
		std::shared_ptr<T> AddResource(ARGS&& ... args)
		{



		}

		template <class T>
		std::shared_ptr<T> GetResourceOfType(const Guid& t_ID) const;

		std::shared_ptr<Resource> GetResource(const Guid& t_ID) const;

		/**
		* Check if there is a resource with this ID loaded or not
		* @return	If the resource ID is loaded or not
		*/
		bool IsLoaded(const Guid& t_ID) const;

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

		typedef std::map<Fling::Guid, std::shared_ptr<Resource>>::iterator ResourceMapIt;
		typedef std::map<Fling::Guid, std::shared_ptr<Resource>>::const_iterator ResourceMapConstIt;

		/** Map of currently loaded resources */
		std::map<Fling::Guid, std::shared_ptr<Resource>> m_ResourceMap;

	};


	template<class T>
	inline std::shared_ptr<T> ResourceManager::GetResourceOfType(const Guid& t_ID) const
	{
		if (std::shared_ptr<Resource> Res = GetResource(t_ID))
		{
			reinterpret_cast<std::shared_ptr<T>>(Res);
		}
		return nullptr;
	}
}	// namespace Fling