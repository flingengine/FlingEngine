#include "pch.h"
#include "ResourceManager.h"

namespace Fling
{
    void ResourceManager::Init()
    {
        
    }

    void ResourceManager::Shutdown()
    {

    }

    std::vector<char> ResourceManager::ReadFile(const char* t_Filename)
    {
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