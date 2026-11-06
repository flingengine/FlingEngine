# An example physics provider that lives outside the engine. It shows how a game
# adds its own provider with no engine change:
#
#   cmake -B build -DFLING_PHYSICS_PROVIDER=Example \
#         -DFLING_PHYSICS_PROVIDER_PATHS=<repo>/FlingTests/PhysicsProviderExample
#
# A game would instead append its folder to FLING_PHYSICS_PROVIDER_PATHS in its
# own CMakeLists.txt before it adds the engine. The "Physics: custom provider"
# test configures the engine with this provider.

fling_add_physics_provider(Example)
