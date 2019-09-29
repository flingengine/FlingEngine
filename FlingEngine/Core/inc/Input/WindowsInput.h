#pragma once

#include "Input/Input.h"

#if defined FLING_WINDOWS

namespace Fling
{
	/**
	* Windows specific input implementation
	*/
	class WindowsInput : public Input
	{
	protected:

		virtual void InitImpl() override;
		virtual void ShutdownImpl() override;

		virtual void InitKeyMap() override;
		
		virtual void PollImpl() override;

		virtual bool IsKeyDownImpl(const std::string& t_KeyName) override;
		virtual bool IsKeyHelpImpl(const std::string& t_KeyName) override;
		virtual bool IsMouseButtonPressedImpl(const std::string& t_KeyName) override;
		virtual bool IsMouseDownImpl(const std::string& t_KeyName) override;

		/**
		 * @brief Get the mouse position in normalized coordinates (0,1)
		 * 
		 * @return MousePos 
		 */
		virtual MousePos GetMousePosImpl() override;
	};
} // namespace Fling

#endif	// FLING_WINDOWS