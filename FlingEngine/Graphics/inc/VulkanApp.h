#pragma once

#include "FlingTypes.h"
#include "FlingVulkan.h"
#include "RenderPipeline.h"

#include <entt/entity/registry.hpp>
#include <vector>

namespace Fling
{
	/** Configuration that can determine what render pipelines will be added to this application */
	enum PipelineFlags
	{
		DEFERRED = 0x01,
		REFLECTIONS = 0x02,
		IMGUI = 0x04,	
		ALL = 0xff
	};

	/**
	* @brief	Core rendering functionality of the Fling Engine. Controls what Render pipelines 
	*			are available
	*/
    class VulkanApp
    {
    public:
        VulkanApp() = default;
        ~VulkanApp() = default;

		/**
		 * @brief	Prepares 
		 */
		void Prepare(PipelineFlags t_Conf);

		void Update(float DeltaTime, entt::registry& t_Reg);

		void Shutdown();

    private:


		void BuildRenderPipelines(PipelineFlags t_Conf);

		// Swap chain

		// Window
		
		// Command Buffer pool
		
		// Logical Device
		// Physical Device

		std::vector<std::unique_ptr<Fling::RenderPipeline>> m_RenderPipelines;

		// VMA Allocator

    };
}   // namespace Fling