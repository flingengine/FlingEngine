#pragma once

#include "BuildConfig.h"
#include "LaunchOptions.h"
#include "Platform.h"
#include "Singleton.hpp"

#define SPDLOG_TRACE_ON
#define SPDLOG_DEBUG_ON

#include "spdlog/async.h"
#include "spdlog/fmt/fmt.h"
#include "spdlog/fmt/ostr.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/sinks/stdout_sinks.h"
#include "spdlog/spdlog.h"

#include <memory>

namespace Fling
{
    /**
     * Singleton class that allows logging to the console as well as async to a file.
     * 			Use the defines to actually log strings out.
     */
    class Logger : public Singleton<Logger>
    {
    public:
        virtual void Init() override;

        /**
         * Write all queued log lines and stop the log thread. Call this last in
         * shutdown. Init can start the logger again.
         */
        virtual void Shutdown() override;

        /**
         * Set the format of the console and file logs. Call this before Init. Json
         * writes one JSON object per line, to fling_log.jsonl instead of fling_log.txt.
         */
        static void SetFormat(LogFormat t_Format) { m_Format = t_Format; }

        /**
         * Gets a reference to the current logging console
         * @return Shader ptr to the current console
         */
        static std::shared_ptr<spdlog::logger> GetCurrentConsole();

        /**
         * Get the current async log file that is being written to
         */
        static std::shared_ptr<spdlog::logger> GetCurrentLogFile();

    protected:
        /** Pointer to the current console that is being used for logging */
        static std::shared_ptr<spdlog::logger> m_Console;

        /** Pointer to the log file logger */
        static std::shared_ptr<spdlog::logger> m_FileLog;

        static LogFormat m_Format;
    };

} // namespace Fling

// Each macro is one statement, so it is safe in an if with no braces.
#if FLING_DEBUG || defined(F_ENABLE_LOGGING)

#define F_LOG_TRACE(...)                                                                                               \
    do                                                                                                                 \
    {                                                                                                                  \
        Fling::Logger::GetCurrentConsole()->info(__VA_ARGS__);                                                         \
        Fling::Logger::GetCurrentLogFile()->info(__VA_ARGS__);                                                         \
    } while (false)
#define F_LOG_WARN(...)                                                                                                \
    do                                                                                                                 \
    {                                                                                                                  \
        Fling::Logger::GetCurrentConsole()->warn(__VA_ARGS__);                                                         \
        Fling::Logger::GetCurrentLogFile()->warn(__VA_ARGS__);                                                         \
    } while (false)
#define F_LOG_ERROR(...)                                                                                               \
    do                                                                                                                 \
    {                                                                                                                  \
        Fling::Logger::GetCurrentConsole()->error(__VA_ARGS__);                                                        \
        Fling::Logger::GetCurrentLogFile()->error(__VA_ARGS__);                                                        \
    } while (false)

/** Log a message to the error console AND throw a runtime exception. Only use for fatal asserts!
    Left in in release builds! */
#define F_LOG_FATAL(...)                                                                                               \
    do                                                                                                                 \
    {                                                                                                                  \
        Fling::Logger::GetCurrentConsole()->error(__VA_ARGS__);                                                        \
        throw std::runtime_error(__VA_ARGS__);                                                                         \
    } while (false)

#else

#define F_LOG_TRACE(...)                                                                                               \
    do                                                                                                                 \
    {                                                                                                                  \
    } while (false)
#define F_LOG_WARN(...)                                                                                                \
    do                                                                                                                 \
    {                                                                                                                  \
    } while (false)
#define F_LOG_ERROR(...)                                                                                               \
    do                                                                                                                 \
    {                                                                                                                  \
    } while (false)

/** Log a message to the error console AND throw a runtime exception. Only use for fatal asserts!
    Left in in release builds! */
#define F_LOG_FATAL(...)                                                                                               \
    do                                                                                                                 \
    {                                                                                                                  \
        Fling::Logger::GetCurrentConsole()->error(__VA_ARGS__);                                                        \
        throw std::runtime_error(__VA_ARGS__);                                                                         \
    } while (false)
#endif
