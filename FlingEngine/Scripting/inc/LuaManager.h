#pragma once

#include "Singleton.hpp"
#include "LuaScript.h"

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