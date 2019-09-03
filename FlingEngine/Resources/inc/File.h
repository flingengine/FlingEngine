#pragma once

#include "Resource.h"

#include <fstream>
#include <vector>
#include <map>

namespace Fling
{
    /**
     * @brief   An File represents a 2D file that has data about each pixel in the File
     */
    class File : public Resource
    {
    public:
        //File() = default;
        explicit File(Guid t_ID);
        
    private:

        void LoadFile();

        /** Array of characters that represents this file */
        std::vector<char> m_Characters;
    };
}   // namespace Fling