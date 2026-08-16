project( "FlingTests" )

################### include folders ###########
set( ENGINE_DIR ../FlingEngine/ )
FLING_ENGINE_INC( ${ENGINE_DIR} )

include_directories(
  src
)

##################### Linking #################

# Prepare "Catch" library for other executables
find_package(Catch2 3 REQUIRED)

#set( CATCH_INCLUDE_DIR   ${CMAKE_CURRENT_SOURCE_DIR}/../external/Catch2/single_include )
#add_library ( Catch INTERFACE )
#target_include_directories( Catch INTERFACE ${CATCH_INCLUDE_DIR} )

set ( LINK_LIBS
    glfw ${GLFW_LIBRARIES}
	Catch2::Catch2WithMain
	"FlingEngine"
)

# link pthread if we need to
if ( NOT WIN32 )
    set( LINK_LIBS ${LINK_LIBS} pthread )
    message( STATUS "Added pthread!" )
endif()

################# Complier Options ############
if( MSVC )
    set ( MY_COMPILER_FLAGS "/W3" )
else()
    set ( MY_COMPILER_FLAGS "-Wall -Wno-reorder -Wno-unknown-pragmas -Wno-multichar" )
endif()

set ( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${MY_COMPILER_FLAGS}" )


### Setup visual studio source groups / filters ###
file( GLOB_RECURSE _source_list
    *.cpp* src/*.h* src/*.hpp* *.h* *.inl
)

if( MSVC )
    foreach( _source IN ITEMS ${_source_list} )
    	get_filename_component( _source_path "${_source}" PATH )
        string( REPLACE "${CMAKE_SOURCE_DIR}" "" _group_path "${_source_path}" )
        string( REPLACE "/" "\\" _group_path "${_group_path}" )
        source_group( "${_group_path}" FILES "${_source}" )
    endforeach()
endif()

################# Add Exe and link ######################

# Link our normal libraries and create an executeable 
add_executable( ${PROJECT_NAME} ${_source_list} )

# Link Catch2 and everything else
target_link_libraries( ${PROJECT_NAME} LINK_PUBLIC ${LINK_LIBS} )

# Shipping builds resolve EngineConfigDir() to a relative "Config" path, and
# ResourceManager::Init() chdirs to the executable directory. Keep TestConf.ini
# next to the binary so FlingTests pass regardless of launch CWD.
add_custom_command( TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${FLING_ROOT_DIR}/Config"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>/Config"
    COMMENT "Copying Config next to ${PROJECT_NAME}"
)