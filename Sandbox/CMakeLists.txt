project( "Sandbox" )


##################### Linking #################


set ( LINK_LIBS
	"FlingEngine"
)

message( STATUS "LINK_LIBS is : " ${LINK_LIBS} )

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