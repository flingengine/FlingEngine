#pragma once

#include "NonCopyable.hpp"

namespace Fling
{
	/**
	* Base input class for polling input in the Fling Engine
	*/
	class Input : public NonCopyable
	{
	public:

		static void Init() { m_Instace->InitImpl(); }
		
		/**
		* Shuts down input manager.
		* Deletes the input implementation pointer.
		*/
		static void Shutdown() 
		{ 
			m_Instace->ShutdownImpl();
			delete m_Instace; 
		}

		static bool IsKeyDown(int t_Keycode) { return m_Instace->IsKeyDownImpl(t_Keycode); }
		static bool IsKeyHeld(int t_Keycode) { return m_Instace->IsKeyDownImpl(t_Keycode); }
		static bool IsMouseButtonPressed(int t_Button) { return m_Instace->IsMouseButtonPressedImpl(t_Button); }

	protected:
	
		virtual void InitImpl() = 0;
		virtual void ShutdownImpl() = 0;

		virtual bool IsKeyDownImpl(int t_Keycode) = 0;
		virtual bool IsKeyHeldImpl(int t_Keycode) = 0;
		virtual bool IsMouseButtonPressedImpl(int t_Button) = 0;

		/** Created by the implementation class @see WindowInput */
		static Input* m_Instace;
	};

}	// namespace Fling