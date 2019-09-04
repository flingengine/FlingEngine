#include "pch.h"
#include "File.h"

namespace Fling
{
    File::File(Guid t_ID)
        : Resource(t_ID)
    {
        LoadFile();
    }

    void File::LoadFile()
    {
        const std::string FilePath = GetFilepathReleativeToAssets();
        
        std::ifstream File(FilePath, std::ios::ate | std::ios::binary);

        if (!File.is_open())
        {
            F_LOG_ERROR("Failed to open file: {}", FilePath);
        }
        else
        {
            size_t Filesize = (size_t)(File.tellg());
            m_Characters.resize(Filesize);
        
            File.seekg(0);
            File.read(m_Characters.data(), Filesize);
            File.close();
        }
    }
} // namespace Fling