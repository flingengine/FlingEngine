#pragma once

#include "pch.h"
#include "FlingTypes.h"
#include "NonCopyable.hpp"

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

		/** Int representing if this window should close or not */
		virtual int ShouldClose() = 0;
		
		/**
		* Check if this window is currently minimized
		* @return	True if the window is currently minimized
		*/
		virtual bool IsMinimized() const = 0;
	};
}   // namespace Fling