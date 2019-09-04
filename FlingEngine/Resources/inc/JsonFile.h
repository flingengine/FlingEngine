#pragma once

#include "Resource.h"

#include <fstream>
#include <nlohmann/json.hpp>

namespace Fling
{
    using json = nlohmann::json;

    /**
     * @brief   A JsonFile provides an interface for easily using JSON files
     */
    class JsonFile : public Resource
    {
    public:

        /**
         * @brief Construct a new JsonFile object
         * 
         * @param t_ID              The GUID that represents the file path to this JsonFile.
         */
        explicit JsonFile(Guid t_ID);
        
        /**
         * Get a reference to the current Json data that is loaded from this file. 
         * @return  Reference to the current json data
         */
        json& GetJsonData() { return m_JsonData; }

    private:

        json m_JsonData;

        /**
         * @brief Loads the JsonFile based on Guid path.
         * @note All Guid paths are relative to the assets directory. 
         */
        void LoadJsonFile();

    };
}   // namespace Fling