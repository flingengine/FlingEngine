## Macro to include all necessary engine header files
## And set some project settings that are the same on everything that uses Fling
MACRO(FLING_ENGINE_INC EngineDir )

	# Take care of warnings about strcpy
	if( MSVC )
		add_definitions( -D_CRT_SECURE_NO_WARNINGS )
	# GCC
	elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")

		add_definitions( -Wno-class-memaccess )
		#add_definitions( -Wno-unused-private-field )
		#add_definitions( -Wno-exceptions )

	# Clang
	elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")

	endif()

	# Set Build Dir
	set( CMAKE_BINARY_DIR    "/build" )
	set_property( GLOBAL PROPERTY USE_FOLDERS ON )
	set( CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/_Output/$<CONFIG> )
	set( CMAKE_CXX_STANDARD 17 )
	set( CMAKE_INSTALL_MESSAGE LAZY )   # Skips update to date messages

	# Sets the output directory
	set( CMAKE_RUNTIME_OUTPUT_DIRECTORY "bin" )

	include_directories(
		${EngineDir}Core/inc
		${EngineDir}Graphics/inc
		${EngineDir}Resources/inc
		${EngineDir}Utils/inc
		${EngineDir}Platform/inc
		${EngineDir}Gameplay/inc
		${GENERATED_INC_FOLDER}		# Generated include files that cmake will handle (i.e. GitVersion)
	)

ENDMACRO(FLING_ENGINE_INC)

# Example usage 

#FLING_ENGINE_INC( "../FlingEngine/" )
#ADD_EXECUTABLE(MyApp ${MY_SRCS})