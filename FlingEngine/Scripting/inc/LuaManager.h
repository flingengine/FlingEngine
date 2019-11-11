#pragma once

#include "pch.h"
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