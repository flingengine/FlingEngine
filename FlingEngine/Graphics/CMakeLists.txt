# Graphics module: the Vulkan renderer, window, GLFW input backends, GPU
# resources (Texture, HDRImage), and render components (MeshRenderer, lights).
# Depends on Core, Resources, and Engine. It must not include Editor or
# Application headers: developer UI plugs in through IDebugUI.

find_package(Vulkan REQUIRED)

fling_add_module(Graphics
	PUBLIC_DEPS
		Core
		Resources
		Engine
	# Public headers do not include Vulkan, GLFW, SPIRV-Cross, or stb.
	# tinyobjloader and ImGui stay private as well.
	PRIVATE_LIBS
		Vulkan::Vulkan
		glfw
		spirv-cross-core
		spirv-cross-cpp
		spirv-cross-reflect
		FlingExt::stb
		FlingExt::tinyobjloader
)

if(WITH_IMGUI_FLAG)
	target_link_libraries(Graphics PRIVATE ImGui)
endif()
