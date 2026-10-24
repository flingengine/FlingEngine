# Gameplay module: World, Level, Game, cameras, Transform, and the component
# type registry. Depends on Core and Resources only. It must not include
# Graphics, Editor, or Engine headers: Graphics registers its own component
# types through RegisterGraphicsComponents().

fling_add_module(Gameplay
	PUBLIC_DEPS
		Core
		Resources	# ComponentTypeRegistry.h exposes JsonArchive
)

# Test-only API (ComponentTypeRegistry::ResetForTests) is compiled in only when
# FlingTests is built, so shipping builds do not contain it.
if(FLING_BUILD_TESTS)
	target_compile_definitions(Gameplay PUBLIC FLING_WITH_TEST_HOOKS)
endif()
