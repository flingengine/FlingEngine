#pragma once

#include "CoreAPI.h"
#include "FlingTypes.h"
#include "Strings/String.h"

#include <functional>
#include <vector>

namespace Fling
{
    /** How one screenshot went (Screenshot::Request). */
    struct ScreenshotResult
    {
        /** The file that was asked for, resolved to an absolute path. */
        String Path;

        /** True when the PNG file was written. */
        bool Ok = false;

        /** Why it failed, when Ok is false. */
        String Error;

        /** The size of the frame in pixels. 0 when it failed. */
        uint32 Width = 0;
        uint32 Height = 0;
    };

    /** Called once for each request, on the main thread, after the file is written or the request fails. */
    using ScreenshotCallback = std::function<void(const ScreenshotResult&)>;

    /**
     * Save the next frame that the renderer draws as a PNG file, from anywhere in the engine:
     * the editor, a frame system, a tool, a test. Request is safe on any thread, and needs no
     * Graphics header: the renderer (VulkanApp) takes the requests at its next frame. It
     * works with a window (the image then has the editor UI in it) and with --offscreen.
     * Under --null-render there is no renderer, so each request fails at the next frame.
     *
     * It costs nothing when nobody asks: a frame with no request does one atomic load. A
     * request does not stall the frame either: the GPU copies the frame in its own command
     * buffer, and a writer thread encodes and writes the PNG. The callback arrives a few
     * frames later.
     *
     * A frame shows the simulation state that the frame side has, not a tick on the game
     * thread. To save the frame of a given tick, use --screenshot-ticks, which calls this
     * when the frame side first shows that tick.
     *
     *     Fling::Screenshot::Request("Saved/door_open.png",
     *         [](const Fling::ScreenshotResult& t_Result) { F_LOG_TRACE("Saved: {}", t_Result.Ok); });
     *
     * Do not call it from simulation code to change the simulation: the result arrives on
     * the main thread, out of step with the ticks.
     */
    class CORE_API Screenshot
    {
    public:
        /** One request waiting for a frame. The renderer side reads it. */
        struct Pending
        {
            String Path;
            ScreenshotCallback OnDone;
        };

        /**
         * Ask for the next frame to be saved. Safe on any thread.
         *
         * @param t_Path    The PNG file. Relative paths are relative to the project folder.
         *                  Missing folders are made. Empty: DefaultPath()
         * @param t_OnDone  Optional. Called on the main thread when the file is written or the
         *                  request fails. A result is also logged either way
         * @return The resolved path the file will be written to
         */
        static String Request(StringView t_Path = {}, ScreenshotCallback t_OnDone = {});

        /** A new file name under Saved/Screenshots, named with the local time, for example
         * Screenshot_2026-11-16_14-03-22_1.png. */
        static String DefaultPath();

        /**
         * True when a request is waiting. One atomic load and no lock, so the renderer can ask
         * every frame for free. Any thread.
         */
        static bool HasPending();

        /**
         * Renderer side (main thread): the requests since the last call, in request order.
         * With none waiting it takes no lock and allocates nothing.
         */
        static std::vector<Pending> TakePending();

        /** Renderer side (main thread): log the result and call the request's callback. */
        static void Finish(Pending& t_Request, const ScreenshotResult& t_Result);

        /** Fail every waiting request with t_Reason (--null-render, or shutdown). Main thread. */
        static void FailPending(StringView t_Reason);
    };
} // namespace Fling
