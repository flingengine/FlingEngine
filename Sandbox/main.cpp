#include "FlingEngine.h"
#include "SandboxGame.h"

/**
* Entry point for using the Fling Engine!
*/
int main(int argc, char* argv[])
{
	Fling::Engine Engine = {};
    Fling::ShaderProgram Shaders("Shaders/vert.spv"_hs, "Shaders/frag.spv"_hs);
                           
	try
	{
		Engine.Run<Sandbox::Game>(&Shaders);
	}
	catch (const std::exception& e)
	{
		std::cerr << e.what() << std::endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}