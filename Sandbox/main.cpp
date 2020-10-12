#include "FlingEngine.h"
#include "SandboxGame.h"

#if WITH_EDITOR

#include "SandboxEditor.h"

#endif
/**
* Entry point for using the Fling Engine!
*/
int main(int argc, char* argv[])
{
	Fling::Engine Engine = {};
    
	try
	{
		// Run with editor and IMGUI
#if WITH_EDITOR
		Engine.Run<Sandbox::Game, Sandbox::SandboxEditor>(argc, argv);
#else
		// Just run with the game and No editor features.
		Engine.Run<Sandbox::Game>(argc, argv);
#endif
	}
	catch (const std::exception& e)
	{
		std::cerr << e.what() << std::endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}