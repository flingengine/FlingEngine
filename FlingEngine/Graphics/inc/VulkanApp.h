#pragma once

#include "Ecs/EntityStorage.h"
#include "FlingTypes.h"
#include "MeshCuller.h"
#include "PresentMode.h"
#include "Singleton.hpp"
#include "Strings/String.h"

#include <memory>
#include <utility>
#include <vector>

namespace Fling
{
    /** Configuration that can determine what render pipelines will be added to this application */
    enum PipelineFlags
    {
        DEFERRED = (1u << 0),
        REFLECTIONS = (1u << 1),
        IMGUI = (1u << 2),
        /** Draw a skybox behind the DEFERRED pipeline. The cube map folder is the [Graphics] Skybox config value. */
        CUBEMAP = (1u << 3),
        DEBUG = (1u << 4),
        ALL = 0xff
    };

    /**
     * What the renderer draws to. Set it with VulkanApp::SetRenderSurface before Init.
     */
    struct RenderSurfaceDesc
    {
        /**
         * True: draw into images that the renderer owns (--offscreen). There is no window, no
         * surface, and no window system: a run works on a machine with no display, and opens
         * nothing on a machine that has one. Screenshots (Screenshot::Request) still work.
         */
        bool Offscreen = false;

        /** The size of the frames (offscreen) or the window. 0: [Engine] WindowWidth and WindowHeight. */
        uint32 Width = 0;
        uint32 Height = 0;
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
    class OffscreenSubpass;
    class FrameCapturer;

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

        /**
         * Make the window, the device, and the render pipelines. t_Reg is the world that
         * the editor UI (IDebugUI) changes. The render passes never read it.
         */
        void Init(PipelineFlags t_Conf, Fling::EntityStorage& t_Reg, std::shared_ptr<Fling::IDebugUI> t_DebugUI);

        /** Wait for the GPU, then free the render pipelines, the device, and the window. */
        void ShutdownRenderer();

        /**
         * Wait for the screenshots that are being written (Screenshot::Request) and call their
         * callbacks. A frame copies a screenshot; a writer thread saves it later, so a run that
         * counts its screenshots calls this before it ends. Main thread.
         */
        void FinishScreenshots();

        /**
         * Specify default constructible so that we have more explicit
         * control with Init and Shutdown
         */
        VulkanApp();
        ~VulkanApp();

        /**
         * Draw a frame from the RenderSnapshot in t_Reg (Graphics.PublishSnapshot writes it).
         */
        void Update(float DeltaTime, const Fling::EntityStorage& t_Reg);

        inline FlingWindow* GetCurrentWindow() const { return m_CurrentWindow; }
        inline LogicalDevice* GetLogicalDevice() const { return m_LogicalDevice; }
        inline PhysicalDevice* GetPhysicalDevice() const { return m_PhysicalDevice; }
        inline FirstPersonCamera* GetCamera() const { return m_Camera; }

        /**
         * True after Init made a GPU device. False under --null-render, where Init is
         * not called: resources then keep their CPU data and make no GPU objects.
         */
        inline bool HasDevice() const { return m_LogicalDevice != nullptr; }

        /** Name reported by the active physical device, or empty when none is selected. */
        String GetPhysicalDeviceName() const;

        /** Vulkan state. Only Graphics translation units can see the members. */
        State& GetState();
        const State& GetState() const;

        /** Callback for when a window is resized and to what width and height */
        void OnWindowResized(int Width, int Height);

        /**
         * VSync on or off (RunSettings::VSync; ChoosePresentMode picks the present mode from it
         * and the modes the surface offers). Before Init it is the setting the swap chain is
         * made with; after Init a change makes the swap chain again on the path a resize takes,
         * after the next present. The same value changes nothing.
         */
        void SetVSync(bool t_VSync);

        /** The present mode of the swap chain. */
        PresentMode GetPresentMode() const;

        /**
         * Frustum culling on or off (RunSettings::FrustumCulling) for the next frame. On: the
         * meshes outside the camera's view are not drawn (MeshCuller). The mesh pass holds the
         * value, so it does nothing before Init; Engine::Present calls it before every frame.
         */
        void SetFrustumCulling(bool t_On);

        /**
         * Occlusion culling on or off (RunSettings::OcclusionCulling) for the next frame. On: the
         * meshes that other meshes hide from the camera are not drawn. The mesh pass holds the
         * value, so it does nothing before Init; Engine::Present calls it before every frame.
         */
        void SetOcclusionCulling(bool t_On);

        /** The meshes the last frame drew and culled. Zero before the first frame and under --null-render. */
        CullStats GetCullStats() const;

        /** What to draw to: a window or offscreen images, and the size. Call it before Init. */
        void SetRenderSurface(const RenderSurfaceDesc& t_Desc) { m_SurfaceDesc = t_Desc; }

        /** True when the renderer draws offscreen (RenderSurfaceDesc::Offscreen): there is no window. */
        bool IsOffscreen() const { return m_SurfaceDesc.Offscreen; }

        /** The size of the frames the renderer draws, in pixels. 0 x 0 before Init. */
        uint32 GetFrameWidth() const;
        uint32 GetFrameHeight() const;

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

        /** Make the swap chain and what depends on it again (a resize, an out-of-date swap chain, a present mode
         * change). */
        void RecreateSwapchain();

        /** Builds any render pipelines with their specific set of sub passes and shaders */
        void BuildRenderPipelines(PipelineFlags t_Conf,
                                  Fling::EntityStorage& t_Reg,
                                  std::shared_ptr<Fling::IDebugUI> t_DebugUI);

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
         * True when the swap chain must be made again after this frame's present (a resize, a
         * present mode change): RecreateSwapchain.
         */
        bool bSwapchainOutOfDate = false;

        /** @see SetVSync */
        bool m_VSync = true;

        /** The pass that draws the meshes, which culls them. Owned by m_RenderPipelines. */
        OffscreenSubpass* m_MeshPass = nullptr;

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

        /** @see SetRenderSurface */
        RenderSurfaceDesc m_SurfaceDesc;

        /** Serves Screenshot::Request: the copy in the frame, and the PNG on a writer thread. */
        std::unique_ptr<FrameCapturer> m_Capturer;
    };
} // namespace Fling
