#include "pch.h"
#include "JsonFile.h"
#include "ResourceManager.h"

namespace Fling
{
	std::shared_ptr<Fling::JsonFile> JsonFile::Create(Guid t_ID)
	{
		return ResourceManager::LoadResource<Fling::JsonFile>(t_ID);
	}

	JsonFile::JsonFile(Guid t_ID)
        : Resource(t_ID)
    {
        LoadJsonFile();
    }

	void JsonFile::Write()
	{
		const std::string FilePath = GetFilepathReleativeToAssets();

		std::ofstream OutStream(FilePath);
		if (OutStream.is_open())
		{
			OutStream << std::setw(4) << m_JsonData << std::endl;
            F_LOG_TRACE("Successfully Wrote JSON file {}", FilePath);
		}
        else
        {
            F_LOG_ERROR("Failed to write JSON file {}", FilePath);
        }

		OutStream.close();
	}

	void JsonFile::LoadJsonFile()
    {
        const std::string FilePath = GetFilepathReleativeToAssets();

        F_LOG_TRACE("Attempting to Load JSON {}", FilePath);

        std::ifstream ifs(FilePath.c_str());

        if (ifs.is_open())
        {
            // Store the info in the scene file in the JSON object
            ifs >> m_JsonData;
            F_LOG_TRACE("Successfully Loaded JSON file {}", FilePath);
        }
        else
        {
            F_LOG_ERROR( "Failed to load JSON File: {}", FilePath);
        }

        ifs.close();

    }
} // namespace Fling