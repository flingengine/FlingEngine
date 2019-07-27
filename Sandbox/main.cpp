#include "pch.h"

#include "Engine.h"

/**
* Entry point for using the Fling Engine! 
*/

int main( int argc, char* argv[] )
{
	Fling::Engine engine = { argc, argv };

	try
	{
		engine.Run();
	}
	catch( const std::exception& e )
	{
		std::cerr << e.what() << std::endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}