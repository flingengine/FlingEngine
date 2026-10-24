# Engine modules, added bottom-up so each module's dependencies exist first.
# See Plans/module-split-plan.md for the module graph.
add_subdirectory( Core )
add_subdirectory( Resources )
add_subdirectory( Engine )
add_subdirectory( Physics )
add_subdirectory( Graphics )
add_subdirectory( Application )

# Editor is a leaf. Only editor applications link it.
if( WITH_EDITOR_FLAG )
    add_subdirectory( Editor )
endif()

# What a game links: everything except the editor
add_library( FlingGameStack INTERFACE )
add_library( Fling::GameStack ALIAS FlingGameStack )
target_link_libraries( FlingGameStack INTERFACE Fling::Application )

# What an editor links: the game stack plus the editor
if( WITH_EDITOR_FLAG )
    add_library( FlingEditorStack INTERFACE )
    add_library( Fling::EditorStack ALIAS FlingEditorStack )
    target_link_libraries( FlingEditorStack INTERFACE Fling::GameStack Fling::Editor )
endif()
