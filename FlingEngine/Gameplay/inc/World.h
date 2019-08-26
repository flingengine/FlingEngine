#pragma once

#include "Singleton.hpp"
#include "Level.h"

#include <string>
#include <vector>
#include <memory>

namespace Fling
{
	/**
	* The world holds all active levels in the game. There will only ever be exactly one World
	* instance at any given time. 
	* @see Level
	*/
    class World : public Singleton<World>
    {
    public: 
		/**
		* @brief	Initializes the world. Loads the StartLevel that is specified in the config.  
		*/
        virtual void Init() override;

        virtual void Shutdown() override;

		/**
		* Called before the first Update tick on the world. 
		*/
		void PreTick();

        /**
         * @brief   Tick all active levels in the world
         * 
         * @param t_DeltaTime   Time between previous frame and the current one. 
         */
        void Update(float t_DeltaTime);

		/** 
		* @brief				Load a level into the world
		* @param t_LevelPath	The path to the current level. (Relative to the assets dir)
		*/
        void LoadLevel(const std::string& t_LevelPath);

    private:

		// #TODO: Pointer to the player!

        /** Currently active levels in the world */
        std::vector<std::unique_ptr<Level>> m_ActiveLevels;
    };
} // namespace Fling