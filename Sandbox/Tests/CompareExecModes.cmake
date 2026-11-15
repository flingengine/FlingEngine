# Runs the Sandbox headless in --exec direct, then once for each variant (other exec
# modes, frame caps, frames per tick, thread jitter, or the same flags again), each with
# --dump-ticks, and fails unless every tick dump is byte-identical to the direct one:
# the per-tick input cut and state hash do not depend on threads, frame rates or the
# process (Docs/threading-model.md section 9).
#
# -DSANDBOX=<exe> -DROOT=<repo root> -DOUT=<folder>
# -DTICKS=<ticks per run> -DSIM_HZ=<ticks a second, for every run>
# -DVARIANTS=<runs separated by |; each is the extra flags of one
# run with spaces between them, for example "--exec inline|--exec threaded --max-fps 30">
# Optional: -DINPUT=<input file to replay> and -DLEVEL=<level to load>, relative to ROOT and
# Assets; with neither, the runs load the default start level. A variant with
# --frames-per-tick N runs TICKS * N frames (--frames counts frames in every exec mode).
# Optional: -DEXPECT_SHA256=<hash>: the SHA-256 of the direct dump must be this value, so a
# build configuration or compiler that simulates differently fails.

foreach( _var SANDBOX ROOT OUT TICKS SIM_HZ VARIANTS )
	if( NOT DEFINED ${_var} )
		message( FATAL_ERROR "CompareExecModes.cmake needs -D${_var}=..." )
	endif()
endforeach()
set( _scene_args "" )
if( INPUT )
	list( APPEND _scene_args --replay-input "${INPUT}" )
endif()
if( LEVEL )
	list( APPEND _scene_args --level "${LEVEL}" )
endif()

# A dead input path (every cut empty, the hash constant) would also give equal dumps, so
# the reference must show input (when it replays a file) and a state that changes
function( require_live_dump _text _file )
	if( INPUT AND NOT _text MATCHES "samples=[1-9]" )
		message( FATAL_ERROR "${_file}: no tick took a mouse sample; the replay did not reach the simulation" )
	endif()
	string( REGEX MATCHALL "hash=[0-9a-f]+" _hashes "${_text}" )
	list( REMOVE_DUPLICATES _hashes )
	list( LENGTH _hashes _distinct )
	if( _distinct LESS 2 )
		message( FATAL_ERROR "${_file}: the state hash never changes" )
	endif()
endfunction()

file( MAKE_DIRECTORY "${OUT}" )
string( REPLACE "|" ";" VARIANTS "${VARIANTS}" )

function( run_variant _name _flags )
	separate_arguments( _args UNIX_COMMAND "${_flags}" )
	set( _frames ${TICKS} )
	if( _flags MATCHES "--frames-per-tick ([0-9]+)" )
		math( EXPR _frames "${TICKS} * ${CMAKE_MATCH_1}" )
	endif()
	set( _dump "${OUT}/ticks-${_name}.txt" )
	file( REMOVE "${_dump}" )
	execute_process(
		COMMAND "${SANDBOX}" --null-render --unattended --seed 1 --frames ${_frames} --sim-hz ${SIM_HZ}
			${_scene_args} --dump-ticks "${_dump}" ${_args}
		WORKING_DIRECTORY "${ROOT}"
		RESULT_VARIABLE _code
		OUTPUT_VARIABLE _output
		ERROR_VARIABLE _output
	)
	if( NOT _code EQUAL 0 )
		message( FATAL_ERROR "Sandbox with '${_flags}' exited with ${_code}:\n${_output}" )
	endif()
	file( STRINGS "${_dump}" _lines )
	list( LENGTH _lines _count )
	if( NOT _count EQUAL ${TICKS} )
		message( FATAL_ERROR "${_dump} ('${_flags}') has ${_count} lines, expected ${TICKS}" )
	endif()
	if( _flags MATCHES "threaded" AND NOT _output MATCHES "Run .threaded.: [0-9]+ frames on the main thread, ${TICKS} ticks" )
		message( FATAL_ERROR "Sandbox with '${_flags}' did not run ${TICKS} ticks on the game thread:\n${_output}" )
	endif()
endfunction()

run_variant( direct "--exec direct" )
file( READ "${OUT}/ticks-direct.txt" _reference )
require_live_dump( "${_reference}" "${OUT}/ticks-direct.txt" )
file( SHA256 "${OUT}/ticks-direct.txt" _sha256 )
message( STATUS "Tick dump SHA-256: ${_sha256}" )
if( DEFINED EXPECT_SHA256 AND NOT _sha256 STREQUAL EXPECT_SHA256 )
	string( REGEX MATCH "[^\n]+\n$" _last "${_reference}" )
	message( FATAL_ERROR "The tick dump (${OUT}/ticks-direct.txt) has SHA-256 ${_sha256}, pinned ${EXPECT_SHA256}: "
		"this build simulates differently. Its last tick: ${_last}" )
endif()

set( _index 0 )
foreach( _flags IN LISTS VARIANTS )
	math( EXPR _index "${_index} + 1" )
	run_variant( "v${_index}" "${_flags}" )
	file( READ "${OUT}/ticks-v${_index}.txt" _dump )
	if( NOT _dump STREQUAL _reference )
		message( FATAL_ERROR "The tick dump of '${_flags}' (${OUT}/ticks-v${_index}.txt) differs from --exec direct (${OUT}/ticks-direct.txt)" )
	endif()
	message( STATUS "Same as direct: ${_flags}" )
endforeach()
message( STATUS "Tick dumps are the same in every exec mode (${TICKS} ticks at ${SIM_HZ} Hz)" )
