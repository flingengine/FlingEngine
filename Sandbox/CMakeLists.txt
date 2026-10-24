project( "Sandbox" )

### Source files (VS filters are set up by fling_configure_target) ###
file( GLOB_RECURSE _source_list
    *.cpp* src/*.h* src/*.hpp* *.h* *.inl
)

# Editor code is only built (and the Editor module only linked) when the editor is on
if( NOT WITH_EDITOR_FLAG )
    list( FILTER _source_list EXCLUDE REGEX "^${CMAKE_CURRENT_SOURCE_DIR}/Editor/" )
endif()

################# Add Exe and link ######################

add_executable( ${PROJECT_NAME} ${_source_list} )
fling_configure_target( ${PROJECT_NAME} )
fling_set_bin_output( ${PROJECT_NAME} )

target_include_directories( ${PROJECT_NAME} PRIVATE Gameplay/inc )

if( WITH_EDITOR_FLAG )
    target_include_directories( ${PROJECT_NAME} PRIVATE Editor/inc )
    target_link_libraries( ${PROJECT_NAME} PRIVATE Fling::EditorStack )
else()
    target_link_libraries( ${PROJECT_NAME} PRIVATE Fling::GameStack )
endif()
