# Resources module: config files, JSON, file loading, and the resource manager.
# Depends on Core only. GPU resources (Texture, HDRImage) live in Graphics.

fling_add_module(Resources
	PUBLIC_DEPS
		Core
	PUBLIC_LIBS
		FlingExt::inih	# FlingConfig.h exposes INIReader
	PRIVATE_LIBS
		FlingExt::json	# Json.h hides nlohmann behind a pimpl
)
