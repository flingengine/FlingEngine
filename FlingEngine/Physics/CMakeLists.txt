# Physics module (stub): proves that a system can use Engine types (Transform)
# without Vulkan on its include path or link line. It must not depend on
# Graphics. Not yet called from the engine loop.

fling_add_module(Physics
	PUBLIC_DEPS
		Core
		Engine
)
