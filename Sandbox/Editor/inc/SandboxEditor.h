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

		void OnLoadLevel(std::string t_FileName) override;

		void OnSaveLevel(std::string t_FileName) override;

    };
}   // namespace Sandbox