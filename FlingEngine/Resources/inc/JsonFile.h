#pragma once

#include "Resource.h"
#include "Json.h"

#include <memory>

namespace Fling
{
	/**
	 * A JsonFile provides an interface for easily using JSON files
	 */
	class JsonFile : public Resource
	{
	public:

		static std::shared_ptr<Fling::JsonFile> Create(Guid t_ID);

		/**
		 * Load a JSON file from the asset path represented by t_ID.
		 *
		 * @param t_ID              The GUID that represents the file path to this JsonFile.
		 */
		explicit JsonFile(Guid t_ID);

		virtual ~JsonFile() = default;

		/**
		 * Get a reference to the current JSON data that is loaded from this file.
		 * @return  Reference to the current json data
		 */
		FORCEINLINE Json& GetJsonData() { return m_JsonData; }

		/**
		* Write the contents of this JSON file out to given name
		*/
		void Write();

	protected:

		Json m_JsonData;

		/**
		 * Loads the JsonFile based on Guid path.
		 * @note All Guid paths are relative to the assets directory.
		 */
		void LoadJsonFile();
	};
}	// namespace Fling
