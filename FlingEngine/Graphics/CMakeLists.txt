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

# Load vulkan-1.dll on the first Vulkan call, not when the process starts. A
# --null-render run makes no Vulkan call, so it can start on a machine that has
# no Vulkan runtime (CI runners, servers). MinGW has no /DELAYLOAD. This is a
# short-term fix: Plans/vulkan-loading.md replaces it on every platform.
if(MSVC)
	# LNK4199: a target that links Graphics but makes no Vulkan call (ApplicationTests)
	target_link_options(Graphics INTERFACE /DELAYLOAD:vulkan-1.dll /IGNORE:4199)
	target_link_libraries(Graphics INTERFACE delayimp)
endif()
