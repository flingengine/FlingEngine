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
	# Public headers expose these types (FlingVulkan.h, DesktopWindow.h,
	# Shader.h, Texture.h), so they cannot be private yet
	PUBLIC_LIBS
		Vulkan::Vulkan
		glfw
		spirv-cross-core
		spirv-cross-cpp
		spirv-cross-reflect
		FlingExt::stb
	PRIVATE_LIBS
		FlingExt::tinyobjloader
)

if(WITH_IMGUI_FLAG)
	target_link_libraries(Graphics PRIVATE ImGui)
endif()
