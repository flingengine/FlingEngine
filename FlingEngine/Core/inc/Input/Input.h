#pragma once

#include "NonCopyable.hpp"
#include "Input/Key.h"

namespace Fling
{
	/**
	* Base input class for polling input in the Fling Engine
	*/
	class Input : public NonCopyable
	{
	public:

		/** Initalize the input instance on this platform. Handle and input mapping on this platform */
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

		/**
		 * @brief Update any input polling that needs to happen on this platform.
		 */
		static void Poll() { m_Instace->PollImpl();  }
		
		static bool IsKeyDown(const std::string& t_KeyName) { return m_Instace->IsKeyDownImpl(t_KeyName); }
		static bool IsKeyHeld(const std::string& t_KeyName) { return m_Instace->IsKeyHelpImpl(t_KeyName); }
		static bool IsMouseButtonPressed(const std::string& t_KeyName) { return m_Instace->IsMouseButtonPressedImpl(t_KeyName); }

		typedef std::map<std::string, Fling::Key> KeyMap;
		typedef std::pair<std::string, Fling::Key> KeyPair;

	protected:

		/** The actual key map */
		static KeyMap m_KeyMap;

		/** Created by the implementation class @see WindowInput */
		static Input* m_Instace;
		
		/**
		 * @brief Add a key mapping to this platform. 
		 * 
		 * @param t_Name 		The name of this key 
		 * @param t_KeyCode 	The KeyCode that maps this key to the current platform
		 */
		static void AddKeyMap(const std::string& t_Name, UINT32 t_KeyCode)
		{
			m_KeyMap.insert( KeyPair(t_Name, Fling::Key(t_Name, t_KeyCode)) );
		}

		virtual void InitImpl() = 0;
		virtual void ShutdownImpl() = 0;

		/** Poll for input from the keyboard */
		virtual void PollImpl() = 0;

		virtual void InitKeyMap() = 0;

		virtual bool IsKeyDownImpl(const std::string& t_KeyName) = 0;
		virtual bool IsKeyHelpImpl(const std::string& t_KeyName) = 0;
		virtual bool IsMouseButtonPressedImpl(const std::string& t_KeyName) = 0;
	};

}	// namespace Fling