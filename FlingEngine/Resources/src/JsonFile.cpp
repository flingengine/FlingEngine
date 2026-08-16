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
		if (m_JsonData.SaveToFile(FilePath))
		{
			F_LOG_TRACE("Successfully Wrote JSON file {}", FilePath);
		}
	}

	void JsonFile::LoadJsonFile()
	{
		m_JsonData.LoadFromFile(GetFilepathReleativeToAssets());
	}
}	// namespace Fling
