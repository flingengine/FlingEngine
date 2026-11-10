#pragma once

#include "Json.h"
#include "Resource.h"

#include <memory>

namespace Fling
{
    /**
     * A JsonFile provides an interface for easily using JSON files
     */
    class JsonFile : public Resource
    {
    public:
        static std::shared_ptr<Fling::JsonFile> Create(const AssetPath& t_Path);

        /**
         * Load a JSON file from the asset path t_Path.
         *
         * @param t_Path              The path of this JSON file, relative to the assets folder.
         */
        explicit JsonFile(const AssetPath& t_Path);

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
         * Loads the JsonFile from its path.
         * @note All paths are relative to the assets directory.
         */
        void LoadJsonFile();
    };
} // namespace Fling
