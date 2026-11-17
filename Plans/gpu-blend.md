# Blending meshes on the GPU

Status: stages 1 and 2 are built, on top of the occlusion culling pass (`Docs/occlusion-culling.md`). Stage 3 is next.

## Why

Between two ticks a moving mesh is drawn at a point between its two published transforms
(`Docs/threading-model.md`, "The two blend states"). `Graphics.PublishSnapshot` made that
matrix on the main thread for every moving mesh, every frame: a slerp, a matrix, and several
component lookups. In a Debug build the lighting test level (3,136 rotating spheres) spent
6.4 ms a frame on it, against 1.6 ms with the spheres still.

Frustum tests on the CPU cut part of that (`PreCullView`), but occlusion is decided on the
GPU after the snapshot, with the blended matrices. The CPU cannot know which meshes are hidden,
so it cannot skip their blend. The way to stop paying for it is to not make the matrix on the CPU.

## Design

A mesh that moved between the two ticks, and has no `Parent`, goes into the snapshot as its two
`Transform` values (`MeshDrawItem::From` and `To`, `HasBlend`) and the frame's factor
(`RenderSnapshot::BlendAlpha`). Everything else keeps its matrix: a still mesh uses its
`LocalToWorld`, a mesh far outside the last view uses its latest `LocalToWorld`, and a child
of a moving parent is blended on the CPU as before (a chain needs the whole parent walk).

The renderer then makes the world matrix:

1. **CPU, always correct** (stage 1): `MeshDrawItem::ResolveWorld`. Used when the GPU path is
   not running, and by passes that need a matrix on the CPU (debug meshes).
2. **GPU** (stage 2): the occlusion compute pass blends each slot that has a pair into a shared
   world matrix buffer, tests the result against the frustum and the depth pyramid, and the
   vertex shader reads the matrix from that buffer by slot. The CPU writes only the pairs
   (and the matrices of still meshes, when their `LocalToWorld` version changes).

The GPU path needs the occlusion compute pass, so it runs only when occlusion culling is on and
supported. With it off, the renderer resolves the matrices on the CPU (stage 1).

## Stages

1. Snapshot carries the pair; the renderer resolves it on the CPU. No picture change.
2. World matrix buffer, the blend and the frustum test in `cull.comp`, the vertex shader reads
   the buffer. `--gpu-blend on|off`.
3. (Not built yet.) Snapshot rebuilt only when a tick arrives (the pairs do not change between ticks), so a frame
   with no new tick does no per-mesh work on the CPU.

## What was built (stages 1 and 2)

- `MeshDrawItem::HasBlend`, `From`, `To` and `RenderSnapshot::BlendAlpha`. `PublishSnapshot` gives a
  mesh the pair when it moved between the two ticks and has no `Parent`; a mesh far outside the
  last view (`PreCullView`) or a child keeps a matrix.
- The G-buffer vertex shader (`mrt.vert`) reads its matrix from one storage buffer by slot
  (`OffscreenSubpass::m_WorldBuffer`, binding 6). The per mesh UBO holds only the slot. The
  CPU writes the matrix of a still mesh when its `LocalToWorld` version changes; with `--gpu-blend off`
  (or with occlusion culling off) it also writes the blended matrices.
- `cull.comp` makes the matrix of each slot with `Blend` in phase 0 (`SlerpQuat`, `LocalMatrix`: the
  same as `Interpolate::Blend` and `TransformUtil::LocalMatrix`) and decides the frustum test for it
  (`InFrustum`: the same as `Frustum::Intersects`), because the CPU has no matrix for it. A barrier
  makes the matrices visible to the vertex shader.
- Shader reflection (`Shader.cpp`) now tells a storage buffer from a uniform buffer.
- `--gpu-blend on|off`, `[Engine] GpuBlend`, a checkbox in GPU Options. On by default; it needs
  occlusion culling to run, otherwise the CPU blends.

## Measured

Debug build, `--offscreen --exec threaded`, 3,136 rotating spheres, CPU ms per frame:

| | CPU blend | GPU blend |
|---|---|---|
| everything in view (`--frustum-culling off`): `ResolveWorlds` | 0.43 | 0.01 |
| default view: `ResolveWorlds` + `Cull` | 0.14 + 2.34 | 0.02 + 1.57 |

`PublishSnapshot` is 1.2 to 1.7 ms in both: that is the per mesh loop (stage 3). Before stage 1 it
was 6.4 ms with the spheres turning. Most of that was the lookups of the old blend (two walks of
the parent chain, several component lookups), not the slerp, so stage 1 gave most of the gain.

The picture: GPU and CPU blend at the same tick differ by at most 3 of 255 on 1,697 of 2 million
pixels (float rounding), and both differ greatly from `--interpolation off`, so the check does
test the blend. No validation layer messages, with and without occlusion culling.

## Checking it

- `--offscreen --frames-per-tick N` puts several frames in one tick, so the factor goes through
  0..1. A screenshot with `--gpu-blend on` and `off` at the same tick must match (within
  float rounding).
- `--profile`: `Graphics.PublishSnapshot` and `Offscreen.*` before and after.
