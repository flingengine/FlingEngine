#pragma once

#include "FlingEditorPanel.h"

#include "Ecs/EntityStorage.h"

namespace Fling
{
    class World;

    /**
     * Debug window for the world's singleton components and its system schedule. It
     * lists each registered Singleton and Queue type, whether the world has it, and
     * the value it would save to a level. Singletons are not on entities, so the
     * entity editor does not show them. A second section shows the schedule dump
     * (the same text as --dump-schedule). The window is read only.
     */
    class WorldSingletonsPanel final : public FlingEditorPanel
    {
    public:
        WorldSingletonsPanel() { m_ShowWindow = false; }

        /** The world whose schedule to show. Null shows no schedule. */
        void SetWorld(const World* t_World) { m_World = t_World; }

        /** Draws the "World Singletons" window. Call this once per frame. */
        void Draw(Fling::EntityStorage& t_Reg) override;

        /** The flag that shows or hides the window, for a menu checkbox. */
        bool& ShowWindowFlag() { return m_ShowWindow; }

    private:
        const World* m_World = nullptr;
    };
} // namespace Fling
