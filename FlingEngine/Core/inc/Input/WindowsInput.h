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

	};
} // namespace Fling

#endif	// FLING_WINDOWS