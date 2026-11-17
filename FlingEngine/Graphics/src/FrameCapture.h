#pragma once

#include "FlingTypes.h"
#include "FlingVulkan.h"
#include "Screenshot.h"
#include "Strings/String.h"
#include "Threading/ThreadQueue.h"

#include <atomic>
#include <memory>
#include <vector>

namespace Fling
{
    class Buffer;
    class CommandRunnable;
    class Swapchain;
    class Thread;

    namespace FrameCapture
    {
        /**
         * Turn the pixels of a copied frame into RGBA8 in place (4 bytes a pixel, rows top to
         * bottom): BGRA formats are swizzled, and alpha is written as opaque (the frame's clear
         * alpha is not meant to be seen). False if t_Format is not 8-bit RGBA or BGRA.
         */
        bool ToRgba(VkFormat t_Format, uint8* t_Pixels, size_t t_Size);

        /**
         * Encode RGBA8 pixels (4 bytes a pixel, rows top to bottom) as PNG bytes. Pure CPU
         * work: the tests call it with no GPU. False if the encoder fails.
         */
        bool EncodePng(uint32 t_Width, uint32 t_Height, const uint8* t_Rgba, String& t_OutPng);

        /** EncodePng, then FlingPaths::SaveFile. */
        bool WritePng(StringView t_Path, uint32 t_Width, uint32 t_Height, const uint8* t_Rgba, String& t_OutError);
    } // namespace FrameCapture

    /**
     * Serves Screenshot::Request for VulkanApp without stalling the frame:
     *
     *  1. RecordCopy (main thread, in the frame): when a request waits, the frame's own command
     *     buffer copies the finished image into a staging buffer, after the render pass. That
     *     is GPU work in the same submit; the CPU does not wait for it.
     *  2. OnFrameDone (main thread, after the frame's fence): the copy is done, so the staging
     *     buffer goes to the writer thread. Nothing is copied on the main thread.
     *  3. The writer thread converts the pixels, encodes the PNG once, writes each file, and
     *     frees the staging buffer for the next capture.
     *  4. DeliverResults (main thread, next frames): the requests' callbacks.
     *
     * A frame with no request costs one atomic load in RecordCopy and one in DeliverResults.
     * The writer thread and the staging buffers are made on the first capture only.
     */
    class FrameCapturer
    {
    public:
        FrameCapturer();
        ~FrameCapturer();

        FrameCapturer(const FrameCapturer&) = delete;
        FrameCapturer& operator=(const FrameCapturer&) = delete;

        /**
         * Record the copy of t_SwapChain's image t_ImageIndex into t_Cmd, outside of a render
         * pass, after everything that draws to it. Takes the waiting requests. Main thread.
         */
        void RecordCopy(VkCommandBuffer t_Cmd, const Swapchain& t_SwapChain, uint32 t_ImageIndex);

        /** The fence of the frame that RecordCopy recorded into has signaled: start the writes. Main thread. */
        void OnFrameDone();

        /** True if RecordCopy recorded a copy whose frame OnFrameDone has not seen yet */
        bool HasRecorded() const { return m_Recorded != nullptr; }

        /** Call the callbacks of the captures that the writer finished. Main thread. */
        void DeliverResults();

        /**
         * Wait for the writer to finish every capture, then DeliverResults: the end of a run
         * (its exit code counts the screenshots), and before the device is destroyed. Main thread.
         */
        void Flush();

        /** Flush, then free the staging buffers. Before the logical device is destroyed. */
        void Release();

    private:
        /** One staging buffer. The writer owns it while InUse. */
        struct Slot
        {
            std::unique_ptr<Buffer> Staging;
            void* Mapped = nullptr;
            std::atomic<bool> InUse{false};
        };

        /** A copy that RecordCopy recorded, waiting for its frame's fence. */
        struct Recorded
        {
            Slot* Target = nullptr;
            VkExtent2D Extent = {};
            VkFormat Format = VK_FORMAT_UNDEFINED;
            std::vector<Screenshot::Pending> Requests;
        };

        /** A finished request, for the main thread. */
        struct Finished
        {
            Screenshot::Pending Request;
            ScreenshotResult Result;
        };

        /** A free slot that holds t_Size bytes, or null when all are with the writer and there are MaxSlots. */
        Slot* AcquireSlot(VkDeviceSize t_Size);

        /** The writer thread's work for one capture. */
        void Write(Recorded& t_Capture);

        /** Fail t_Requests with t_Error now (main thread). */
        void Fail(std::vector<Screenshot::Pending>& t_Requests, StringView t_Error);

        /** At most this many captures in flight; more requests wait for a frame with a free slot. */
        static constexpr size_t MaxSlots = 3;

        std::vector<std::unique_ptr<Slot>> m_Slots;

        /** Requests taken from Screenshot that wait for a free slot. */
        std::vector<Screenshot::Pending> m_Waiting;

        /** The copy recorded into this frame, if any. */
        std::shared_ptr<Recorded> m_Recorded;

        std::shared_ptr<CommandRunnable> m_Writer;
        std::shared_ptr<Thread> m_WriterThread;

        /** Writer thread to main thread. */
        ThreadQueue<Finished> m_Finished;

        /** Items in m_Finished: one atomic load answers "nothing to deliver". */
        std::atomic<int64> m_FinishedCount{0};
    };
} // namespace Fling
