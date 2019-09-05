#include "pch.h"
#include "JsonFile.h"

namespace Fling
{
    JsonFile::JsonFile(Guid t_ID)
        : Resource(t_ID)
    {
        LoadJsonFile();
    }

    void JsonFile::LoadJsonFile()
    {
        const std::string FilePath = GetFilepathReleativeToAssets();

        std::ifstream ifs(FilePath.c_str());

        if (ifs.is_open())
        {
            // Store the info in the scene file in the JSON object
            ifs >> m_JsonData;
        }
        else
        {
            F_LOG_ERROR( "Failed to load JSON File: {}", FilePath);
        }

        ifs.close();

    }
} // namespace Fling