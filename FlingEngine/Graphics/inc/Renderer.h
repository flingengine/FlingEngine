
#pragma once

// Resolve warnings
#include "Platform.h"

#include "FlingVulkan.h"

#include <GLFW/glfw3.h>
#include <imgui.h>

#include "Singleton.hpp"

#include "Vertex.h"
#include "UniformBufferObject.h"
#include "FirstPersonCamera.h"

#include "FlingWindow.h"
#include "PhyscialDevice.h"
#include "Instance.h"
#include "LogicalDevice.h"
#include "Buffer.h"
#include "SwapChain.h"
#include "DepthBuffer.h"
#include "Model.h"

#include <entt/entity/registry.hpp>
#include "MeshRenderer.h"
#include "Material.h"

#include "ShaderProgram.h"
#include "Shader.h"
#include "FlingImgui.h"
#include "ImguiDisplay.h"
#include <atomic>
#include "Cubemap.h"
#include "MultiSampler.h"

#include "Lighting/DirectionalLight.hpp"
#include "Lighting/PointLight.hpp"
#include "Lighting/Lighting.hpp"

namespace Fling
{
    // File resource
    class File;

    // Imgui resource
    class FlingImgui;

    /// <summary>
    /// Core renderer for the application
    /// </summary>
    class Renderer : public Singleton<Renderer>
    {
        friend class Engine;
    public:

        virtual void Init() override;

        virtual void Shutdown() override;   

        /// <summary>
        /// Init GLFW and create the game window
        /// </summary>
        /// <param name="t_width">Width of the window</param>
        /// <param name="t_height">Height of the window</param>
        void CreateGameWindow(const UINT32 t_width, const UINT32 t_height);

        FlingWindow* GetCurrentWindow() const { return m_CurrentWindow; }

        /** Happens before draw frame. Update the window  */
        void Tick(float DeltaTime);

        /**
        * Draw the frame!
        */
        void DrawFrame(entt::registry& t_Reg);

        /**
        * Prepare for shutdown of the rendering pipeline, close any open semaphores
        */
        void PrepShutdown();

        /**
         * @brief Get the logical graphics Device object
         *
         * @return const ref to VkDevice
         */
        static const VkDevice& GetLogicalVkDevice()  { return Renderer::Get().m_LogicalDevice->GetVkDevice(); }

		static VkSampleCountFlagBits GetMsaaSampleCount() 
		{ 
			if (Renderer::Get().m_MsaaSampler)
			{
				return Renderer::Get().m_MsaaSampler->GetSampleCountFlagBits();
			}
			else
			{
				return VK_SAMPLE_COUNT_1_BIT;
			}
		}

        LogicalDevice* GetLogicalDevice() const { return m_LogicalDevice; }

        /**
         * @brief Get the Physical Device object used by this renderer
         *
         * @return const VkPhysicalDevice&
         */
        const VkPhysicalDevice& GetPhysicalVkDevice() const { return m_PhysicalDevice->GetVkPhysicalDevice(); }

        PhysicalDevice* GetPhysicalDevice() const { return m_PhysicalDevice; }

        const VkCommandPool& GetCommandPool() const { return m_CommandPool; }

        const VkQueue& GetGraphicsQueue() const { return m_LogicalDevice->GetGraphicsQueue(); }

        void SetFrameBufferHasBeenResized(bool t_Setting) { m_FrameBufferResized = t_Setting; }

        const VkSurfaceKHR& GetVkSurface() const { return m_Surface; }

        Swapchain* GetSwapChain() const { return m_SwapChain; }

    private:

        void InitDevices();

        /** Init the actual Vulkan API and rendering pipeline */
        void InitGraphics();
        
        /// Init imgui context 
        void InitImgui();
        void UpdateImguiIO();

        /**
        * @brief Set any component type callbacks needed for the rendering pipeline
        */
        void InitComponentData();

        /**
         * @brief Create a Descriptor Layout object
         * @see UniformBufferObject.h
         */
        void CreateDescriptorLayout();

        /**
        * Create the graphics pipeline (IA, VS, FS, etc)
        */
        void CreateGraphicsPipeline();

        /**
        * Create the frame buffer that will be used by the graphics pipeline
        */
        void CreateRenderPass();

        /**
        * Create the frame buffers for use by the swap chain
        */
        void CreateFrameBuffers();
        
