# The Foundation target contains the core building blocks that Fling can use to make the Engine.
# this shouldn't depend on any other Fling Engine targets to avoid circular build dependcies

cmake_minimum_required (VERSION 3.11.0)

project("Foundation" VERSION 0.1)

message(status "-- The foundation library is being configured!! -- ")

include_directories(
	inc
)

# Find any C++ files here
file( GLOB_RECURSE foundation_source_list
    *.cpp* src/*.h* src/*.hpp* *.h* *.inl
)

# Make VS look ok
if( MSVC )
    foreach( _source IN ITEMS ${foundation_source_list} )
    	get_filename_component( _source_path "${_source}" PATH )
        string( REPLACE "${CMAKE_SOURCE_DIR}" "" _group_path "${_source_path}" )
        string( REPLACE "/" "\\" _group_path "${_group_path}" )
        source_group( "${_group_path}" FILES "${_source}" )
    endforeach()
endif()

# the "Shared" part here means that the library target is a DLL
add_library ( ${PROJECT_NAME} SHARED ${foundation_source_list} )

# I think fix the include paths?
target_include_directories(${PROJECT_NAME} PUBLIC inc)
target_include_directories(${PROJECT_NAME} PRIVATE src)

# set the version of this library
set_target_properties(${PROJECT_NAME} PROPERTIES VERSION ${PROJECT_VERSION})

#target_include_directories (${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${SPIRV_CROSS_INCLUDE_DIR})

# If we need to link against anything, do so here
# target_link_libraries( ${PROJECT_NAME} LINK_PUBLIC ${LINK_LIBS} )