#include "JsonFile.h"
#include "Logger.h"
#include "ResourceManager.h"
#include "Strings/String.h"

namespace Fling
{
	std::shared_ptr<Fling::JsonFile> JsonFile::Create(const AssetPath& t_Path)
	{
		return ResourceManager::LoadResource<Fling::JsonFile>(t_Path);
	}

	JsonFile::JsonFile(const AssetPath& t_Path)
		: Resource(t_Path)
	{
		LoadJsonFile();
	}

	void JsonFile::Write()
	{
		const String FilePath = GetFilepathReleativeToAssets();
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
