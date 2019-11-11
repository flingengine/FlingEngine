#pragma once

#include "Platform.h"
#include "Singleton.hpp"
#include "ScriptComponent.h"

namespace Fling
{
	class LuaManager : public Singleton<LuaManager>
	{
	public:
		virtual void Init() override;

		virtual void Shutdown() override;
	private:

	};
}