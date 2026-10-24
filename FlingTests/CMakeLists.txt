project( "FlingTests" )

##################### Linking #################

# Only reached when FLING_BUILD_TESTS is ON (see the root CMakeLists.txt)
find_package(Catch2 3 REQUIRED)
include(Catch)

# One Catch2 executable per module that owns tests. Each links Catch2 and only
# that module; public dependencies come along transitively. An empty folder
# adds no target, so a module with no real tests (Graphics, Editor) stays out.
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
fling_add_module_tests( Physics )
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
fling_add_isolation_test( GraphicsHeaders          Fling::Graphics  "VulkanApp.h"       VISIBLE )
fling_add_isolation_test( GraphicsCannotSeeApplication Fling::Graphics "FlingEngine.h"  HIDDEN )
fling_add_isolation_test( GraphicsCannotSeeEditor  Fling::Graphics  "BaseEditor.h"      HIDDEN )
fling_add_isolation_test( ApplicationHeaders       Fling::Application "FlingEngine.h"   VISIBLE )
fling_add_isolation_test( ApplicationCannotSeeEditor Fling::Application "BaseEditor.h"  HIDDEN )
if( WITH_EDITOR_FLAG )
	fling_add_isolation_test( EditorHeaders        Fling::Editor    "BaseEditor.h"      VISIBLE )
endif()
