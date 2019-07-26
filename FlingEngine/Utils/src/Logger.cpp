#include "pch.h"
#include "Logger.h"

namespace Fling
{
	// Static definitions
	std::shared_ptr<spdlog::logger> Logger::m_Console = nullptr;
	std::shared_ptr<spdlog::logger> Logger::m_FileLog = nullptr;

	void Logger::Init()
	{
		// Create the SPD log console 
		m_Console = spdlog::stdout_color_mt( "LOG" );
		m_FileLog = spdlog::create_async<spdlog::sinks::basic_file_sink_mt>("async_file_logger", "fling_log.txt");
		spdlog::set_pattern( "[%H:%M:%S] [%^%L%$] [thread %t] %v" );
		
		F_LOG_TRACE( "Logger initalized!" );
	}

	std::shared_ptr<spdlog::logger> Logger::GetCurrentConsole()
	{
		return m_Console;
	}

	std::shared_ptr<spdlog::logger> Logger::GetCurrentLogFile()
	{
		return m_FileLog;
	}

}	// namespace Fling