#pragma once

#include "Resource.h"

#include <fstream>
#include <vector>

namespace Fling
{
    /**
     * A file is a basic text file that contains a basic text file
     */
    class File : public Resource
    {
    public:

		static std::shared_ptr<Fling::File> Create(Guid t_ID);

        /**
         * Load a file from the asset path represented by t_ID.
         * 
         * @param t_ID              The GUID that represents the file path to this file.
         */
        explicit File(Guid t_ID);
        
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
         * Loads the file based on Guid path.
         * @note All Guid paths are relative to the assets directory. 
         */

        void LoadFile();

        /** Array of characters that represents this file */
        std::vector<char> m_Characters;
    };
}   // namespace Fling