# Engine module: the foundational world layer that everything above builds on.
# World, Level, the Game base class, Transform, cameras, and the component type
# registry. Depends on Core and Resources only. It must not include Graphics,
# Editor, or Application headers: Graphics registers its own component types
# through RegisterGraphicsComponents().
#
# Actual gameplay systems (AI, abilities, ...) belong in their own modules that
# depend on Engine, not in Engine itself.

fling_add_module(Engine
	PUBLIC_DEPS
		Core
		Resources	# ComponentTypeRegistry.h exposes JsonArchive
)

# Test-only API (ComponentTypeRegistry::ResetForTests) is compiled in only when
# FlingTests is built, so shipping builds do not contain it.
if(FLING_BUILD_TESTS)
	target_compile_definitions(Engine PUBLIC FLING_WITH_TEST_HOOKS)
endif()
