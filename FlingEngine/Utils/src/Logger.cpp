#include "pch.h"
#include "Logger.h"

std::shared_ptr<spdlog::logger> Logger::console = nullptr;

Logger::Logger()
{
	std::cout << "Create logger!" << std::endl;
}

Logger::~Logger()
{
	std::cout << "Destroy logger!" << std::endl;
}

void Logger::Init()
{
	// Create the SPD log console 
	console = spdlog::stdout_color_mt( "LOG" );
	spdlog::set_pattern( "[%H:%M:%S] [%^%L%$] [thread %t] %v" );
}

void Logger::Shutdown()
{

}

std::shared_ptr<spdlog::logger> Logger::GetCurrentConsole()
{
	return console;
}
