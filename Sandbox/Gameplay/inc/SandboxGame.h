#pragma once

#include "Game.h"

namespace Sandbox
{
	/**
	 * @brief Custom game class that will have control of it's gameplay systems.
	 */
	class Game : public Fling::Game
	{

		void Init(entt::registry& t_Reg) override final;

		/**
		 * @brief 	Called before the first gameplay loop tick.
		 * 			Do any initalization for custom gameplay systems here.
		 */
		void OnStartGame(entt::registry& t_Reg) override final;

		/**
		* Update is called every frame. Call any system updates for your gameplay systems inside of here
		*/
		void Update(entt::registry& t_Reg, float DeltaTime) override final;

		/* Called when the engine is shutting down */
		void OnStopGame(entt::registry& t_Reg) override final;

		void Shutdown(entt::registry& t_Reg) override final;

		/**
		 * @brief Callback for when the user has given input that shows they want to exit
		 */
		void OnQuitPressed();

		/* Creates a 3D array of meshes */
		void GenerateTestMeshes(entt::registry& t_Reg);

		void LightingTest(entt::registry& t_Reg);

		/* Toggles the visibility of the cursor */
		void ToggleCursorVisibility();

	private:

		bool m_DoRotations = false;
		bool m_MovePointLights = false;

		void AddFloor(entt::registry& t_Reg, const std::string& t_Model, const std::string& t_Mat, const glm::vec3 t_Scale = glm::vec3(1.0f));

		void ToggleRotation();

		void OnTestSpawn();

		void OnToggleMoveLights();

		/** Temp vector for keeping track of the movement of things */
		glm::vec3 MoveDelta = {};

	};
}	// namespace Sandbox