#include "pch.h"
#include "Level.h"

namespace Fling
{
    Level::Level(const std::string& t_LevelFile)
    {
        // Load in a file resource based on this file
        LoadLevel();

        // Parse the file appropriately!
        PostLoad();
    }

    void Level::LoadLevel()
    {

    }

    void Level::PostLoad()
    {

    }

    Level::~Level()
    {
        // Clean up any actors in this level
        Unload();
    }

    void Level::Update(float t_DeltaTime)
    {   
        // Update the BSP of actors
        // m_Model.Update(t_DeltaTime);
        // Tick an active actors
        // for(Actor* : m_ActiveActors) { ... }
    }

    void Level::Unload()
    {

    }
}   // namespace Fling