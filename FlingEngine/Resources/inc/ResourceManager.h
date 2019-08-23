#pragma once

#include "Singleton.hpp"
#include "Guid.h"
#include "Resource.h"

#include <fstream>
#include <vector>
#include <unordered_map>
#include <map>

namespace Fling
{
	class ResourceManager : public Singleton<ResourceManager>
	{
	public:

		virtual void Init() override;

		virtual void Shutdown() override;


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

		std::map<Fling::Guid, std::shared_ptr<Resource>> m_ResourceMap;

	};


}	// namespace Fling