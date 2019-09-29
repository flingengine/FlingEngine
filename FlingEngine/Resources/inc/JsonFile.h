#pragma once

#include "Resource.h"

#include <sstream>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <nlohmann/json.hpp>

namespace Fling
{
    /**
     * @brief   A JsonFile provides an interface for easily using JSON files
     */
    class JsonFile : public Resource
    {
    public:

		static std::shared_ptr<Fling::JsonFile> Create(Guid t_ID);

        /**
         * @brief Construct a new JsonFile object
         * 
         * @param t_ID              The GUID that represents the file path to this JsonFile.
         */
        explicit JsonFile(Guid t_ID);
        
        /**
         * Get a reference to the current JSON data that is loaded from this file. 
         * @return  Reference to the current json data
         */
		nlohmann::json& GetJsonData() { return m_JsonData; }

		/**
		* @brief	Write the contents of this JSON file out to given name
		*/
		void Write();

    private:

		nlohmann::json m_JsonData;

        /**
         * @brief Loads the JsonFile based on Guid path.
         * @note All Guid paths are relative to the assets directory. 
         */
        void LoadJsonFile();
    };
}   // namespace Fling