# Occlusion culling

The G-buffer pass does not draw a mesh that other meshes hide. It runs on the GPU, after the
frustum test ([`MeshCuller`](../FlingEngine/Graphics/inc/MeshCuller.h)), and it only removes
draws, so the picture is the same. Issue #252.

## Settings

| How | What |
| --- | --- |
| `--occlusion-culling on|off` | Run setting. On by default |
| `[Engine] OcclusionCulling` | The same, in `Config/EngineConf.ini` |
| Editor, GPU Options | A checkbox, and the count of hidden meshes (the value of the previous frame) |

The pass turns itself off when the compute shaders did not load, or the device cannot sample the
depth format or store `R32_SFLOAT` images. It works on a software driver (llvmpipe).

## How it works

Two phases in one command buffer (`OcclusionCuller`, `Assets/Shaders/Occlusion/`):

1. **Phase 0** (compute). Each slot that is in the view and was visible in the last frame gets an
   indirect draw (`Draw1`).
2. The **first render pass** draws `Draw1` and clears the images as before.
3. A **depth pyramid** (`R32_SFLOAT`, each texel is the farthest depth below it) is built from
   that depth (`hiz_downsample.comp`).
4. **Phase 1** (compute). Every slot in the view is tested against the pyramid. A slot that is not
   hidden and was not drawn in step 2 gets a draw in `Draw2`. The slot's visible flag is saved for
   the next frame, and the hidden slots are counted.
5. The **second render pass** (`FrameBuffer::GetLoadRenderPassHandle`, load ops, same frame buffer)
   draws `Draw2`.

The pyramid comes from this frame's depth with this frame's camera and meshes, so a mesh is never
hidden by an old picture: no popping and no reprojection. A mesh that comes into view is not
visible in the last frame, so it is tested in phase 1 and drawn in the second pass. The first
frame (or a new buffer) draws everything in the second pass.

The test must never hide a visible mesh. It uses the nearest depth of the mesh's bounding box,
the farthest depth under the box's screen rectangle (a pyramid level that covers it with 2 x 2
texels), and a small bias (`Occlusion::DepthBias`). A box that reaches the camera's plane is never
hidden. Both phases write every slot's command, so the recorded draw calls do not change from frame
to frame. A resize makes the pyramid again (`OffscreenSubpass::OnSwapchainResized`).

## Blending on the GPU

The same compute pass makes the world matrix of a mesh that moves between two ticks
(`Plans/gpu-blend.md`). Its slot holds the two `Transform` values (`OcclusionSlot::From*`, `To*`)
and the frame's blend factor is in the parameters. Phase 0 writes the matrix into the world matrix
buffer, which the G-buffer vertex shader reads, and tests the mesh against the frustum, since the
CPU has no matrix for it. `--gpu-blend off`, or occlusion culling off, makes the CPU do it.

## The CPU twin

`Occlusion::ProjectBounds`, `Occlusion::IsOccluded` and `DepthPyramid` (`OcclusionTest.h`) do the
same as the shaders. `FlingTests/Graphics/OcclusionCullingTests.cpp` runs them without a window,
including a randomized scene (rotated boxes, moving camera) that checks no visible box is hidden.
Keep the shaders and the CPU code in step.

## Check it

`Sandbox --offscreen --unattended --frames 60 --seed 1 --screenshot a.png --occlusion-culling on`
and the same with `off` give the same picture. Set `EnableValidationLayers=true` in
`Config/EngineConf.ini` to run with the validation layers.