        /*
        * Builds command buffer to submit to device
        */
        void BuildCommandBuffers(entt::registry& t_Reg);

        /**
        * Create semaphores and fence objects
        */
        void CreateSyncObjects();

        void CleanupFrameResources();

        /**
        * Re-create the image views, render passes, and command buffers
        */
        void RecreateFrameResources();

        void CreateDescriptorPool();

        void CreateDescriptorSets();

        /**
        * Determine the best match extents based on our window width and height
        *
        * @return   Extents with the best matching resolution
        */
        VkExtent2D ChooseSwapExtent();

        /**
         * @brief Update the uniform buffer data. Called during DrawFrame
         *
         * @param t_CurrentImage The current image index that we are using
         */
        void UpdateUniformBuffer(UINT32 t_CurrentImage);

        /**
        * @brief    Callback for when a mesh renderer component is added to the game
        *            Initializes and loads any meshes that we may need
        */
        void MeshRendererAdded(entt::entity t_Ent, entt::registry& t_Reg, MeshRenderer& t_MeshRend);

        /**
         * @brief   Callback for when a directional light is added to Fling so that we can keep track of how many
         *          we need
         */
        void DirLightAdded(entt::entity t_Ent, entt::registry& t_Reg, DirectionalLight& t_Light);

		void PointLightAdded(entt::entity t_Ent, entt::registry& t_Reg, PointLight& t_Light);


        /** Entt registry that the renderer will be using. Set by the Engine */
        entt::registry* m_Registry = nullptr;

        /** Camera Instance */
        FirstPersonCamera* m_camera;

        /** Skybox Instance */
        Cubemap* m_Skybox = nullptr;

        /** The shader program that will allow the users to define their graphics pipeline */
        ShaderProgram* m_ShaderProgram = nullptr;

        FlingWindow* m_CurrentWindow = nullptr;

        /** Imgui Instance **/
        FlingImgui* m_flingImgui = nullptr;
        
        /** Holds imgui ui data **/
        ImguiDisplay m_imguiDisplay;

        Instance* m_Instance = nullptr;

        LogicalDevice* m_LogicalDevice = nullptr;

        PhysicalDevice* m_PhysicalDevice = nullptr;

        /** Handle to the surface extension used to interact with the windows system */
        VkSurfaceKHR m_Surface = VK_NULL_HANDLE;

        Swapchain* m_SwapChain = nullptr;

        VkRenderPass m_RenderPass;

        /** Pipeline layout stores uniforms (global shader vars) */
        VkDescriptorSetLayout m_DescriptorSetLayout;
        VkPipelineLayout m_PipelineLayout;

        VkPipeline m_GraphicsPipeline;

        /** @see Renderer::CreateCommandPool */
        VkCommandPool m_CommandPool;

        /** @see CreateDescriptorPool */
        VkDescriptorPool m_DescriptorPool;

        DepthBuffer* m_DepthBuffer = nullptr;

        /** MSAA for the graphics pipeline */
        Multisampler* m_MsaaSampler = nullptr;

        size_t CurrentFrameIndex = 0;

        /** Used to determine if the frame buffer has been resized or not */
        bool m_FrameBufferResized = false;

        static const int MAX_FRAMES_IN_FLIGHT;

        /** The alignment of the dynamic UBO on this device */
        size_t m_DynamicAlignment;

        std::vector<VkDescriptorSet> m_DescriptorSets;

        /**
        * The frame buffers for the swap chain
        * @see Renderer::CreateFrameBuffers
        */
        std::vector<VkFramebuffer> m_SwapChainFramebuffers;

        /**
        * Command buffers
        * @see m_CommandPool
        */
        std::vector<VkCommandBuffer> m_CommandBuffers;

        std::vector<VkSemaphore> m_ImageAvailableSemaphores;
        std::vector<VkSemaphore> m_RenderFinishedSemaphores;
        std::vector<VkFence> m_InFlightFences;

		std::shared_ptr<Material> m_DefaultMat;

        // Lighting -----------------------
        Lighting m_Lighting = {};
		std::shared_ptr<Image> m_BRDFLookupTexture;
   

        LightingUbo m_LightingUBO = {}; 

        void CreateLightBuffers();

        // Flag for toggling imgui 
        bool m_DrawImgui;
		bool m_IsQuitting = false;
    };
}    // namespace Fling