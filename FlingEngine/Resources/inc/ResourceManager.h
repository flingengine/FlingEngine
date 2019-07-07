#pragma once

#include "Singleton.hpp"
#include <fstream>
#include <vector>

namespace Fling
{
    /**
    * The resource manager handles loading in of any file resources	from disk.
    */
    class ResourceManager : public Singleton<ResourceManager>
    {
    public:
        virtual void Init() override;

        virtual void Shutdown() override;

        /**
        * Read in the given file name and place it's contents into a vector
        * of char's
        * 
        * @param 	Name of the file to read in	
        *
        * @return   A vector of char's that are the contents of the file
        */
        static std::vector<char> ReadFile(const std::string& t_Filename);
        
        // #TODO Make a file level abstraction for handling
        
        // #TODO Assign every resource that is loaded in a GUID to ensure only 1 is loaded
        // #TODO Allow users to specific the custom allocators that may be needed in the future
        // @see Chap. 8 of Game Engine arch
    };

}   // namespace Fling
