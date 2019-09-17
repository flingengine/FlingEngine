#include "FlingEngine.h"

/**
* Entry point for using the Fling Engine!
*/
int main(int argc, char* argv[])
{
	Fling::Engine Engine = {};

	try
	{
		Engine.Run();
	}
	catch (const std::exception& e)
	{
		std::cerr << e.what() << std::endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}