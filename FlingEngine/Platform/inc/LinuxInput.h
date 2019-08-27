#pragma once

#include "Input/Input.h"

#ifdef FLING_LINUX

#include "LinuxKeycodes.h"

namespace Fling
{
	class LinuxInput : public Input
	{
	protected:

		virtual void InitImpl() override;
		virtual void ShutdownImpl() override;

		virtual bool IsKeyDownImpl(int t_Keycode) override;
		virtual bool IsKeyHeldImpl(int t_Keycode) override;
		virtual bool IsMouseButtonPressedImpl(int t_Button) override;

	};

} // namespace Fling

#endif	// FLING_LINUX