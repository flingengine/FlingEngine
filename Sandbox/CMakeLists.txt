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

################# Headless tests ######################

# Run the Sandbox game with no window and no GPU, the same way CI and agents do.
# These run on CI machines that have no display and no Vulkan device.
if( FLING_BUILD_TESTS )
    add_test( NAME "Sandbox: headless run"
        COMMAND ${PROJECT_NAME} --null-render --unattended --frames 120 --seed 1
        WORKING_DIRECTORY "${FLING_ROOT_DIR}"
    )

    add_test( NAME "Sandbox: headless json logs"
        COMMAND ${PROJECT_NAME} --null-render --unattended --frames 10 --seed 1 --log-format json
        WORKING_DIRECTORY "${FLING_ROOT_DIR}"
    )
    set_tests_properties( "Sandbox: headless json logs" PROPERTIES
        PASS_REGULAR_EXPRESSION "{\"event\":\"exit\",\"code\":0,\"ticks\":10,\"seed\":1,\"render\":\"null\""
    )

    # A bad launch argument must exit with code 3 (ExitCode::BadArguments)
    add_test( NAME "Sandbox: bad launch argument"
        COMMAND ${PROJECT_NAME} --null-render --unattended --frames not-a-number
        WORKING_DIRECTORY "${FLING_ROOT_DIR}"
    )
    set_tests_properties( "Sandbox: bad launch argument" PROPERTIES
        PASS_REGULAR_EXPRESSION "Fling exit: code=3 ticks=0"
    )

    # The linked binary must not import libvulkan / vulkan-1.dll. The loader is
    # opened at runtime by Fling::DynamicLibrary.
    add_test(NAME "Sandbox: no Vulkan import"
        COMMAND ${CMAKE_COMMAND}
            -DBINARY=$<TARGET_FILE:${PROJECT_NAME}>
            -P "${FLING_ROOT_DIR}/FlingTests/VulkanLoad/ExpectNoVulkanImport.cmake"
    )

    set_tests_properties(
        "Sandbox: headless run"
        "Sandbox: headless json logs"
        "Sandbox: bad launch argument"
        "Sandbox: no Vulkan import"
        PROPERTIES TIMEOUT 120
    )
endif()
