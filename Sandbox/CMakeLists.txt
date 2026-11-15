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

    # The physics test level: the ball rolls into the pyramid of cubes. The tick of the hit
    # and the checksum are pinned for the Jolt provider at the default 30 Hz (two collision
    # steps a tick, PhysicsCollisionSteps), the same in every exec mode. Another provider
    # only has to report the hit. With no physics library (FLING_PHYSICS_PROVIDER=None)
    # nothing moves, so there is no test.
    if( NOT FLING_PHYSICS_PROVIDER STREQUAL "None" )
        if( FLING_PHYSICS_PROVIDER STREQUAL "Jolt" )
            set( _physics_result "PhysicsTest: ball hit cube on tick 78.*checksum 260.239876002.*Fling exit: code=0 ticks=300" )
        else()
            set( _physics_result "PhysicsTest: ball hit cube on tick.*Fling exit: code=0 ticks=300" )
        endif()
        foreach( _exec direct inline threaded )
            add_test( NAME "Sandbox: physics test level ${_exec}"
                COMMAND ${PROJECT_NAME} --null-render --unattended --frames 300 --seed 1 --exec ${_exec}
                        --level Levels/PhysicsTest.json
                WORKING_DIRECTORY "${FLING_ROOT_DIR}"
            )
            set_tests_properties( "Sandbox: physics test level ${_exec}" PROPERTIES
                PASS_REGULAR_EXPRESSION "${_physics_result}"
                TIMEOUT 120
            )
        endforeach()
    endif()

    # A level with entities gives the same tick dump on every run and in every exec mode:
    # loading it and starting play write nothing random into the hashed state (EntityGuid
    # is not hashed, and the play snapshot does not change the entities). Two direct runs,
    # then inline and threaded. The physics level needs a physics library to move.
    # With Jolt, the SHA-256 of each 300-tick dump is pinned too: every build configuration,
    # compiler and platform must simulate exactly the same, so one value serves Windows and
    # Linux (the default level is the start level). Simulation code uses SimMath, never the
    # platform's sin or cos (FlingTests/SimMath/CheckSimMath.cmake). These values were
    # measured with GCC and Clang on Linux; the MSVC and clang-cl runs still have to confirm them.
    set( _LevelDumpLevels "Default|" "Level_1|Levels/Level_1.json" )
    if( NOT FLING_PHYSICS_PROVIDER STREQUAL "None" )
        list( APPEND _LevelDumpLevels "PhysicsTest|Levels/PhysicsTest.json" )
    endif()
    set( _LevelDumpSha256_Default "3c8ea95f10bebb9a2572c3a245950a52f90bce9d9fe2ef1f415f70d404de593c" )
    set( _LevelDumpSha256_Level_1 "4b6842bc226437aa64cff15b4c09b25b406e5c98a1333042acc117eaf56a195f" )
    set( _LevelDumpSha256_PhysicsTest "3c64332ad5966e9841b2258976ff60c49517947a2c5bef3957835ef60c212ded" )
    foreach( _case IN LISTS _LevelDumpLevels )
        string( REPLACE "|" ";" _parts "${_case}" )
        list( GET _parts 0 _level )
        list( GET _parts 1 _file )
        set( _level_args "" )
        if( _file )
            list( APPEND _level_args -DLEVEL=${_file} )
        endif()
        if( FLING_PHYSICS_PROVIDER STREQUAL "Jolt" )
            list( APPEND _level_args -DEXPECT_SHA256=${_LevelDumpSha256_${_level}} )
        endif()
        add_test( NAME "Sandbox: tick dump level ${_level}"
            COMMAND ${CMAKE_COMMAND}
                -DSANDBOX=$<TARGET_FILE:${PROJECT_NAME}> -DROOT=${FLING_ROOT_DIR} ${_level_args}
                -DOUT=${CMAKE_CURRENT_BINARY_DIR}/TickDumps/level-${_level} -DTICKS=300 -DSIM_HZ=30
                "-DVARIANTS=--exec direct|--exec inline|--exec threaded"
                -P "${FLING_ROOT_DIR}/Sandbox/Tests/CompareExecModes.cmake"
        )
        set_tests_properties( "Sandbox: tick dump level ${_level}" PROPERTIES
            PASS_REGULAR_EXPRESSION "Tick dumps are the same in every exec mode"
            TIMEOUT 300
        )
    endforeach()

    # The ECS system schedule is valid, and --dump-schedule lists it and exits
    add_test( NAME "Sandbox: system schedule"
        COMMAND ${PROJECT_NAME} --null-render --unattended --dump-schedule
        WORKING_DIRECTORY "${FLING_ROOT_DIR}"
    )
    set_tests_properties( "Sandbox: system schedule" PROPERTIES
        PASS_REGULAR_EXPRESSION "Fling exit: code=0 ticks=0"
    )

    # The Rotator and Mover systems run headless. The checksum line at exit lets a
    # change to gameplay code be compared with the run before it: the same in every
    # exec mode, at the default 30 Hz.
    if( NOT DEFINE_SHIPPING )
        foreach( _exec direct inline threaded )
            add_test( NAME "Sandbox: systems move the lighting test ${_exec}"
                COMMAND ${PROJECT_NAME} --null-render --unattended --frames 120 --seed 1 --exec ${_exec}
                        --replay-input Sandbox/Tests/Input/toggle_rotate_and_lights.input.jsonl
                WORKING_DIRECTORY "${FLING_ROOT_DIR}"
            )
            set_tests_properties( "Sandbox: systems move the lighting test ${_exec}" PROPERTIES
                PASS_REGULAR_EXPRESSION "Sandbox: 754 transforms, checksum 47091.359022474.*Fling exit: code=0 ticks=120"
                TIMEOUT 120
            )

            # Render interpolation is presentation only: off, the checksum is the same
            add_test( NAME "Sandbox: systems move the lighting test ${_exec} interpolation off"
                COMMAND ${PROJECT_NAME} --null-render --unattended --frames 120 --seed 1 --exec ${_exec}
                        --interpolation off --replay-input Sandbox/Tests/Input/toggle_rotate_and_lights.input.jsonl
                WORKING_DIRECTORY "${FLING_ROOT_DIR}"
            )
            set_tests_properties( "Sandbox: systems move the lighting test ${_exec} interpolation off" PROPERTIES
                PASS_REGULAR_EXPRESSION "interpolation off \\(command line\\).*Sandbox: 754 transforms, checksum 47091.359022474.*Fling exit: code=0 ticks=120"
                TIMEOUT 120
            )
        endforeach()
    endif()

    # Render interpolation (Docs/threading-model.md, "The two blend states"): the Rotator
    # spheres turn at a constant rate, drawn at 4 frames per tick. Sandbox.MotionProbe measures
    # the drawn turn of each frame: every frame turns by the same angle, and none repeats the
    # frame before (with no interpolation 3 frames of 4 would repeat). Direct and Inline only:
    # they run each tick in a frame, so the frame that first has a tick is fixed. A threaded
    # headless frame never waits for the game thread, so which frame first has a tick depends
    # on thread timing; GraphicsTests "[interp][threads]" steps the game thread on a virtual
    # clock to test threaded motion exactly.
    if( NOT DEFINE_SHIPPING )
        foreach( _mode direct inline )
            add_test( NAME "Sandbox: rendered motion is even ${_mode}"
                COMMAND ${PROJECT_NAME} --null-render --unattended --frames 240 --seed 1 --frames-per-tick 4
                        --exec ${_mode} --replay-input Sandbox/Tests/Input/toggle_rotate_and_lights.input.jsonl
                WORKING_DIRECTORY "${FLING_ROOT_DIR}"
            )
            set_tests_properties( "Sandbox: rendered motion is even ${_mode}" PROPERTIES
                PASS_REGULAR_EXPRESSION "rendered turn over [0-9]+ frames, .* even yes, repeated frames 0.*Fling exit: code=0 ticks=60"
                TIMEOUT 120
            )
        endforeach()
    endif()

    # The look camera replay, with a tick every 2 frames: the frame side runs between the
    # ticks. The ticks and the checksum must not change; what the frames show is tested in
    # SandboxExampleTests (LookCameraTests), and the tick ledger below compares the camera's state
    # hash across frame patterns. The file's "tick" is a frame at 60 Hz with 2 frames per
    # tick (1/120 s), so the test runs at 60 Hz.
    if( NOT DEFINE_SHIPPING )
        add_test( NAME "Sandbox: look camera replay"
            COMMAND ${PROJECT_NAME} --null-render --unattended --frames 40 --seed 1 --frames-per-tick 2 --sim-hz 60
                    --replay-input Sandbox/Tests/Input/look_camera.input.jsonl
            WORKING_DIRECTORY "${FLING_ROOT_DIR}"
        )
        set_tests_properties( "Sandbox: look camera replay" PROPERTIES
            PASS_REGULAR_EXPRESSION "checksum 44871.195499957.*Fling exit: code=0 ticks=20"
            TIMEOUT 120
        )
    endif()

    # The tick ledger does not depend on the frame rate or the exec mode (--dump-ticks), at
    # the default 30 Hz and at 60 Hz. A recording with per-tick cuts, replayed with 1 and 3
    # frames per tick: the same input cut and state hash on every tick. A file with no cuts
    # (assigned by time), with 1 and 2 frames per tick: the same. Cuts replay only at the
    # rate they were recorded at, so each rate has its recording; the 60 Hz one is a
    # version 1 file (no rate stored: 60 Hz is assumed).
    if( NOT DEFINE_SHIPPING )
        foreach( _case "cuts|look_camera_cuts|30|1|3"
                       "time|look_camera|30|1|2"
                       "cuts 60 Hz|look_camera_cuts_60hz|60|1|3"
                       "time 60 Hz|look_camera|60|1|2" )
            string( REPLACE "|" ";" _parts "${_case}" )
            list( GET _parts 0 _name )
            list( GET _parts 1 _file )
            list( GET _parts 2 _hz )
            list( GET _parts 3 _fpt_a )
            list( GET _parts 4 _fpt_b )
            string( REPLACE " " "-" _dir "${_name}" )
            add_test( NAME "Sandbox: tick dump replay by ${_name}"
                COMMAND ${CMAKE_COMMAND}
                    -DSANDBOX=$<TARGET_FILE:${PROJECT_NAME}> -DROOT=${FLING_ROOT_DIR}
                    -DINPUT=Sandbox/Tests/Input/${_file}.input.jsonl
                    -DOUT=${CMAKE_CURRENT_BINARY_DIR}/TickDumps/${_dir} -DTICKS=120 -DFPT_A=${_fpt_a} -DFPT_B=${_fpt_b}
                    -DSIM_HZ=${_hz}
                    -P "${FLING_ROOT_DIR}/Sandbox/Tests/CompareTickDumps.cmake"
            )
            set_tests_properties( "Sandbox: tick dump replay by ${_name}" PROPERTIES
                PASS_REGULAR_EXPRESSION "Tick dumps are the same"
                TIMEOUT 180
            )
        endforeach()

        # A recording made at 60 Hz does not replay at 30 Hz: the run stops with code 3 and
        # names the rate to use (Skills/input-playback.md, tick cuts and their rate)
        add_test( NAME "Sandbox: replay of cuts at another tick rate is refused"
            COMMAND ${PROJECT_NAME} --null-render --unattended --frames 10 --seed 1
                    --replay-input Sandbox/Tests/Input/look_camera_cuts_60hz.input.jsonl
            WORKING_DIRECTORY "${FLING_ROOT_DIR}"
        )
        set_tests_properties( "Sandbox: replay of cuts at another tick rate is refused" PROPERTIES
            PASS_REGULAR_EXPRESSION "recorded at 60 ticks a second, and the simulation runs at 30. Replay it with --sim-hz 60.*Fling exit: code=3"
            TIMEOUT 120
        )

        # The tick ledger does not depend on threads (Docs/threading-model.md section 9): the same dump in
        # --exec direct, inline (also with frames between ticks) and threaded, at several
        # frame caps of the threaded frame side, and with random waits on both threads, at
        # the default 30 Hz (and the cuts at 60 Hz too).
        # In every exec mode the main thread polls the input; a threaded run's game thread
        # cuts it by the timeline and waits for the main thread's watermark
        # (Docs/time-and-ticks.md, the live input rule).
        set( _ExecVariants "--exec inline|--exec inline --frames-per-tick 3|--exec threaded|--exec threaded --frames-per-tick 3" )
        set( _RateVariants "--exec threaded --max-fps 30|--exec threaded --max-fps 60|--exec threaded --max-fps 144|--exec threaded --max-fps 1000" )
        set( _JitterVariants "" )
        foreach( _seed RANGE 1 10 )
            list( APPEND _JitterVariants "--exec threaded --max-fps 1000 --thread-jitter ${_seed}:2000" )
        endforeach()
        string( REPLACE ";" "|" _JitterVariants "${_JitterVariants}" )
        # "stasis" replays G (Sandbox.Stasis, gameplay that stops the look camera), with F and
        # mouse moves inside the stasis.
        foreach( _case "cuts|look_camera_cuts|30|120|${_ExecVariants}"
                       "cuts 60 Hz|look_camera_cuts_60hz|60|120|${_ExecVariants}"
                       "time|look_camera|30|600|${_ExecVariants}|${_RateVariants}"
                       "jitter|look_camera_cuts|30|120|${_JitterVariants}"
                       "stasis|stasis|30|150|${_ExecVariants}|${_RateVariants}" )
            string( REPLACE "|" ";" _parts "${_case}" )
            list( POP_FRONT _parts _name _file _hz _ticks )
            string( REPLACE ";" "|" _variants "${_parts}" )
            string( REPLACE " " "-" _dir "${_name}" )
            # The look camera replay's 600-tick dump is pinned, as the level dumps above
            set( _pin "" )
            if( _name STREQUAL "time" AND FLING_PHYSICS_PROVIDER STREQUAL "Jolt" )
                set( _pin -DEXPECT_SHA256=14bce16318b1fdaf22792576cbeca4d8df040b03e9c993a0322db28ae32ea5e8 )
            endif()
            add_test( NAME "Sandbox: tick dump exec modes ${_name}"
                COMMAND ${CMAKE_COMMAND}
                    -DSANDBOX=$<TARGET_FILE:${PROJECT_NAME}> -DROOT=${FLING_ROOT_DIR} ${_pin}
                    -DINPUT=Sandbox/Tests/Input/${_file}.input.jsonl
                    -DOUT=${CMAKE_CURRENT_BINARY_DIR}/TickDumps/exec-${_dir} -DTICKS=${_ticks}
                    -DSIM_HZ=${_hz}
                    "-DVARIANTS=${_variants}"
                    -P "${FLING_ROOT_DIR}/Sandbox/Tests/CompareExecModes.cmake"
            )
            set_tests_properties( "Sandbox: tick dump exec modes ${_name}" PROPERTIES
                PASS_REGULAR_EXPRESSION "Tick dumps are the same in every exec mode"
                TIMEOUT 300
            )
        endforeach()
    endif()

    # A threaded headless run: the game thread runs 600 ticks, the main thread the frames
    add_test( NAME "Sandbox: threaded headless run"
        COMMAND ${PROJECT_NAME} --null-render --unattended --frames 600 --seed 1 --exec threaded
        WORKING_DIRECTORY "${FLING_ROOT_DIR}"
    )
    set_tests_properties( "Sandbox: threaded headless run" PROPERTIES
        PASS_REGULAR_EXPRESSION "Run .threaded.: [0-9]+ frames on the main thread, 600 ticks.*Fling exit: code=0 ticks=600"
        TIMEOUT 120
    )

    # A Quit from a tick on the game thread ends the run at that tick, as in --exec direct
    # (the outbox): the Escape press of the replay at 0.5 s goes out on tick 0.5 * rate, so
    # the game quits after 16 ticks at 30 Hz and 31 at 60 Hz
    if( NOT DEFINE_SHIPPING )
        foreach( _case "30|16" "60|31" )
            string( REPLACE "|" ";" _parts "${_case}" )
            list( GET _parts 0 _hz )
            list( GET _parts 1 _ticks )
            add_test( NAME "Sandbox: threaded replay input quits at ${_hz} Hz"
                COMMAND ${PROJECT_NAME} --null-render --unattended --frames 600 --seed 1 --exec threaded --sim-hz ${_hz}
                        --replay-input Sandbox/Tests/Input/escape_quits.input.jsonl
                WORKING_DIRECTORY "${FLING_ROOT_DIR}"
            )
            set_tests_properties( "Sandbox: threaded replay input quits at ${_hz} Hz" PROPERTIES
                PASS_REGULAR_EXPRESSION "Fling exit: code=0 ticks=${_ticks} "
                TIMEOUT 120
            )
        endforeach()
    endif()

    # A bad launch argument must exit with code 3 (ExitCode::BadArguments)
    add_test( NAME "Sandbox: bad launch argument"
        COMMAND ${PROJECT_NAME} --null-render --unattended --frames not-a-number
        WORKING_DIRECTORY "${FLING_ROOT_DIR}"
    )
    set_tests_properties( "Sandbox: bad launch argument" PROPERTIES
        PASS_REGULAR_EXPRESSION "Fling exit: code=3 ticks=0"
    )

    # A simulation rate out of range is a bad argument too, and the message names the flag
    add_test( NAME "Sandbox: bad simulation rate"
        COMMAND ${PROJECT_NAME} --null-render --unattended --frames 10 --sim-hz 0
        WORKING_DIRECTORY "${FLING_ROOT_DIR}"
    )
    set_tests_properties( "Sandbox: bad simulation rate" PROPERTIES
        PASS_REGULAR_EXPRESSION "--sim-hz must be from 1 to 1000 ticks a second, not 0.*Fling exit: code=3 ticks=0"
    )

    # The startup line says each run setting and where it came from
    add_test( NAME "Sandbox: run settings line"
        COMMAND ${PROJECT_NAME} --null-render --unattended --frames 10 --seed 1
        WORKING_DIRECTORY "${FLING_ROOT_DIR}"
    )
    set_tests_properties( "Sandbox: run settings line" PROPERTIES
        PASS_REGULAR_EXPRESSION "Run settings: tick rate 30 Hz \\(default\\), exec threaded \\(default\\), frame cap none \\(default for --null-render\\), vsync on \\(default\\), interpolation on \\(default\\).*Fling exit: code=0 ticks=10"
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
        # Replay an input file on one thread. The Escape press at 0.5 s goes out on tick 15
        # at the default 30 Hz, so the game quits after 16 ticks.
        add_test( NAME "Sandbox: replay input quits"
            COMMAND ${PROJECT_NAME} --null-render --unattended --frames 600 --seed 1 --exec direct
                    --replay-input Sandbox/Tests/Input/escape_quits.input.jsonl
            WORKING_DIRECTORY "${FLING_ROOT_DIR}"
        )
        set_tests_properties( "Sandbox: replay input quits" PROPERTIES
            PASS_REGULAR_EXPRESSION "Fling exit: code=0 ticks=16 "
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
