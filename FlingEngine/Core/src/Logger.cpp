#include "pch.h"
#include "Logger.h"

namespace Fling
{
	// Static definitions
	std::shared_ptr<spdlog::logger> Logger::m_Console = nullptr;
	std::shared_ptr<spdlog::logger> Logger::m_FileLog = nullptr;

	void Logger::Init()
	{
		// Create the logs directory if needed
		if(!FlingPaths::DirExists( FlingPaths::EngineLogDir().c_str() ))
		{
			FlingPaths::MakeDir( FlingPaths::EngineLogDir().c_str() );
		}

		// Create the SPD log console 
		if(!m_Console)
		{
			m_Console = spdlog::stdout_color_mt( "LOG" );
		}
		
		// Create an async file logger for errors/warnings
		if(!m_FileLog)
		{
			m_FileLog = spdlog::create_async<spdlog::sinks::basic_file_sink_mt>(
				"async_file_logger", 
				FlingPaths::EngineLogDir() + "/fling_log.txt"
			);
			spdlog::set_pattern( "[%H:%M:%S] [%^%L%$] [thread %t] %v" );
		}
		
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