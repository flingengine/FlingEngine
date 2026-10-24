# Editor module: BaseEditor and its panels. A leaf: only editor applications
# link it, and no other module may include its headers. It plugs into the
# renderer as an IDebugUI (see Graphics/inc/IDebugUI.h).

fling_add_module(Editor
	PUBLIC_DEPS
		Engine
		Graphics
	PUBLIC_LIBS
		ImGui	# BaseEditor.h includes imgui.h
)
