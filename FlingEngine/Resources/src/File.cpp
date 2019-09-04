#include "pch.h"
#include "Image.h"
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
        const std::string t_Filename = FlingPaths::EngineAssetsDir() + GetGuidString();
        
        std::ifstream File(t_Filename, std::ios::ate | std::ios::binary);

        F_LOG_WARN("Load file: {}", t_Filename);

        if (!File.is_open())
        {
            F_LOG_ERROR("Failed to open file: {}", t_Filename);
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