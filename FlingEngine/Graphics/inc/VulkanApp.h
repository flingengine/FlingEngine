#pragma once

#include "FlingTypes.h"
#include "Singleton.hpp"

#include <entt/entity/registry.hpp>
#include <memory>
#include <string>
#include <vector>

namespace Fling
{
    /** Configuration that can determine what render pipelines will be added to this application */
    enum PipelineFlags
    {
        DEFERRED = (1u << 0),
        REFLECTIONS = (1u << 1),
        IMGUI = (1u << 2),
        CUBEMAP = (1u << 3),
        DEBUG = (1u << 4),
        ALL = 0xff
    };

    class Instance;
    class LogicalDevice;
    class PhysicalDevice;
    class Swapchain;
    class FlingWindow;
    class RenderPipeline;
    class CommandBuffer;
    class FirstPersonCamera;
    class DepthBuffer;
    class IDebugUI;

    /**
     * Core rendering functionality of the Fling Engine. Controls what Render pipelines
     * are available
     */
    class VulkanApp : public Singleton<VulkanApp>
    {
    public:
        /**
         * Vulkan objects owned by this app. The complete type is private to the
         * Graphics module, so this header does not include Vulkan.
         */
        struct State;

        void Init(PipelineFlags t_Conf, entt::registry& t_Reg, std::shared_ptr<Fling::IDebugUI> t_DebugUI);
        void Shutdown(entt::registry& t_Reg);

        /**
         * Specify default constructible so that we have more explicit
         * control with Init and Shutdown
         */
        VulkanApp();
        ~VulkanApp();

        /**
         * Updates all rendering buffers and sends commands to draw a frame
         */
        void Update(float DeltaTime, entt::registry& t_Reg);

        inline FlingWindow* GetCurrentWindow() const { return m_CurrentWindow; }
        inline LogicalDevice* GetLogicalDevice() const { return m_LogicalDevice; }
        inline PhysicalDevice* GetPhysicalDevice() const { return m_PhysicalDevice; }
        inline FirstPersonCamera* GetCamera() const { return m_Camera; }

        /** Name reported by the active physical device, or empty when none is selected. */
        std::string GetPhysicalDeviceName() const;

        /** Vulkan state. Only Graphics translation units can see the members. */
        State& GetState();
        const State& GetState() const;

        /** Callback for when a window is resized and to what width and height */
        void OnWindowResized(int Width, int Height);

    protected:
        void Init() override {}
        void Shutdown() override {}

    private:
        /**
         * Prepare logical, physical and swap chain devices.
         * Prepares window based on the Fling Config
         */
        void Prepare();

        /**
         * Create semaphores for available swap chain images and fences
         * for the current frame in flight
         */
        void CreateFrameSyncResources();

        /**
         * Creates a window and preps the surface
         */
        void CreateGameWindow(const uint32 t_width, const uint32 t_height);

        void RecreateFrameResourcesForResize(entt::registry& t_Reg);

        /** Builds any render pipelines with their specific set of sub passes and shaders */
        void
        BuildRenderPipelines(PipelineFlags t_Conf, entt::registry& t_Reg, std::shared_ptr<Fling::IDebugUI> t_DebugUI);

        /**
         * Build the frame buffers for each swap chain image along with the render pass
         * for it to use
         */
        void BuildSwapChainResources();

        void CleanupSwapChainResources();

        void BuildGlobalRenderPass();

        void BuildSwapChainFrameBuffer();

        /** Vulkan Devices that need to get created. @see VulkanApp::Prepare */
        Instance* m_Instance = nullptr;
        LogicalDevice* m_LogicalDevice = nullptr;
        PhysicalDevice* m_PhysicalDevice = nullptr;
        FlingWindow* m_CurrentWindow = nullptr;

        // Swap chain related stuff ---------------------------------------------------------------------
        Swapchain* m_SwapChain = nullptr;
        // Depth buffer acts as a container for our swap chain depth attachment
        DepthBuffer* m_DepthBuffer = nullptr;

        /**
         * Flag that when set to true, means that there is a pending resize of a window
         * so we must recreate the necessary swap chain/frame buffer elements
         */
        uint8 bNeedsResizing : 1;

        /** Keep a vector of command buffers that we want to use so that we can have one for each active frame */
        std::vector<CommandBuffer*> m_DrawCmdBuffers;

        /**
         * The index of the current frame in flight. This is controlled with the in flight fences
         * and is updated every Update of the Vulkan app.
         */
        size_t CurrentFrameIndex = 0;

        std::vector<RenderPipeline*> m_RenderPipelines;

        /** The Vulkan app will specify the current camera and be limited to one for now */
        FirstPersonCamera* m_Camera = nullptr;

        std::unique_ptr<State> m_State;
    };
} // namespace Fling
