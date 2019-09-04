#pragma once

#include "Resource.h"

#include <fstream>
#include <vector>
#include <map>

namespace Fling
{
    /**
     * @brief   A file is a basic text file that contains a basic string 
     */
    class File : public Resource
    {
    public:

        /**
         * @brief Construct a new File object
         * 
         * @param t_ID The GUID that represents the file path to this file. 
         * @note All file paths are relative to the ASSETS directory.
         */
        explicit File(Guid t_ID);
        
        const char* GetData() const { return m_Characters.data(); }

        size_t GetFileLength() const { return m_Characters.size(); }

    private:

        /**
         * @brief Loads the file based on Guid path. 
         * @note All Guid paths are relative to the assets directory. 
         */
        void LoadFile();

        /** Array of characters that represents this file */
        std::vector<char> m_Characters;
    };
}   // namespace Fling