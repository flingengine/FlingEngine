project( "FlingTests" )

##################### Linking #################

# Only reached when FLING_BUILD_TESTS is ON (see the root CMakeLists.txt)
find_package(Catch2 3 REQUIRED)
include(Catch)

# One Catch2 executable per module that owns tests. Each links Catch2 and only
# that module; public dependencies come along transitively. An empty folder
# adds no target, so a module with no real tests (Editor) stays out. Graphics
# tests use no GPU: they test the CPU side of the renderer.
# Extra libraries after MODULE are linked too (for example the physics provider).
function( fling_add_module_tests MODULE )
	file( GLOB _sources CONFIGURE_DEPENDS
		"${CMAKE_CURRENT_SOURCE_DIR}/${MODULE}/*.cpp"
	)
	if( NOT _sources )
		return()
	endif()

	set( _target "${MODULE}Tests" )
	add_executable( ${_target} ${_sources} )
	fling_configure_target( ${_target} )
	fling_set_bin_output( ${_target} )

	# Catch2WithMain is the only main. Test sources do not define CATCH_CONFIG_MAIN.
	target_link_libraries( ${_target} PRIVATE
		Catch2::Catch2WithMain
		Fling::${MODULE}
		${ARGN}
	)

	# PRE_TEST lists the tests when ctest runs instead of running the binary at
	# build time. Tests run from the repo root, the same as CI, so the engine
	# writes to ./Logs.
	catch_discover_tests( ${_target}
		DISCOVERY_MODE PRE_TEST
		WORKING_DIRECTORY "${FLING_ROOT_DIR}"
	)

	# Shipping builds resolve EngineConfigDir() to a relative "Config" path, and
	# ResourceManager::Init() chdirs to the executable directory. Keep TestConf.ini
	# next to ResourcesTests so those cases pass regardless of launch CWD.
	if( MODULE STREQUAL "Resources" )
		add_custom_command( TARGET ${_target} POST_BUILD
			COMMAND ${CMAKE_COMMAND} -E copy_directory
				"${FLING_ROOT_DIR}/Config"
				"$<TARGET_FILE_DIR:${_target}>/Config"
			COMMENT "Copying Config next to ${_target}"
		)
	endif()
endfunction()

fling_add_module_tests( Core )
fling_add_module_tests( Resources )
fling_add_module_tests( Engine )
fling_add_module_tests( Graphics )
fling_add_module_tests( Physics Fling::DefaultPhysicsProvider )
fling_add_module_tests( Application )

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
	# Each test runs cmake --build on this build tree. Do not run two at the same time.
	set_tests_properties( "Isolation: ${NAME}" PROPERTIES RESOURCE_LOCK "fling-engine-build" )
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
fling_add_isolation_test( ResourcesCannotSeeEngine Fling::Resources "World.h"           HIDDEN )
fling_add_isolation_test( EngineHeaders            Fling::Engine    "World.h"           VISIBLE )
fling_add_isolation_test( EngineCannotSeeGraphics  Fling::Engine   "VulkanApp.h"       HIDDEN )
fling_add_isolation_test( EngineCannotSeeApplication Fling::Engine "FlingEngine.h"     HIDDEN )
fling_add_isolation_test( PhysicsHeaders           Fling::Physics   "Physics.h"         VISIBLE )
fling_add_isolation_test( PhysicsCannotSeeGraphics Fling::Physics   "VulkanApp.h"       HIDDEN )
fling_add_isolation_test( PhysicsCannotSeeBullet   Fling::Physics   "btBulletDynamicsCommon.h" HIDDEN )
fling_add_isolation_test( PhysicsCannotSeeJolt     Fling::Physics   "Jolt/Jolt.h"       HIDDEN )
# The engine finds physics providers by searching folders, including a provider
# outside the engine (FlingTests/PhysicsProviderExample)
add_test( NAME "Physics: providers are found"
	COMMAND ${CMAKE_COMMAND} -DFLING_ROOT_DIR=${FLING_ROOT_DIR}
		-P "${CMAKE_CURRENT_SOURCE_DIR}/PhysicsProviderExample/FindProvidersTest.cmake"
)

# A provider links its physics library PRIVATE, so the game never sees it,
# whichever provider is selected
fling_add_isolation_test( ApplicationCannotSeeJolt   Fling::Application "Jolt/Jolt.h"              HIDDEN )
fling_add_isolation_test( ApplicationCannotSeeBullet Fling::Application "btBulletDynamicsCommon.h" HIDDEN )
fling_add_isolation_test( GraphicsHeaders          Fling::Graphics  "VulkanApp.h"       VISIBLE )
fling_add_isolation_test( GraphicsCannotSeeApplication Fling::Graphics "FlingEngine.h"  HIDDEN )
fling_add_isolation_test( GraphicsCannotSeeEditor  Fling::Graphics  "BaseEditor.h"      HIDDEN )
fling_add_isolation_test( ApplicationHeaders       Fling::Application "FlingEngine.h"   VISIBLE )
fling_add_isolation_test( ApplicationCannotSeeEditor Fling::Application "BaseEditor.h"  HIDDEN )
if( WITH_EDITOR_FLAG )
	fling_add_isolation_test( EditorHeaders        Fling::Editor    "BaseEditor.h"      VISIBLE )
