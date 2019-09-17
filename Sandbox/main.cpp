
//#include "pch.h"
//#include "Engine.h"
#include "FlingEngine.h"

/**
* Entry point for using the Fling Engine!
*/
int main(int argc, char* argv[])
{
#if defined(_DEBUG)
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);	// report at any function exit
	_CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_DEBUG);	// send this info the the output log
#endif

	/*Fling::Engine Engine = { argc, argv };

	try
	{
		Engine.Run();
	}
	catch (const std::exception& e)
	{
		std::cerr << e.what() << std::endl;
		return EXIT_FAILURE;
	}*/

	//dump that info
#if defined(_DEBUG)
	_CrtDumpMemoryLeaks();
#endif

	return EXIT_SUCCESS;
}