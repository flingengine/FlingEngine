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

		/// <summary>
		/// Init GLFW and create the game window
		/// </summary>
		/// <param name="t_width">Width of the window</param>
		/// <param name="t_height">Height of the window</param>
		void CreateGameWindow( const int t_width, const int t_height );

	private:

		/// <summary>
		/// Init the current graphics API
		/// </summary>
		void InitGraphics();

		/// <summary>
		/// Create a vulkan instance
		/// </summary>
		void CreateGraphicsInstance();

		/// <summary>
		/// Determine if all requested layers are available.
		/// </summary>
		/// <returns>Returns true if layers are found, false otherwise</returns>
		bool CheckValidationLayerSupport();

		/// <summary>
		/// Configure graphics API debug callbacks [Vulkan]
		/// </summary>
		void SetupDebugMessesages();

		std::vector<const char*> GetRequiredExtensions();

		static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
			VkDebugUtilsMessageSeverityFlagBitsEXT t_messageSeverity,
			VkDebugUtilsMessageTypeFlagsEXT t_messageType,
			const VkDebugUtilsMessengerCallbackDataEXT* t_CallbackData,
			void* t_UserData
		);

		VkResult CreateDebugUtilsMessengerEXT(
			VkInstance instance,
			const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
			const VkAllocationCallbacks* pAllocator,
			VkDebugUtilsMessengerEXT* pDebugMessenger );

		void DestroyDebugUtilsMessengerEXT( 
			VkInstance instance,
			VkDebugUtilsMessengerEXT debugMessenger,
			const VkAllocationCallbacks* pAllocator );

		/** The window that the game is being drawn to */
		GLFWwindow* m_window = nullptr;

		VkInstance m_instance = VK_NULL_HANDLE;

		VkDebugUtilsMessengerEXT m_debugMessenger;

		/** Width of the window that GLFW creates */
		int m_width = 800;
		/** Height of the window that GLFW creates */
		int m_height = 600;

#ifdef NDEBUG
		const bool m_enableValidationLayers = false;
#else
		const bool m_enableValidationLayers = true;
#endif

		const std::vector<const char*> m_validationLayers =
		{
			"VK_LAYER_KHRONOS_validation"
		};

	public:

		GLFWwindow* Window() const { return m_window; }

	};

}	// namespace Fling