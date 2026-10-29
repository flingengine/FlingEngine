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
		Vulkan::Headers
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

# Engine shaders. Load them by their path under Assets/Shaders plus ".spv",
# for example HS("Deferred/mrt.vert.spv").
fling_add_shaders(Graphics
	BASE_DIR "${FLING_ENGINE_SHADER_DIR}"
	SOURCES
		"${FLING_ENGINE_SHADER_DIR}/CubeMapReflections.vert"
		"${FLING_ENGINE_SHADER_DIR}/CubeMapReflections.frag"
		"${FLING_ENGINE_SHADER_DIR}/PBRDefault.vert"
		"${FLING_ENGINE_SHADER_DIR}/PBRDefault.frag"
		"${FLING_ENGINE_SHADER_DIR}/Deferred/deferred.vert"
		"${FLING_ENGINE_SHADER_DIR}/Deferred/deferred.frag"
		"${FLING_ENGINE_SHADER_DIR}/Deferred/mrt.vert"
		"${FLING_ENGINE_SHADER_DIR}/Deferred/mrt.frag"
		"${FLING_ENGINE_SHADER_DIR}/imgui/ui.vert"
		"${FLING_ENGINE_SHADER_DIR}/imgui/ui.frag"
		"${FLING_ENGINE_SHADER_DIR}/skybox/skybox.vert"
		"${FLING_ENGINE_SHADER_DIR}/skybox/skybox.frag"
)

# Keep prototypes off even when a file includes vulkan.h before FlingVulkan.h.
# The loader is opened at runtime. Do not link Vulkan::Vulkan.
target_compile_definitions(Graphics PRIVATE VK_NO_PROTOTYPES)

set_source_files_properties(src/VulkanFunctions.inl PROPERTIES HEADER_FILE_ONLY TRUE)
