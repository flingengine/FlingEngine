# Vulkan loading plan

Design for how the Graphics module loads Vulkan at runtime, with only the
official Vulkan SDK (Khronos headers and loader) and no third-party Vulkan
helper libraries.

This is a plan, not an established standard. When a phase is done, move the
rules that it establishes into [`Skills/`](../Skills/).

Related plans: [`improve-ai-iteration.md`](improve-ai-iteration.md) (phase A,
`--null-render`) and [`project-model.md`](project-model.md) (P5, the
`<Name>Server` target).

## Problem

- Graphics links `Vulkan::Vulkan`, the import library of the Vulkan loader.
  The operating system then loads the loader (`vulkan-1.dll`,
  `libvulkan.so.1`) when the process starts, before `main()`. A process cannot
  start on a machine that has no Vulkan runtime, even with `--null-render`.
- The short-term fix (PR #219) delay-loads `vulkan-1.dll` with MSVC
  `/DELAYLOAD` (`FlingEngine/Graphics/CMakeLists.txt`). It works only with
  MSVC:
  - Linux still loads `libvulkan.so.1` at process start. A headless run on a
    server image with no `libvulkan1` package does not start.
  - MinGW has no `/DELAYLOAD`, so it still loads `vulkan-1.dll` at process start.
- With delay loading, a missing runtime in a windowed run is an exception on
  the first Vulkan call (a crash), not an error that the engine can report.
- Every Vulkan call goes through the loader's dispatch code. Device functions
  that come from `vkGetDeviceProcAddr` skip that step.

## Goals

1. A `--null-render` run starts with no Vulkan runtime, on every platform and
   every compiler.
2. A windowed run with no Vulkan runtime, or with no Vulkan driver, logs a
   clear error and exits with `ExitCode::Crash` (2). There is no exception and
   no "DLL not found" dialog.
3. Only the official Vulkan SDK: the Khronos headers and the Khronos loader.
   No volk and no other third-party Vulkan helper library.
4. Call sites in Graphics do not change. `vkCreateBuffer(...)` stays
   `vkCreateBuffer(...)`.
5. Building needs only the Vulkan headers, not the import library.

## Non-goals

- More than one Vulkan device at a time. The engine has one `LogicalDevice`.
  A per-device function table can come later if that changes.
- Vulkan-Hpp (`vulkan.hpp`). It is part of the SDK and has its own dynamic
  dispatcher, but Graphics uses the C API. Moving to Vulkan-Hpp is a much
  larger change than this plan.
- A Graphics module that is not linked. That is the `<Name>Server` target
  (P5 in `project-model.md`).

## Design

### 1. No prototypes

`FlingVulkan.h` is the only first-party file that includes `<vulkan/vulkan.h>`.
It defines `VK_NO_PROTOTYPES` before the include. The SDK header then declares
the `PFN_vk...` function-pointer types, but no functions. Nothing links against
the loader, so the import library is no longer needed.

Every first-party file must get Vulkan through `FlingVulkan.h`.
`DesktopWindow.h` defines `GLFW_INCLUDE_VULKAN`, and `glfw3.h` then includes
`<vulkan/vulkan.h>` itself. It must include `FlingVulkan.h` first, so
`VK_NO_PROTOTYPES` is set (or stop using `GLFW_INCLUDE_VULKAN`). A build check:
compile one file that calls a Vulkan function without the table; it must fail.

### 2. One list of functions

A hand-written X-macro list, `Graphics/src/VulkanFunctions.inl`, names every
Vulkan function that Graphics calls (about 90 today). It has three sections,
one for each level at which Vulkan gives the pointer:

```cpp
// Loader level: from vkGetInstanceProcAddr(nullptr, ...)
FLING_VK_GLOBAL(vkCreateInstance)
FLING_VK_GLOBAL(vkEnumerateInstanceExtensionProperties)
FLING_VK_GLOBAL(vkEnumerateInstanceLayerProperties)

// Instance level: from vkGetInstanceProcAddr(instance, ...)
FLING_VK_INSTANCE(vkDestroyInstance)
FLING_VK_INSTANCE(vkEnumeratePhysicalDevices)
FLING_VK_INSTANCE(vkGetPhysicalDeviceSurfaceCapabilitiesKHR)
FLING_VK_INSTANCE(vkCreateDevice)
// ...

// Device level: from vkGetDeviceProcAddr(device, ...)
FLING_VK_DEVICE(vkCreateBuffer)
FLING_VK_DEVICE(vkCmdDraw)
FLING_VK_DEVICE(vkQueuePresentKHR)
// ...
```

- The same list declares the pointers (in `FlingVulkan.h`), defines them, and
  loads them (in `VulkanLoader.cpp`). A function that is added to one place is
  added to all three.
- Each pointer is a global with the same name as the Vulkan function:
  `extern PFN_vkCreateBuffer vkCreateBuffer;`. `VK_NO_PROTOTYPES` removes the
  real declarations, so these names are free, and call sites do not change.
  This is the same approach that the Khronos samples use when they load
  Vulkan dynamically.
- The SDK's `vk.xml` registry says at which level each function is (a
  `VkDevice`, `VkQueue`, or `VkCommandBuffer` first parameter means device
  level). The list is short enough to keep by hand. A generator from `vk.xml`
  is an option later, if the list grows a lot.
