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

        virtual void Draw(Fling::EntityStorage& t_Reg, float DeltaTime) override;

    protected:
        void OnLoadLevel(Fling::String t_FileName) override;

        void OnSaveLevel(Fling::String t_FileName) override;
    };
} // namespace Sandbox