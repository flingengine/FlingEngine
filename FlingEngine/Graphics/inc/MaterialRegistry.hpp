#pragma once

#include "Singleton.hpp"
#include "Material.h"

namespace Fling
{
	/**
	 * @brief	The material registry is used to keep track of all active materials in
	 *			the engine. 
	 */
	class MaterialRegistry : public Singleton<MaterialRegistry>
	{
	public:

		void Init() override;

		void Shutdown() override;

		void RegisterMaterial(const std::shared_ptr<Material>& t_Mat);

	private:

		/** Array of currently active materials */
		//std::vector<std::shared_ptr<Material>> m_Materials;
	};
}	// namespace Fling