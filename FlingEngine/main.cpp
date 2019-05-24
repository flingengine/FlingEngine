#include "pch.h"

#include "Engine.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <iostream>

int main( int argc, char* argv[] )
{
	Fling::Engine engine( 800, 600 );

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
