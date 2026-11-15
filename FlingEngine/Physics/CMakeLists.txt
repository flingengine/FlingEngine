# Physics module: the part of physics that gameplay sees, with no physics
# library in it. It has the components (RigidBody, Collider), the singletons, the
# fixed step clock, the physics systems, and the PhysicsProvider interface. A
# provider module (PhysicsJolt or PhysicsBullet, picked with
# FLING_PHYSICS_PROVIDER) does the simulation. It must not depend on Graphics.
# The Physics.Step system calls Physics::Tick in the Engine.Physics group of each fixed tick.

fling_add_module(Physics
	PUBLIC_DEPS
		Core
		Engine
)

fling_set_deterministic_math(Physics)
