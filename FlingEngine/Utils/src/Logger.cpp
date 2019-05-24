#include "pch.h"
#include "Logger.h"

namespace Fling
{
	// Static definitions
	std::shared_ptr<spdlog::logger> Logger::m_console = nullptr;

	void Logger::Init()
	{
		// Create the SPD log console 
		m_console = spdlog::stdout_color_mt( "LOG" );
		spdlog::set_pattern( "[%H:%M:%S] [%^%L%$] [thread %t] %v" );
		F_LOG_TRACE( "Logger initalized!" );
	}

	std::shared_ptr<spdlog::logger> Logger::GetCurrentConsole()
	{
		return m_console;
	}

}	// namespace Fling