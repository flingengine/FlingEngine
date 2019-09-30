#pragma once

#include "World.h"
#include "Components/Transform.h"

namespace Fling
{
	template<class ...ARGS>
	bool World::OutputLevelFile(const std::string& t_LevelToLoad)
	{
		std::string FullPath = FlingPaths::EngineAssetsDir() + "/" + t_LevelToLoad;

		std::ofstream OutStream(FullPath);
		if(!OutStream.is_open())
		{
			F_LOG_ERROR("Failed to open out stream to level {}", FullPath);
			return false;
		}
		
		F_LOG_TRACE("Outputting Level file to {}", FullPath);

    	cereal::JSONOutputArchive archive(OutStream);

		// Write out a copy of what is in this registry 
		m_Registry.snapshot()
			.entities(archive)
			.component<Fling::Transform, ARGS...>(archive);
		
		return true;
	}
	
	template<class ...ARGS>
	bool World::LoadLevelFile(const std::string& t_LevelToLoad)
	{
		std::string FullPath = FlingPaths::EngineAssetsDir() + "/" + t_LevelToLoad;

		F_LOG_TRACE("Load Scene file to: {}", FullPath);

		// Create a cereal input stream
		std::ifstream InputStream(FullPath);
		if(!InputStream.is_open())
		{
			F_LOG_ERROR("Failed to open input stream from file: {}", FullPath);
			return false;
		}

		F_LOG_TRACE("Loading Level file from {}", FullPath);

    	cereal::JSONInputArchive archive(InputStream);

		// This type of loader requires the registry to be cleared first
		m_Registry.reset();

		// Load into thre registry based on the serialization from the stream
		m_Registry.loader()
			.entities(archive)
			.component<Fling::Transform, ARGS...>(archive);
		
		return true;
	}
}