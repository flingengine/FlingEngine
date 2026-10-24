# Application module: the top-level startup layer. Engine::Run starts every
# system, runs the main loop, and shuts down. It boots a game client, server,
# test, or editor application. Only a game's main() includes it. It must not
# include Editor headers: an editor is passed in as an IDebugUI.

fling_add_module(Application
	PUBLIC_DEPS
		Engine
		Graphics
)
