#include "pch.h"
#include "Engine.h"

Engine::Engine( const int t_width, const int t_height )
{
}

Engine::~Engine()
{

}

UINT64 Engine::Run()
{
	Startup();

	Tick();

	Shutdown();

	return 0;
}

void Engine::Startup()
{
	Utils::Random::Init();

	Logger::instance().Init();

	F_LOG_TRACE( "Lol here I am!" );
}

void Engine::Tick()
{
	// Update systems
}

void Engine::Shutdown()
{
	// Cleanup any resources
	Logger::instance().Shutdown();
}