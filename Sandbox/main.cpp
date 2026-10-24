#include "FlingEngine.h"
#include "SandboxGame.h"
#include <iostream>

#if WITH_EDITOR

#include "SandboxEditor.h"

#endif
/**
* Entry point for using the Fling Engine!
*/
int main(int argc, const char* argv[])
{
	Fling::Engine Engine = {};

	try
	{
		// Run with editor and IMGUI
#if WITH_EDITOR
		return Engine.Run<Sandbox::Game, Sandbox::SandboxEditor>(argc, argv);
#else
		// Just run with the game and No editor features.
		return Engine.Run<Sandbox::Game>(argc, argv);
#endif
	}
	catch (const std::exception& e)
	{
		// A fatal error (F_LOG_FATAL) stopped the engine
		std::cerr << e.what() << std::endl;
		return Fling::ToInt(Fling::ExitCode::Crash);
	}
}
