#pragma once

#include "FlingMath.h"
#include "Game.h"

#include <string>

namespace Sandbox
{
	/**
	 * Custom game class that will have control of it's gameplay systems.
	 */
	class Game : public Fling::Game
	{

		void Init(Fling::EntityStorage& t_Reg) override final;

		/** Adds the Sandbox component types and systems. Defined in RegisterSandbox.cpp. */
		void Register(Fling::ModuleRegistration& t_Modules) override final;

		/**
		 * Called before the first gameplay loop tick.
		 * 			Do any initalization for custom gameplay systems here.
		 */
		void OnStartGame(Fling::EntityStorage& t_Reg) override final;

        /** Called when play stops. Resets the state that is not in the registry. */
        void OnStopGame(Fling::EntityStorage& t_Reg) override final;

		void Shutdown(Fling::EntityStorage& t_Reg) override final;

		/* Creates a 3D array of meshes */
		void GenerateTestMeshes(Fling::EntityStorage& t_Reg);

		void LightingTest(Fling::EntityStorage& t_Reg);

	private:

		void AddFloor(Fling::EntityStorage& t_Reg, const std::string& t_Model, const std::string& t_Mat, const glm::vec3 t_Scale = glm::vec3(1.0f));

    };
}	// namespace Sandbox