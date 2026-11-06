# Jolt physics provider (external/JoltPhysics), built with
# CROSS_PLATFORM_DETERMINISTIC. The engine adds this folder only when
# FLING_PHYSICS_PROVIDER is Jolt, so Jolt is configured and compiled only then.
# Jolt is a PRIVATE dependency, so no other module sees Jolt headers.

include(FlingJolt)

fling_add_physics_provider(Jolt
	LIBS
		FlingExt::jolt
)
