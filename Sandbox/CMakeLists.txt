project( "Sandbox" )


##################### Linking #################


set ( LINK_LIBS
	"FlingEngine"
)

message( STATUS "LINK_LIBS is : " ${LINK_LIBS} )

################# Complier Options #################
if( MSVC )
    set ( MY_COMPILER_FLAGS "/W3" )
else()
    set ( MY_COMPILER_FLAGS "-Wall -Wno-reorder -Wno-unknown-pragmas -Wno-multichar" )
endif()

set ( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${MY_COMPILER_FLAGS}" )

# Previously applied to every engine consumer by FLING_ENGINE_INC()
if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    add_compile_options( -Wno-class-memaccess )
endif()

### Source files (VS filters are set up by fling_configure_target) ###
file( GLOB_RECURSE _source_list
    *.cpp* src/*.h* src/*.hpp* *.h* *.inl
)

################# Add Exe and link ######################

# Link our normal libraries and create an executeable 
add_executable( ${PROJECT_NAME} ${_source_list} )
fling_configure_target( ${PROJECT_NAME} )
fling_set_bin_output( ${PROJECT_NAME} )

target_include_directories( ${PROJECT_NAME} PRIVATE Gameplay/inc )

# Editor
if( WITH_EDITOR_FLAG )
    target_include_directories( ${PROJECT_NAME} PRIVATE Editor/inc )
endif()

# Link libs
target_link_libraries( ${PROJECT_NAME} PRIVATE ${LINK_LIBS} )