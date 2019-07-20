#include "pch.h"
#include "ResourceManager.h"
// #TODO Create a platform abstraction
#ifndef _WIN32
#include <unistd.h>
#endif
namespace Fling
{
    void ResourceManager::Init()
    {
        // Thank god for Chris
#ifdef _WIN32
        // Ensure "Current Directory" (relative path) is always the .exe's folder
        // - Without this, the relative path is different when running through VS
        //    and when running the .exe directly, which makes it a pain to load files
        //    - Running through VS: Current Dir is the *project folder*
        //    - Running from .exe:  Current Dir is the .exe's folder
        // - This has nothing to do with DEBUG and RELEASE modes - it's purely a 
        //    Visual Studio "thing", and isn't obvious unless you know to look 
        //    for it.  In fact, it could be fixed by changing a setting in VS, but
        //    the option is stored in a user file (.suo), which is ignored by most
        //    version control packages by default.  Meaning: the option must be
        //    changed every on every PC.  Ugh.  So instead, I fixed it here.
        // - This is a new change this year to simplify a long-standing headache.  
        //    If it breaks something on your end, feel free to comment this section out
        {
            // Get the real, full path to this executable, end the string before
            // the filename itself and then set that as the current directory
            char currentDir[1024] = {};
            GetModuleFileName(0, currentDir, 1024);
            char* lastSlash = strrchr(currentDir, '\\');
            if (lastSlash)
            {
                *lastSlash = 0; // End the string at the last slash character
                SetCurrentDirectory(currentDir);
            }
        }
#else
        {
            char cwd[1024] = {};
            if (getcwd(cwd, 1024) != nullptr) 
            {
                F_LOG_TRACE("Current working dir: {}\n", cwd);
            }
            else 
            {
                F_LOG_FATAL("getcwd() error");
            }

            if (chdir(cwd) == -1) 
            {
                F_LOG_FATAL("chdir() error");
            }
        }
#endif
    }

    void ResourceManager::Shutdown()
    {

    }

    std::vector<char> ResourceManager::ReadFile(const std::string& t_Filename)
    {
        F_LOG_TRACE("Attempt to Read file: {}", t_Filename);

        std::ifstream File(t_Filename, std::ios::ate | std::ios::binary);

        if (!File.is_open())
        {
            F_LOG_FATAL("Failed to open file! ");
        }

        size_t Filesize = (size_t)(File.tellg());
        std::vector<char> Buffer(Filesize);

        File.seekg(0);
        File.read(Buffer.data(), Filesize);
        File.close();

        return Buffer;
    }
}