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
# Isolation/ sources build as separate targets (see fling_add_isolation_test below)
list( FILTER _source_list EXCLUDE REGEX "^${CMAKE_CURRENT_SOURCE_DIR}/Isolation/" )

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

# Module isolation tests. Each one builds IsolationCheck.cpp with one header,
# linked against one module only:
#   VISIBLE: the header must compile (proves the module's public headers are
#            enough on their own).
#   HIDDEN:  the build must fail because the header is not found (proves the
#            module cannot reach a module it does not depend on).
function( fling_add_isolation_test NAME MODULE HEADER EXPECT )
    set( _target "Isolation_${NAME}" )
    add_executable( ${_target} EXCLUDE_FROM_ALL Isolation/IsolationCheck.cpp )
    target_link_libraries( ${_target} PRIVATE ${MODULE} )
    target_compile_definitions( ${_target} PRIVATE "FLING_ISOLATION_HEADER=\"${HEADER}\"" )
    fling_configure_target( ${_target} )
    set_target_properties( ${_target} PROPERTIES FOLDER "FlingTests/Isolation" )

    add_test( NAME "Isolation: ${NAME}"
        COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target ${_target} --config $<CONFIG>
    )
    if( EXPECT STREQUAL "HIDDEN" )
        # Pass only on a "header not found" error (GCC, Clang, MSVC wording),
        # not on any build error: the failed command line also names the header.
        string( REPLACE "." "\\." _h "${HEADER}" )
        set_tests_properties( "Isolation: ${NAME}" PROPERTIES PASS_REGULAR_EXPRESSION
            "${_h}: No such file or directory;'${_h}' file not found;Cannot open include file: '${_h}'"
        )
    endif()
endfunction()

fling_add_isolation_test( CoreHeaders              Fling::Core      "Logger.h"          VISIBLE )
fling_add_isolation_test( CoreCannotSeeResources   Fling::Core      "ResourceManager.h" HIDDEN )
fling_add_isolation_test( ResourcesHeaders         Fling::Resources "ResourceManager.h" VISIBLE )
fling_add_isolation_test( ResourcesCannotSeeGameplay Fling::Resources "World.h"         HIDDEN )
fling_add_isolation_test( GameplayHeaders          Fling::Gameplay  "World.h"           VISIBLE )
fling_add_isolation_test( GameplayCannotSeeGraphics Fling::Gameplay "VulkanApp.h"       HIDDEN )
