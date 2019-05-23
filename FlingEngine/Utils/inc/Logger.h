#pragma once

#include "Singleton.hpp"

#define SPDLOG_TRACE_ON
#define SPDLOG_DEBUG_ON

#include "spdlog/spdlog.h"
#include "spdlog/sinks/stdout_sinks.h"
#include "spdlog/sinks/stdout_color_sinks.h"

class Logger : public Singleton<Logger>
{
public:

	Logger();
	virtual ~Logger();

	virtual void Init() override;

	virtual void Shutdown() override;

	/// <summary>
	/// Gets a reference to the current logging console
	/// </summary>
	/// <returns>Shader ptr to the current console</returns>
	static std::shared_ptr<spdlog::logger> GetCurrentConsole();

protected:

	/** Pointer to the current console that is being used for logging */
	static std::shared_ptr<spdlog::logger> console;

};


#if defined( DEBUG ) || defined ( _DEBUG ) || defined ( F_ENABLE_LOGGING )

#define  F_LOG_TRACE( ... )    Logger::GetCurrentConsole()->info( __VA_ARGS__ )
#define  F_LOG_WARN( ... )     Logger::GetCurrentConsole()->warn( __VA_ARGS__ )
#define  F_LOG_ERROR( ... )    Logger::GetCurrentConsole()->error( __VA_ARGS__ )

#else

#define  F_LOG_TRACE( ... ) 
#define  F_LOG_WARN( ... )  
#define  F_LOG_ERROR( ... ) 

#endif