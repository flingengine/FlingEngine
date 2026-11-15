# Runs the Sandbox headless with the same input file and two values of
# --frames-per-tick, in each exec mode (direct, inline, threaded), each with
# --dump-ticks, and fails unless every tick dump is the same: the per-tick input
# cut and the state hash do not depend on the frame rate or the thread.
# Each run sets its exec mode and its simulation rate explicitly.
#
# -DSANDBOX=<exe> -DROOT=<repo root> -DINPUT=<file, relative to ROOT> -DOUT=<folder>
# -DTICKS=<ticks per run> -DFPT_A=<n> -DFPT_B=<m> -DSIM_HZ=<ticks a second>

foreach( _var SANDBOX ROOT INPUT OUT TICKS FPT_A FPT_B SIM_HZ )
	if( NOT DEFINED ${_var} )
		message( FATAL_ERROR "CompareTickDumps.cmake needs -D${_var}=..." )
	endif()
endforeach()

# A dead input path (every cut empty, the hash constant) would also give equal dumps, so
# the reference must show input and a state that changes
function( require_live_dump _text _file )
	if( NOT _text MATCHES "samples=[1-9]" )
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
set( _reference "" )
foreach( _exec direct inline threaded )
	foreach( _fpt ${FPT_A} ${FPT_B} )
		math( EXPR _frames "${TICKS} * ${_fpt}" )
		set( _dump "${OUT}/ticks-${_exec}-fpt${_fpt}.txt" )
		file( REMOVE "${_dump}" )
		execute_process(
			COMMAND "${SANDBOX}" --null-render --unattended --seed 1 --frames ${_frames} --frames-per-tick ${_fpt}
				--exec ${_exec} --sim-hz ${SIM_HZ} --replay-input "${INPUT}" --dump-ticks "${_dump}"
			WORKING_DIRECTORY "${ROOT}"
			RESULT_VARIABLE _code
			OUTPUT_VARIABLE _output
			ERROR_VARIABLE _output
		)
		if( NOT _code EQUAL 0 )
			message( FATAL_ERROR "Sandbox with --exec ${_exec} --frames-per-tick ${_fpt} exited with ${_code}:\n${_output}" )
		endif()
		file( STRINGS "${_dump}" _lines )
		list( LENGTH _lines _count )
		if( NOT _count EQUAL ${TICKS} )
			message( FATAL_ERROR "${_dump} has ${_count} lines, expected ${TICKS}" )
		endif()
		file( READ "${_dump}" _text )
		if( _reference STREQUAL "" )
			set( _reference "${_text}" )
			set( _reference_file "${_dump}" )
			require_live_dump( "${_text}" "${_dump}" )
		elseif( NOT _text STREQUAL _reference )
			message( FATAL_ERROR "The tick dumps differ: ${_reference_file} and ${_dump}" )
		endif()
	endforeach()
endforeach()
message( STATUS "Tick dumps are the same (${TICKS} ticks at ${SIM_HZ} Hz, direct, inline and threaded, --frames-per-tick ${FPT_A} and ${FPT_B})" )
