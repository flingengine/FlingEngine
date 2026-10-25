#include "Logger.h"
#include "FlingPaths.h"
#include "spdlog/pattern_formatter.h"

#include <cstdio>

namespace Fling
{
	namespace
	{
		/** The %* pattern flag: the log message, escaped for a JSON string. */
		class JsonEscapedMessageFlag : public spdlog::custom_flag_formatter
		{
		public:
			void format(const spdlog::details::log_msg& t_Msg, const std::tm&, spdlog::memory_buf_t& t_Dest) override
			{
				for (const char c : t_Msg.payload)
				{
					switch (c)
					{
					case '"': Append(t_Dest, "\\\""); break;
					case '\\': Append(t_Dest, "\\\\"); break;
					case '\n': Append(t_Dest, "\\n"); break;
					case '\r': Append(t_Dest, "\\r"); break;
					case '\t': Append(t_Dest, "\\t"); break;
					default:
						if (static_cast<unsigned char>(c) < 0x20)
						{
							char Escaped[8] = {};
							std::snprintf(Escaped, sizeof(Escaped), "\\u%04x", static_cast<unsigned int>(c));
							Append(t_Dest, Escaped);
						}
						else
						{
							t_Dest.push_back(c);
						}
						break;
					}
				}
			}

			std::unique_ptr<custom_flag_formatter> clone() const override
			{
				return std::make_unique<JsonEscapedMessageFlag>();
			}

		private:
			static void Append(spdlog::memory_buf_t& t_Dest, const char* t_Str)
			{
				for (; *t_Str != '\0'; ++t_Str)
				{
					t_Dest.push_back(*t_Str);
				}
			}
		};
	}	// namespace

	// Static definitions
	std::shared_ptr<spdlog::logger> Logger::m_Console = nullptr;
	std::shared_ptr<spdlog::logger> Logger::m_FileLog = nullptr;
	LogFormat Logger::m_Format = LogFormat::Text;

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
			const char* FileName = m_Format == LogFormat::Json ? "/fling_log.jsonl" : "/fling_log.txt";
			m_FileLog = spdlog::create_async<spdlog::sinks::basic_file_sink_mt>(
				"async_file_logger",
				FlingPaths::EngineLogDir() + FileName
			);
		}

		if (m_Format == LogFormat::Json)
		{
			auto Formatter = std::make_unique<spdlog::pattern_formatter>();
			Formatter->add_flag<JsonEscapedMessageFlag>('*').set_pattern(
				R"({"time":"%Y-%m-%dT%H:%M:%S.%e","level":"%l","thread":%t,"msg":"%*"})"
			);
			spdlog::set_formatter( std::move(Formatter) );
		}
		else
		{
			spdlog::set_pattern( "[%H:%M:%S] [%^%L%$] [thread %t] %v" );
		}

		F_LOG_TRACE( "Logger initalized!" );
	}

	void Logger::Shutdown()
	{
		// spdlog::shutdown writes the queued lines of the async file log before it
		// stops the log thread
		spdlog::shutdown();
		m_Console = nullptr;
		m_FileLog = nullptr;
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
