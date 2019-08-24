#pragma once

#include "Input.h"

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

		virtual bool IsKeyDownImpl(int t_Keycode) override;
		virtual bool IsMouseButtonPressedImpl(int t_Button) override;
	};
} // namespace Fling

#endif	// FLING_WINDOWS