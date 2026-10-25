# Run CrashProbe in one mode, and pass only if it exits with code 2
# (ExitCode::Crash) and prints a stack trace.
#
#   cmake -DPROBE=<path to CrashProbe> -DMODE=<abort|assert|segv|throw> -P ExpectCrash.cmake

execute_process(
	COMMAND "${PROBE}" "${MODE}"
	RESULT_VARIABLE _result
	OUTPUT_VARIABLE _out
	ERROR_VARIABLE _err
	TIMEOUT 60
)

message("${_out}${_err}")

if(NOT _result STREQUAL "2")
	message(FATAL_ERROR "CrashProbe ${MODE}: expected exit code 2, got '${_result}'")
endif()

if(NOT _err MATCHES "Stack trace:")
	message(FATAL_ERROR "CrashProbe ${MODE}: no stack trace on stderr")
endif()
