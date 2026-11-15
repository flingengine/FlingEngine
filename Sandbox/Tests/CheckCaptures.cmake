# Runs the Sandbox headless twice, in --exec direct and --exec threaded, each writing captures
# at two ticks, then fails unless every file is there and the two runs wrote the same bytes:
# a capture shows the simulation, which does not depend on threads (Docs/threading-model.md
# section 9).
#
#   KIND=state       --null-render with --dump-state (JSON). Needs no GPU.
#   KIND=screenshot  --offscreen with --screenshot (PNG). Needs a Vulkan driver (a software one,
#                    such as llvmpipe, is enough) but no display.
#
# -DSANDBOX=<exe> -DROOT=<repo root> -DOUT=<folder> -DKIND=state|screenshot

foreach( _var SANDBOX ROOT OUT KIND )
	if( NOT DEFINED ${_var} )
		message( FATAL_ERROR "CheckCaptures.cmake needs -D${_var}=..." )
	endif()
endforeach()

set( _ticks 10 30 )
if( KIND STREQUAL "state" )
	set( _render --null-render )
	set( _flags --dump-state "{tick}.json" --dump-state-ticks 10,30 )
	set( _ext json )
elseif( KIND STREQUAL "screenshot" )
	set( _render --offscreen --resolution 320x180 )
	set( _flags --screenshot "{tick}.png" --screenshot-ticks 10,30 )
	set( _ext png )
else()
	message( FATAL_ERROR "KIND must be state or screenshot, not ${KIND}" )
endif()

file( REMOVE_RECURSE "${OUT}" )
foreach( _exec direct threaded )
	set( _dir "${OUT}/${_exec}" )
	file( MAKE_DIRECTORY "${_dir}" )
	# The file names are relative to the project, so give the folder as an absolute path
	string( REPLACE "{tick}" "${_dir}/{tick}" _run_flags "${_flags}" )
	execute_process(
		COMMAND "${SANDBOX}" ${_render} --unattended --frames 30 --seed 1 --exec ${_exec} ${_run_flags}
		WORKING_DIRECTORY "${ROOT}"
		RESULT_VARIABLE _code
		OUTPUT_VARIABLE _out
		ERROR_VARIABLE _out
	)
	if( NOT _code EQUAL 0 )
		message( FATAL_ERROR "--exec ${_exec} exited with ${_code}:\n${_out}" )
	endif()
	foreach( _tick IN LISTS _ticks )
		if( NOT EXISTS "${_dir}/${_tick}.${_ext}" )
			message( FATAL_ERROR "--exec ${_exec} wrote no ${_dir}/${_tick}.${_ext}:\n${_out}" )
		endif()
	endforeach()
endforeach()

foreach( _tick IN LISTS _ticks )
	file( SHA256 "${OUT}/direct/${_tick}.${_ext}" _direct )
	file( SHA256 "${OUT}/threaded/${_tick}.${_ext}" _threaded )
	if( NOT _direct STREQUAL _threaded )
		message( FATAL_ERROR "The ${KIND} of tick ${_tick} differs between --exec direct and threaded" )
	endif()
endforeach()

if( KIND STREQUAL "state" )
	file( READ "${OUT}/direct/30.json" _doc )
	if( NOT _doc MATCHES "\"tick\": 30" OR NOT _doc MATCHES "\"Transform\"" )
		message( FATAL_ERROR "The state of tick 30 has no tick or no Transform:\n${_doc}" )
	endif()
else()
	# A PNG file starts with these 8 bytes
	file( READ "${OUT}/direct/30.png" _sig LIMIT 8 HEX )
	if( NOT _sig STREQUAL "89504e470d0a1a0a" )
		message( FATAL_ERROR "30.png is not a PNG file (it starts with ${_sig})" )
	endif()
endif()

message( STATUS "Captures are the same in --exec direct and threaded" )
