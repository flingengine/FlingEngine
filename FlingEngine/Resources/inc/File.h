#pragma once

#include "Resource.h"

#include <memory>
#include <vector>

namespace Fling
{
    /**
     * A file is a basic text file that contains a basic text file
     */
    class File : public Resource
    {
    public:
        static std::shared_ptr<Fling::File> Create(const AssetPath& t_Path);

        /**
         * Load a file from the asset path t_Path.
         *
         * @param t_Path              The path of this file, relative to the assets folder.
         */
        explicit File(const AssetPath& t_Path);

        /**
         * Get char* that represents the text in this file
         *
         * @return const char*
         */
        const char* GetData() const { return m_Characters.data(); }

        /**
         * Get the File Length object
         *
         * @return size_t Length of the file in characters
         */
        size_t GetFileLength() const { return m_Characters.size(); }

        /**
         * Returns true if this file resource is loaded or not (i.e. has any characters in the file)
         */
        bool IsLoaded() const { return m_Characters.size() != 0; }

    private:
        /**
         * Loads the file from its path.
         * @note All paths are relative to the assets directory.
         */

        void LoadFile();

        /** Array of characters that represents this file */
        std::vector<char> m_Characters;
    };
} // namespace Fling