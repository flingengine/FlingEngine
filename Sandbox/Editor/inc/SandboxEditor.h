#pragma once

#include "BaseEditor.h"

namespace Sandbox
{
    class SandboxEditor : public Fling::BaseEditor
    {
    public:
        SandboxEditor() = default;
        ~SandboxEditor() = default;
    
        virtual void Draw(entt::registry& t_Reg, float DeltaTime) override;

    protected:

    };
}   // namespace Sandbox