endif()

# ECS compile checks. Each case builds EcsCompileFail.cpp with one
# FLING_ECS_CASE_<Name> defined, and passes only if the build fails with that
# rule's static_assert message (Plans/ecs-systems.md section 3.6). The "Good"
# case defines nothing and must build.
function( fling_add_ecs_compile_check CASE MESSAGE )
	set( _target "EcsCompileFail_${CASE}" )
	add_executable( ${_target} EXCLUDE_FROM_ALL EcsCompileFail/EcsCompileFail.cpp )
	target_link_libraries( ${_target} PRIVATE Fling::Engine )
	if( NOT CASE STREQUAL "Good" )
		target_compile_definitions( ${_target} PRIVATE "FLING_ECS_CASE_${CASE}" )
	endif()
	fling_configure_target( ${_target} )
	set_target_properties( ${_target} PROPERTIES FOLDER "FlingTests/EcsCompileFail" )

	add_test( NAME "ECS compile check: ${CASE}"
		COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target ${_target} --config $<CONFIG>
	)
	set_tests_properties( "ECS compile check: ${CASE}" PROPERTIES RESOURCE_LOCK "fling-engine-build" )
	if( NOT CASE STREQUAL "Good" )
		set_tests_properties( "ECS compile check: ${CASE}" PROPERTIES PASS_REGULAR_EXPRESSION "${MESSAGE}" )
	endif()
endfunction()

fling_add_ecs_compile_check( Good                   "" )
fling_add_ecs_compile_check( QueryUndeclared        "queries a component that its Access<...> does not allow" )
fling_add_ecs_compile_check( WriteThroughRead       "queries a component that its Access<...> does not allow" )
fling_add_ecs_compile_check( HasUndeclared          "checks a component that its Access<...> does not allow" )
fling_add_ecs_compile_check( SpawnUndeclared        "Add Fling::Spawns to the access list" )
fling_add_ecs_compile_check( RawStorageUndeclared   "Add Fling::RawStorage to the access list" )
fling_add_ecs_compile_check( FrameCommands          "Only a fixed phase system has Commands" )
fling_add_ecs_compile_check( MemberState            "A system cannot have member state" )
fling_add_ecs_compile_check( WrongUpdateRate        "A fixed phase system implements FixedUpdate" )
fling_add_ecs_compile_check( FixedReadsRealTime     "A fixed rate system cannot read RealFrameTime" )
fling_add_ecs_compile_check( FixedReadsPresentation "A fixed rate system cannot read a Presentation component" )
fling_add_ecs_compile_check( FrameWritesSimulation  "A frame rate system cannot write a Simulation component" )
fling_add_ecs_compile_check( WritesSimTime          "No system writes SimTime or RealFrameTime" )
fling_add_ecs_compile_check( SingletonNotOwned      "Only a writer of a singleton makes it" )
fling_add_ecs_compile_check( QueueRead              "A queue is not read or written directly" )
fling_add_ecs_compile_check( NotAComponent          "is not a Fling::Component" )
fling_add_ecs_compile_check( NameMismatch           "saved .Name must end with its C.. type name" )
fling_add_ecs_compile_check( DuplicateAccess        "A type appears more than once in Access" )

# Every first-party vk* call must be named in VulkanFunctions.inl.
add_test(NAME "Vulkan calls are listed"
	COMMAND ${CMAKE_COMMAND}
		-DSOURCE_ROOT=${FLING_ROOT_DIR}
		-P "${CMAKE_CURRENT_SOURCE_DIR}/VulkanLoad/CheckVulkanCalls.cmake"
)

# Unattended crash tests. CrashProbe crashes on purpose after
# CrashHandler::InstallUnattended. Each test passes only if the process exits
# with code 2 and prints a stack trace. A modal dialog makes the test time out.
if( WITH_CRASH_HANDLER_FLAG )
	add_executable( CrashProbe CrashProbe/CrashProbe.cpp )
	fling_configure_target( CrashProbe )
	fling_set_bin_output( CrashProbe )
	target_link_libraries( CrashProbe PRIVATE Fling::Core )

	foreach( _mode abort assert segv throw )
		add_test( NAME "Unattended crash: ${_mode}"
			COMMAND ${CMAKE_COMMAND}
				-DPROBE=$<TARGET_FILE:CrashProbe>
				-DMODE=${_mode}
				-P "${CMAKE_CURRENT_SOURCE_DIR}/CrashProbe/ExpectCrash.cmake"
		)
		set_tests_properties( "Unattended crash: ${_mode}" PROPERTIES TIMEOUT 90 )
	endforeach()
endif()
