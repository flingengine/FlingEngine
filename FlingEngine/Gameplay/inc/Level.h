#pragma once

#include "NonCopyable.hpp"

#include <string>

namespace Fling
{
    class World;

    /**
     * @brief   A level contains active objects and provides the environment
     *          for the player. You should only load a level through the world. 
     */
    class Level : public NonCopyable
    {
    public:
		/**
		* Loads this level based on the given file name. 
		* @param t_LevelFile		The path to the level file (should be a full path, NOT relative to assets dir)
		*/
        explicit Level(const std::string& t_LevelFile, World* t_OwningWorld);
        ~Level();

        /**
         * @brief   Update the BSP of actors and tick every active actor. 
         * @see     World::Update
         * 
         * @param t_DeltaTime   Time between previous frame and the current one. 
         */
        void Update(float t_DeltaTime);

        /**
         * @brief Unload the current level and all actors inside of it
         */
        void Unload();

        /**
         * @brief Get the Owning World object of this level. 
         * 
         * @return World* 
         */
        World* GetOwningWorld() const { return m_OwningWorld; }

    private:

        // BSPTree m_Model;
        // std::vector<Actor> m_ActiveActors;

		/** The path to the level file (should be a full path, NOT relative to assets dir) */
        std::string m_LevelFileName = "UNLOADED";

        /**
         * @brief Load the level based on the current file name! 
         */
        void LoadLevel();

        /**
         * @brief Any behavior that needs to happen after the level has been fully loaded.
         */
        void PostLoad();

        /** The owning work that this level exists in */
        World* m_OwningWorld = nullptr;
    };
}   // namespace Fling