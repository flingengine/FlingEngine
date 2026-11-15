#pragma once

#include "BaseEditor.h"
#include "Strings/String.h"

namespace Sandbox
{
    class SandboxEditor : public Fling::BaseEditor
    {
    public:
        SandboxEditor() = default;
        ~SandboxEditor() = default;

        /** As BaseEditor::Init. In --play the GPU Options window starts open: it is the demo's control panel. */
        void Init(Fling::EntityStorage& t_Reg,
                  const std::shared_ptr<Fling::World>& t_World,
                  const std::shared_ptr<Fling::Game>& t_Game,
                  Fling::RunControls& t_Controls) override;

        virtual void Draw(Fling::EntityStorage& t_Reg, float DeltaTime) override;

    protected:
        void OnLoadLevel(Fling::String t_FileName) override;

        void OnSaveLevel(Fling::String t_FileName) override;

    private:
        /** The "Camera: ..." line of the Demo Info window, from the CameraStatus that the frame side made. */
        void DrawCameraStatus();
    };
} // namespace Sandbox