- A function from an extension (`vkCreateSwapchainKHR`, debug utils) is
  loaded only after its extension is enabled. A null pointer for an optional
  extension is not an error. For a required extension it is.

### 3. Loader

`VulkanLoader` (Graphics, private) has three steps. `VulkanApp` calls them in
order:

| Step | Called from | Does |
| --- | --- | --- |
| `LoadLoader()` | Start of `VulkanApp::Init` | `LoadLibraryW(L"vulkan-1.dll")` on Windows, `dlopen("libvulkan.so.1", RTLD_NOW \| RTLD_LOCAL)` on Linux. Gets `vkGetInstanceProcAddr`, then the loader-level pointers. Returns false if the library or the symbol is missing |
| `LoadInstance(VkInstance)` | After `vkCreateInstance` (`Instance.cpp`) | Instance-level pointers |
| `LoadDevice(VkDevice)` | After `vkCreateDevice` (`LogicalDevice.cpp`) | Device-level pointers, from `vkGetDeviceProcAddr` |

- `UnloadLoader()` runs at the end of `VulkanApp::Shutdown`, after
  `vkDestroyInstance`. It sets every pointer back to null.
- If `LoadLoader()` fails, `VulkanApp::Init` logs
  "Vulkan runtime not found: install a GPU driver with Vulkan support" and the
  engine exits with `ExitCode::Crash`. If a required pointer is null after
  `LoadInstance` or `LoadDevice`, it logs the function name.
- Under `--null-render`, `VulkanApp::Init` is not called, so the loader is
  never loaded.
- macOS (MoltenVK) is not a target today (`Platform.h` stops the build). If it
  becomes one, `LoadLoader()` also tries `libvulkan.1.dylib`.

### 4. GLFW

GLFW loads the Vulkan loader itself for `glfwCreateWindowSurface` and
`glfwGetRequiredInstanceExtensions`. GLFW 3.4 and newer (the vendored version
is 3.5) has `glfwInitVulkanLoader(PFN_vkGetInstanceProcAddr)`. Call it with our
`vkGetInstanceProcAddr` before `glfwInit()`, so that GLFW uses the same loader
and does not open a second copy.

### 5. CMake

- Graphics links `Vulkan::Headers` (from CMake's `FindVulkan`) instead of
  `Vulkan::Vulkan`.
- Remove the `/DELAYLOAD:vulkan-1.dll` and `delayimp` lines from
  `FlingEngine/Graphics/CMakeLists.txt`.
- Linux: link `${CMAKE_DL_LIBS}` for `dlopen` (glibc 2.34 and newer has it in
  libc, but older systems need `libdl`).
- `find_package(Vulkan REQUIRED)` stays. The SDK is still how a developer gets
  the headers, and the validation layers for debug runs.

## Tests

- **No Vulkan import.** A CTest that fails if the Sandbox binary imports the
  Vulkan loader: `readelf -d` (no `libvulkan` in `NEEDED`) on Linux, and the
  PE import table on Windows (`dumpbin /imports` or `objdump -p`). Run it with
  `cmake -P`, like `FlingTests/CrashProbe/ExpectCrash.cmake`.
- **Headless with no runtime.** One Linux CI job runs the headless CTest tests
  in a container without the `libvulkan1` package (build in a separate step
  that has it).
- **Missing runtime in a windowed run.** In the same container with no
  `libvulkan1`, a windowed Sandbox run must exit with code 2 and log
  "Vulkan runtime not found". `LoadLoader()` runs before GLFW opens a window,
  so the test does not need a display.
- The existing headless and windowed runs must give the same results as
  before.

## Phases

Each phase must leave the engine, Sandbox, and FlingTests building and passing
on every CI platform.

| Phase | Work | Depends on |
| --- | --- | --- |
| **L1** | `VK_NO_PROTOTYPES` in `FlingVulkan.h`, `VulkanFunctions.inl`, `VulkanLoader`, calls from `VulkanApp`, `Instance`, and `LogicalDevice`. Link `Vulkan::Headers`. Remove `/DELAYLOAD`. "No Vulkan import" test | None |
| **L2** | `glfwInitVulkanLoader`. Clear error and `ExitCode::Crash` when the runtime is missing. Linux CI job with no `libvulkan1` | L1 |
| **Later** | Generate `VulkanFunctions.inl` from `vk.xml`, if the hand-kept list becomes a problem. A per-device table, if the engine uses more than one device | L1 |

When L1 is done, update the Graphics rules in
[`Skills/testing.md`](../Skills/testing.md#headless-runs) and
[`Skills/architecture.md`](../Skills/architecture.md): include Vulkan only
through `FlingVulkan.h`, and add each new Vulkan function to
`VulkanFunctions.inl`.

## Open questions

- Is the hand-kept list enough, or do we generate it from `vk.xml` from the
  start? About 90 functions today suggests that by hand is fine.
- Do device-level functions get loaded with `vkGetDeviceProcAddr` (faster
  calls, one device only), or all with `vkGetInstanceProcAddr` (slower calls,
  but works with any device)? The plan picks `vkGetDeviceProcAddr`, because
  there is one device.
- Does the "runtime not found" case need a message box for players (not
  `--unattended`), or is a log line and exit code enough?
