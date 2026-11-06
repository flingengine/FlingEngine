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

# A schedule error fails the build (--validate-schedule after each link)
fling_validate_schedule( ${PROJECT_NAME} )

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

    # The physics test level: the ball rolls into the pyramid of cubes. With no
    # physics library (FLING_PHYSICS_PROVIDER=None) nothing moves, so there is no test.
    if( NOT FLING_PHYSICS_PROVIDER STREQUAL "None" )
        add_test( NAME "Sandbox: physics test level"
            COMMAND ${PROJECT_NAME} --null-render --unattended --frames 300 --seed 1
                    --level Levels/PhysicsTest.json
            WORKING_DIRECTORY "${FLING_ROOT_DIR}"
        )
        set_tests_properties( "Sandbox: physics test level" PROPERTIES
            PASS_REGULAR_EXPRESSION "PhysicsTest: ball hit cube on tick"
            TIMEOUT 120
        )
    endif()

    # The ECS system schedule is valid, and --dump-schedule lists it and exits
    add_test( NAME "Sandbox: system schedule"
        COMMAND ${PROJECT_NAME} --null-render --unattended --dump-schedule
        WORKING_DIRECTORY "${FLING_ROOT_DIR}"
    )
    set_tests_properties( "Sandbox: system schedule" PROPERTIES
        PASS_REGULAR_EXPRESSION "Fling exit: code=0 ticks=0"
    )

    # The Rotator and Mover systems run headless. The checksum line at exit lets a
    # change to gameplay code be compared with the run before it.
    if( NOT DEFINE_SHIPPING )
        add_test( NAME "Sandbox: systems move the lighting test"
            COMMAND ${PROJECT_NAME} --null-render --unattended --frames 120 --seed 1
                    --replay-input Sandbox/Tests/Input/toggle_rotate_and_lights.input.jsonl
            WORKING_DIRECTORY "${FLING_ROOT_DIR}"
        )
        set_tests_properties( "Sandbox: systems move the lighting test" PROPERTIES
            PASS_REGULAR_EXPRESSION "Sandbox: 754 transforms, checksum .*Fling exit: code=0 ticks=120"
            TIMEOUT 120
        )
    endif()

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

    # Input replay is compiled out of shipping builds (WITH_INPUT_RECORDING)
    if( NOT DEFINE_SHIPPING )
        # Replay an input file. The Escape press at 0.5 s goes out on tick 30, so the
        # game quits after 31 ticks.
        add_test( NAME "Sandbox: replay input quits"
            COMMAND ${PROJECT_NAME} --null-render --unattended --frames 600 --seed 1
                    --replay-input Sandbox/Tests/Input/escape_quits.input.jsonl
            WORKING_DIRECTORY "${FLING_ROOT_DIR}"
        )
        set_tests_properties( "Sandbox: replay input quits" PROPERTIES
            PASS_REGULAR_EXPRESSION "Fling exit: code=0 ticks=31 "
        )

        # An input file that cannot be read must exit with code 3 (ExitCode::BadArguments)
        add_test( NAME "Sandbox: replay input bad file"
            COMMAND ${PROJECT_NAME} --null-render --unattended --frames 10 --seed 1
                    --replay-input Sandbox/Tests/Input/does_not_exist.input.jsonl
            WORKING_DIRECTORY "${FLING_ROOT_DIR}"
        )
        set_tests_properties( "Sandbox: replay input bad file" PROPERTIES
            PASS_REGULAR_EXPRESSION "Fling exit: code=3"
        )
        set_tests_properties( "Sandbox: replay input quits" "Sandbox: replay input bad file" PROPERTIES TIMEOUT 120 )
    else()
        add_test( NAME "Sandbox: replay input is off in shipping"
            COMMAND ${PROJECT_NAME} --null-render --unattended --frames 10 --seed 1
                    --replay-input Sandbox/Tests/Input/escape_quits.input.jsonl
            WORKING_DIRECTORY "${FLING_ROOT_DIR}"
        )
        set_tests_properties( "Sandbox: replay input is off in shipping" PROPERTIES
            PASS_REGULAR_EXPRESSION "--replay-input is not available in shipping builds"
            TIMEOUT 120
        )
    endif()

    set_tests_properties(
        "Sandbox: headless run"
        "Sandbox: headless json logs"
        "Sandbox: bad launch argument"
        "Sandbox: system schedule"
        "Sandbox: no Vulkan import"
        PROPERTIES TIMEOUT 120
    )
endif()
