#pragma once

#include "pch.h"
#include "FlingTypes.h"
#include "NonCopyable.hpp"

enum WindowMode 
{ 
    Fullscreen, 
    Windowed, 
    BorderlessWindowed 
};

namespace Fling
{
	/** Window creation data */
	struct WindowProps
	{
		UINT32 m_Width = 800;
		UINT32 m_Height = 600;
		std::string m_Title = "Fling Engine";
	};

	/**
	* Base class that represents a window to the Fling Engine
	*/
	class FlingWindow : public NonCopyable
	{
	public:
		
		/** Create a window with the given data. Implemented per platform */
		static FlingWindow* Create(const WindowProps& t_Props);

		virtual ~FlingWindow() = default;

		/** Per frame update of this window. */
		virtual void Update() = 0;

		/** Create the rendering surface for this window */
		virtual void CreateSurface(void* t_GraphicsInstance, void* t_SurfData) = 0;

		/** Any work that this window needs to do for swap chain recreation */
		virtual void RecreateSwapChain() = 0;

		/** The current width of this window */
		virtual UINT32 GetWidth() const = 0;

		/** The current height of this window */
		virtual UINT32 GetHeight() const = 0;

		/** The current aspect ratio of this windows*/
		virtual float GetAspectRatio() const = 0;

		/** Int representing if this window should close or not */
		virtual int ShouldClose() = 0;

		/** Set whether the window hides the mouse cursor **/
		virtual void SetMouseVisible(bool t_IsVisible) = 0;

		/** Gets current visibility of mouse cursor **/
		virtual bool GetMouseVisible() = 0;

		/** 
		* @brief		Set this window's icon.
		* @param t_ID	the GUID of the window icon
		*/
		virtual void SetWindowIcon(Guid t_ID) = 0;
		
		/**
		* Check if this window is currently minimized
		* @return	True if the window is currently minimized
		*/
		virtual bool IsMinimized() const = 0;

		/** Changes window mode between fullscreen, window, and borderless window **/
		virtual void SetWindowMode(WindowMode t_WindowMode) = 0;

		/** Gets current window mode**/
		virtual WindowMode GetWindowMode() = 0;

	protected:
		/** Tracks mouse visibility in window **/
		bool m_IsMouseVisible = true;

		/** Tracks current window mode **/
		WindowMode m_WindowMode = WindowMode::Windowed;

		/** Saves window size information when switching between fullscreen and windowed
		modes, so users return to their previous size **/
	};
}   // namespace Fling