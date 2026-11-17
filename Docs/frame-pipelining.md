# Frame pipelining: the CPU records a frame while the GPU draws the one before

The renderer keeps two frames in flight (`VkConfig::MAX_FRAMES_IN_FLIGHT`, `FrameSync::FramesInFlight`).
The main thread records frame N while the GPU still draws frame N-1. Before this, each frame waited for
its own fence right after the submit, so the CPU and the GPU took turns: the frame time was the CPU time
plus the GPU time. Now it is the larger of the two.

Measured on the lighting test level (RelWithDebInfo, 1080p window, RTX 3090, VSync off, no frame cap,
30 Hz sim, 900 ticks, three runs each):

| | Before | After |
|---|---|---|
| Spheres turning | 2.70 to 2.73 ms (366 to 370 fps) | 1.85 to 1.87 ms (536 to 541 fps) |
| Spheres still | 2.76 to 2.77 ms (361 to 363 fps) | 1.82 to 1.88 ms (533 to 549 fps) |

The frame is now GPU bound. `Vk.Acquire` (the render thread waits for an image) is the largest scope.

## The parts

1. **The fence moved to the start of the frame.** `VulkanApp::Update` waits for the fence of the frame
   slot (`frame number % FramesInFlight`) before it records. That frame was submitted two frames ago, so it
   is done. The frame before it may still run on the GPU. `FrameSync::Begin` then tells the passes which
   frames are finished.

2. **The present thread** (`PresentThread`, optional, see below). On some drivers (NVIDIA on Wayland) `vkQueuePresentKHR`
   does not return until the GPU has finished the frame. Called on the render thread, that brings the wait
   back. So a thread of its own makes the present call, on a second queue of the graphics family
   (`LogicalDevice::HasSeparatePresentQueue`). A swap chain must not be used by two threads at once, so the
   same thread also acquires the images, one after each present, and the render thread takes them
   (`TakeAcquired`). Without a second queue, or offscreen, the render thread presents and acquires itself:
   the picture is the same, and there is no overlap.

3. **Data for the GPU goes through `FrameUploader`.** A buffer that the CPU wrote each frame and the GPU
   read (the camera, the world matrices, the draw commands, the occlusion slot data and parameters) was a
   host visible buffer that the CPU overwrote. With two frames in flight that races with the GPU. Now the
   CPU writes into a staging buffer of its own frame slot (`Stage`), and the frame's first command buffer
   (`FrameUploader::Record`) copies it into a buffer in GPU memory. Only the GPU writes those buffers, in
   the order of the submissions. They are also faster to read than host memory.

4. **The GPU runs one frame at a time.** The frames share the G-buffer images, the depth buffers, the depth
   pyramid and the visibility flags. The first submission of a frame waits for the last one of the frame
   before (`VulkanApp::State::m_FrameChain`), and the upload command buffer starts with a full memory
   barrier. The CPU records ahead; the GPU frames do not overlap each other.

## The present thread is optional

`[Graphics] PresentThread` in `Config/EngineConf.ini` (default `true`) turns the present thread on or off. The
engine itself has two threads, the game thread and the main thread, and the present thread is an extra one for a
driver that blocks. Off (or when the device has no second queue), the main thread does the swap chain calls
itself and gets the same overlap another way: it presents a frame after it has submitted the next one, so the GPU
has that frame queued while the present call waits. The cost is that a frame is shown one frame later than it could
be. When the frame limiter is going to sleep anyway (`FrameLimiter::WillWait`), the engine presents the held frame
first (`VulkanApp::PresentPendingFrame`), so a capped run does not pay it. A driver whose present call does not
block gains nothing from the thread, and can switch it off.

Same scene (uncapped, VSync off): about 560 fps with the thread, about 460 fps without it, 368 fps before the
pipelining.

## Rules for a pass

- A buffer that the CPU writes while recording and that a command buffer reads: put the data through
  `FrameUploader::Stage`, or keep one copy for each frame slot (`FrameSync::Slot()`): the ImGui pass and the
  debug pass do this. A swap chain image index is not a frame slot, but the buffers that are indexed by
  image (the lighting UBOs) are safe too: two frames in a row never get the same image.
- Free a resource that a frame used (a descriptor set, a buffer) with `RetireList`: `Add` tags it with the
  current frame, `Release` frees it when `FrameSync::IsDone` says the frames that could use it are finished.
- Writing a descriptor set that a frame in flight uses is not allowed. The few places that must do it (a
  larger world matrix buffer, new occlusion buffers) call `LogicalDevice::WaitForIdle()` first. This is
  rare: the buffers grow by doubling.
- Before the engine frees a model or a texture that a frame may use, call `VulkanApp::WaitForGpuIdle()`
  (`Engine` does this at shutdown).
- A screenshot copy is recorded in one frame, and the capturer takes it when that frame is done
  (`VulkanApp::State::m_CaptureFrame`). One copy is in flight at a time.

## Checking it

- `--profile`: `Vk.WaitForGpu` (the fence at the start of the frame), `Vk.Acquire`, `Vk.Submit`,
  `Vk.QueuePresent` (handing the present over), `Present.Thread` and `Present.Acquire` (on the present thread),
  `Upload.Record`.
- The validation layers (`EnableValidationLayers=true` in `Config/EngineConf.ini`) report nothing in a run,
  also when the swap chain is made again every 40 frames.
- The synchronization validation (`VK_LAYER_ENABLES=VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT`)
  reports `SYNC-HAZARD-*-RACING-*` between the submissions of two frames. The frames are ordered by a
  semaphore chain and by the barrier at the start of the upload, and the layer does not follow that chain
  between batches of one queue. It reports nothing between frames when each submit is followed by a fence wait.
  It also reports the depth attachment of the final render pass (`WRITE_AFTER_WRITE vs. layout transition`),
  which has nothing to do with the pipelining.
- With `--exec threaded` and `--frames-per-tick N`, the frame that a `--screenshot-ticks` capture shows can
  differ from run to run when frames are fast: the game thread and the frames are not locked step by step.
  Compare pictures of runs with the same mode in one build, or use `--frustum-culling off`.

## Not done

- The CPU still waits for an image in `Vk.Acquire` when the GPU is the slower side. That is the limit now:
  reducing the GPU time (it is about 1.8 ms for the lighting test level) is the next gain.
- Three frames in flight, and overlapping two GPU frames, need a copy of the G-buffer images and the depth
  pyramid for each frame slot.
