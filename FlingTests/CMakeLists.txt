project( "FlingTests" )

##################### Linking #################

# Only reached when FLING_BUILD_TESTS is ON (see the root CMakeLists.txt)
find_package(Catch2 3 REQUIRED)
include(Catch)

set ( LINK_LIBS
    glfw ${GLFW_LIBRARIES}
	Catch2::Catch2WithMain
	"FlingEngine"
)

### Source files (VS filters are set up by fling_configure_target) ###
file( GLOB_RECURSE _source_list
    *.cpp* src/*.h* src/*.hpp* *.h* *.inl
)

################# Add Exe and link ######################

# Link our normal libraries and create an executeable 
add_executable( ${PROJECT_NAME} ${_source_list} )
fling_configure_target( ${PROJECT_NAME} )
fling_set_bin_output( ${PROJECT_NAME} )

target_include_directories( ${PROJECT_NAME} PRIVATE src )

# Link Catch2 and everything else
target_link_libraries( ${PROJECT_NAME} PRIVATE ${LINK_LIBS} )

# Shipping builds resolve EngineConfigDir() to a relative "Config" path, and
# ResourceManager::Init() chdirs to the executable directory. Keep TestConf.ini
# next to the binary so FlingTests pass regardless of launch CWD.
add_custom_command( TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${FLING_ROOT_DIR}/Config"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>/Config"
    COMMENT "Copying Config next to ${PROJECT_NAME}"
)

# Register each TEST_CASE with CTest (ctest --test-dir build). PRE_TEST lists the
# tests when ctest runs instead of running the binary at build time. Tests run
# from the repo root, the same as CI, so the engine writes to ./Logs.
catch_discover_tests( ${PROJECT_NAME}
    DISCOVERY_MODE PRE_TEST
    WORKING_DIRECTORY "${FLING_ROOT_DIR}"
)
