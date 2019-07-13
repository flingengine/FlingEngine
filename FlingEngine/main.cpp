#include "pch.h"

#include "Engine.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <iostream>

int main( int argc, char* argv[] )
{
	// #TODO: Make an ini file for the resolution and other settings
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
