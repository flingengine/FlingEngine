#pragma once

// GLFW
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "Singleton.hpp"

namespace Fling
{
	class Renderer : public Singleton<Renderer>
	{
	public:

		virtual void Init() override;

		virtual void Shutdown() override;

		GLFWwindow* GetCurrentWindow() { return m_window; }

	private:

		/// <summary>
		/// Init GLFW and create the game window
		/// </summary>
		void InitWindow();

		/** The window that the game is being drawn to */
		GLFWwindow* m_window = nullptr;

	};

}	// namespace Fling