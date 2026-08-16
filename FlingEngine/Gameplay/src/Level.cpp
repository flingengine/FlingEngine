#include "pch.h"
#include "Level.h"
#include "World.h"

namespace Fling
{
	Level::Level(const std::string& t_LevelFile, World* t_OwningWorld)
		: m_LevelFileName(t_LevelFile)
		, m_OwningWorld(t_OwningWorld)
	{
		LoadLevel();
		PostLoad();
	}

	void Level::LoadLevel()
	{
		F_LOG_TRACE("Level::LoadLevel is unused; levels load via World::LoadLevelFile");
	}

	void Level::PostLoad()
	{
		F_LOG_TRACE("Level '{}' Post Load", m_LevelFileName);
	}

	Level::~Level()
	{
		Unload();
	}

	void Level::Update(float t_DeltaTime)
	{
		(void)t_DeltaTime;
	}

	void Level::Unload()
	{
		F_LOG_TRACE("Unload Level '{}'", m_LevelFileName);
	}
}	// namespace Fling
