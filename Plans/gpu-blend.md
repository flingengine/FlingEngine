# Blending meshes on the GPU

Status: in progress, on top of the occlusion culling pass (`Docs/occlusion-culling.md`).

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
3. Snapshot rebuilt only when a tick arrives (the pairs do not change between ticks), so a frame
   with no new tick does no per-mesh work on the CPU.

## Checking it

- `--offscreen --frames-per-tick N` puts several frames in one tick, so the factor goes through
  0..1. A screenshot with `--gpu-blend on` and `off` at the same tick must match (within
  float rounding).
- `--profile`: `Graphics.PublishSnapshot` and `Offscreen.*` before and after.
