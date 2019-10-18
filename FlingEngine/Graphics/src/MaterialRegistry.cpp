#include "pch.h"
#include "MaterialRegistry.hpp"
#include <algorithm>

namespace Fling
{
	void MaterialRegistry::Init()
	{

	}

	void MaterialRegistry::Shutdown()
	{
		m_Materials.clear();
	}

	void MaterialRegistry::RegisterMaterial(const std::shared_ptr<Material>& t_Mat)
	{
		const auto& it = std::find(std::begin(m_Materials), std::end(m_Materials), t_Mat);
		// If this material isn't loaded already then add it to the registry
		if (it == std::end(m_Materials))
		{
			m_Materials.emplace_back(t_Mat);
		}
	}
}   // namespace Fling