#pragma once

#include "Singleton.hpp"

#define SPDLOG_TRACE_ON
#define SPDLOG_DEBUG_ON

#include "spdlog/spdlog.h"
#include "spdlog/sinks/stdout_sinks.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/async.h"
#include "spdlog/sinks/stdout_color_sinks.h"

namespace Fling
{
	/**
	 * @brief 	Singleton class that allows logging to the console as well as async to a file.
	 * 			Use the defines to actually log strings out.  
	 */
	class Logger : public Singleton<Logger>
	{
	public:

		virtual void Init() override;

		/// <summary>
		/// Gets a reference to the current logging console
		/// </summary>
		/// <returns>Shader ptr to the current console</returns>
		static std::shared_ptr<spdlog::logger> GetCurrentConsole();

		/**
		* @brief	Get the current async log file that is being written to
		*/
		static std::shared_ptr<spdlog::logger> GetCurrentLogFile();


	protected:

		/** Pointer to the current console that is being used for logging */
		static std::shared_ptr<spdlog::logger> m_Console;

		/** Pointer to the log file logger */
		static std::shared_ptr<spdlog::logger> m_FileLog;

	};

}	// namespace Fling

// Debug/release mode defs
#if defined( DEBUG ) || defined ( _DEBUG ) || defined ( F_ENABLE_LOGGING )

#define  F_LOG_TRACE( ... )    Logger::GetCurrentConsole()->info( __VA_ARGS__ ); Logger::GetCurrentLogFile()->info( __VA_ARGS__ )
#define  F_LOG_WARN( ... )     Logger::GetCurrentConsole()->warn( __VA_ARGS__ ); Logger::GetCurrentLogFile()->warn( __VA_ARGS__ )
#define  F_LOG_ERROR( ... )    Logger::GetCurrentConsole()->error( __VA_ARGS__ ); Logger::GetCurrentLogFile()->error( __VA_ARGS__ )

/** Log a message to the error console AND throw a runtime exception. Only use for fatal asserts!
    Left in in release builds! */
#define  F_LOG_FATAL( ... )    Logger::GetCurrentConsole()->error( __VA_ARGS__ ); \
                               throw std::runtime_error( __VA_ARGS__ )

#else

#define  F_LOG_TRACE( ... ) 
#define  F_LOG_WARN( ... )  
#define  F_LOG_ERROR( ... ) 

/** Log a message to the error console AND throw a runtime exception. Only use for fatal asserts!
    Left in in release builds! */
#define  F_LOG_FATAL( ... )    Logger::GetCurrentConsole()->error( __VA_ARGS__ ); \
                               throw std::runtime_error( __VA_ARGS__ )
#endif