# Bullet physics provider (external/bullet3). The engine adds this folder only
# when FLING_PHYSICS_PROVIDER is Bullet, so Bullet is configured and compiled
# only then. Bullet is a PRIVATE dependency, so no other module sees Bullet
# headers.

include(FlingBullet)

fling_add_physics_provider(Bullet
	LIBS
		FlingExt::bullet
)
