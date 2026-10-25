# Vulkan loading plan

Design for how the Graphics module loads Vulkan at runtime, with only the
official Vulkan SDK (Khronos headers and loader) and no third-party Vulkan
helper libraries.

L1 and L2 are implemented. This file stays a plan until the rules move fully
into [`Skills/`](../Skills/). The rules that already hold are also noted in
[`Skills/architecture.md`](../Skills/architecture.md) and
[`Skills/testing.md`](../Skills/testing.md).

Related plans: [`improve-ai-iteration.md`](improve-ai-iteration.md) (phase A,
`--null-render`) and [`project-model.md`](project-model.md) (P5, the
`<Name>Server` target).

## Problem

- Graphics used to link `Vulkan::Vulkan`, the import library of the Vulkan
  loader. The operating system then loaded the loader (`vulkan-1.dll`,
  `libvulkan.so.1`) when the process started, before `main()`. A process could
  not start on a machine that has no Vulkan runtime, even with `--null-render`.
- The short-term fix (PR #219) delay-loaded `vulkan-1.dll` with MSVC
  `/DELAYLOAD`. It worked only with MSVC. Linux still loaded `libvulkan.so.1`
  at process start. MinGW has no `/DELAYLOAD`.
- With delay loading, a missing runtime in a windowed run was an exception on
  the first Vulkan call, not an error that the engine can report.

## Goals

1. A `--null-render` run starts with no Vulkan runtime, on every platform and
   every compiler.
2. A windowed run with no Vulkan runtime logs a clear error and exits with
   `ExitCode::Crash` (2). There is no exception from the operating system and
   no "DLL not found" dialog.
3. Only the official Vulkan SDK: the Khronos headers and the Khronos loader.
   No volk and no other third-party Vulkan helper library.
4. Call sites in Graphics do not change. `vkCreateBuffer(...)` stays
   `vkCreateBuffer(...)`.
5. Building needs the Vulkan headers. The linked binary must not import
   `libvulkan` or `vulkan-1.dll`.

## Non-goals

- More than one Vulkan device at a time. The engine has one `LogicalDevice`.
  One global function table is correct. A per-device table can come later if
  that changes.
- Vulkan-Hpp (`vulkan.hpp`). Graphics uses the C API.
- A Graphics module that is not linked. That is the `<Name>Server` target
  (P5 in `project-model.md`).
- A player message box. A log line and exit code 2 are enough.
- Generating `VulkanFunctions.inl` from `vk.xml`. The hand-kept list is enough.

## Design

### 1. No prototypes

`FlingVulkan.h` is the only first-party file that includes `<vulkan/vulkan.h>`.
It defines `VK_NO_PROTOTYPES` before the include. Graphics also compiles with
`VK_NO_PROTOTYPES`, so a direct include cannot turn the prototypes back on.
The SDK header then declares the `PFN_vk...` types and no functions.

`DesktopWindow.h` includes `FlingVulkan.h` before it defines
`GLFW_INCLUDE_VULKAN`. `glfw3.h` then includes `<vulkan/vulkan.h>` with the
prototypes already off.

### 2. One list of functions

`FlingEngine/Graphics/src/VulkanFunctions.inl` is the hand-kept X-macro list of
every Vulkan command first-party code calls. The same list declares the
`extern "C"` pointers in `FlingVulkan.h`, defines them in `VulkanLoader.cpp`,
and loads them. A command added to the list is added to all three.

Groups:

- `FLING_VK_GLOBAL` — loader commands, from `vkGetInstanceProcAddr(nullptr, ...)`.
- `FLING_VK_GLOBAL_OPTIONAL` — same level. A null pointer is not an error.
  `vkEnumerateInstanceVersion` is in this group. Older loaders may not export it.
- `FLING_VK_INSTANCE` — instance commands, including commands whose first
  parameter is `VkPhysicalDevice`. Load them with `vkGetInstanceProcAddr(instance, ...)`.
- `FLING_VK_INSTANCE_OPTIONAL` — extension commands that may be absent.
  `vkCreateDebugUtilsMessengerEXT` and `vkDestroyDebugUtilsMessengerEXT` are here.
  A null pointer is not an error.
- `FLING_VK_DEVICE` — commands whose first parameter is `VkDevice`, `VkQueue`,
  or `VkCommandBuffer`. Load them with `vkGetDeviceProcAddr`.
- `FLING_VK_DEVICE_OPTIONAL` — same level. A null pointer is not an error.
  The group is empty today.

`vkGetDeviceProcAddr` takes `VkDevice`, but it is loaded with
`vkGetInstanceProcAddr`. It is how the device commands are loaded. Physical-device
commands stay on the instance list.

Each pointer is a global with the C API name:

```cpp
extern "C" PFN_vkCreateBuffer vkCreateBuffer;
```

`VK_NO_PROTOTYPES` removes the real declarations, so these names are free and
call sites do not change.

A null required pointer logs `Required Vulkan function is null: <name>` and
stops the process with `ExitCode::Crash`. A null optional pointer is not an
error.

`FlingTests/VulkanLoad/CheckVulkanCalls.cmake` fails when a first-party
`vk[A-Z]` call is missing from the list. CTest runs it as
`Vulkan calls are listed`. Do not generate the list from `vk.xml` unless the
hand-kept list becomes a problem.

### 3. Loader

`Fling::DynamicLibrary` (Core) is the only code that calls `LoadLibraryEx` or
`dlopen`. Graphics does not call `LoadLibrary`, `LoadLibraryEx`, `dlopen`,
`GetProcAddress`, or `dlsym`.

`DynamicLibrary` is move-only. `Open` loads one file name. `OpenFirst` tries
each name and returns the first library that opens. `Symbol` returns an
address, or null when the library is closed or the symbol is missing. `Close`
unloads the library and is safe to call twice.

On Windows, `Open` calls `WinVerifyTrust` with
`WINTRUST_ACTION_GENERIC_VERIFY_V2` before it maps the file. The check uses
`WTD_UI_NONE`, `WTD_REVOKE_NONE`, and
`WTD_SAFER_FLAG | WTD_CACHE_ONLY_URL_RETRIEVAL`. Startup shows no UI and does
not use the network. `ERROR_SUCCESS` means the signature is valid. That covers
an embedded signature and a catalog signature, so `kernel32.dll` still loads.
`TRUST_E_NOSIGNATURE`, `TRUST_E_SUBJECT_FORM_UNKNOWN`, and
`TRUST_E_PROVIDER_UNKNOWN` report "The shared library is not signed". Any other
failure reports "The shared library signature is not trusted".

A bare Windows name is resolved in System32 (`GetSystemDirectoryW`), then in
the directory of the executable (`GetModuleFileNameW`). The current directory
and `PATH` are not searched. When a candidate file exists and the signature
check fails, `Open` returns that failure and does not try the next directory.
A name that contains a slash, a backslash, or a drive prefix is an explicit
path. `Open` resolves it with `GetFullPathNameW`, verifies that file, and does
not search. The load uses `LoadLibraryExW` on the absolute path that passed
the check, with
`LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32`. A missing
file reports "The shared library was not found". The name is UTF-8 and the
Win32 calls take UTF-16. A missing DLL does not open a dialog. `Symbol` uses
`GetProcAddress`. `Close` uses `FreeLibrary`.

On Linux, `Open` uses `dlopen(name, RTLD_NOW | RTLD_LOCAL)`. Linux does not
check a signature. `Symbol` uses `dlsym` and `Close` uses `dlclose`. There is
no flag parameter. macOS is not a target.

`VulkanLoader` (Graphics, private) uses that type:

| Step | Called from | Does |
| --- | --- | --- |
| `LoadLoader()` | Start of `VulkanApp::Init`, before GLFW | Linux: `OpenFirst` of `libvulkan.so.1`, then `libvulkan.so`. Windows: `Open("vulkan-1.dll")`. `SymbolAs` reads `vkGetInstanceProcAddr` from the library, then the global commands are loaded. Returns false when the library or that symbol is missing |
| `LoadInstance(VkInstance)` | After `vkCreateInstance` (`Instance.cpp`) | Instance commands, from `vkGetInstanceProcAddr` |
| `LoadDevice(VkDevice)` | After `vkCreateDevice` (`LogicalDevice.cpp`) | Device commands, from `vkGetDeviceProcAddr` |
| `UnloadLoader()` | End of `VulkanApp::Shutdown`, after `vkDestroyInstance` | Sets every Vulkan function pointer to null, then `Close()` |

`vkGetInstanceProcAddr` is taken from the library with `Symbol`. It is not
replaced by calling `vkGetInstanceProcAddr` on itself. Vulkan 1.0 and 1.1
loaders do not return that pointer.

When `LoadLoader()` fails, `VulkanApp::Init` logs
"Vulkan runtime not found: install a GPU driver with Vulkan support" and
throws. `main` catches that and returns `ExitCode::Crash` (2). There is no
message box. Under `--null-render`, `VulkanApp::Init` is not called, so
`DynamicLibrary::Open` is not called.

### 4. GLFW

GLFW 3.4 and newer can take our loader. `DesktopWindow` calls
`glfwInitVulkanLoader(vkGetInstanceProcAddr)` and then `glfwInit()`. GLFW uses
that pointer and does not open a second copy. `LoadLoader()` runs first, so the
pointer is set.

### 5. CMake

- Graphics links `Vulkan::Headers`, not `Vulkan::Vulkan`.
- `/DELAYLOAD:vulkan-1.dll` and `delayimp` are gone.
- `find_package(Vulkan REQUIRED)` stays. Configure time still needs the SDK
  library so CMake can find the headers. The linked Sandbox binary must not
  import `libvulkan` or `vulkan-1.dll`.
- Core links `${CMAKE_DL_LIBS}` privately. `dlopen` is in libc on glibc 2.34
  and newer. Older glibc keeps it in `libdl`. On Windows, Core links
  `wintrust` for `WinVerifyTrust`. Graphics does not call `dlopen`.

## Tests

- **No Vulkan import.** `Sandbox: no Vulkan import` runs
  `FlingTests/VulkanLoad/ExpectNoVulkanImport.cmake`. Linux: `readelf -d` has
  no `libvulkan` in `NEEDED`. Windows: the PE import table (and the delay-import
  table) has no `vulkan-1.dll`, read with `objdump -p`, `dumpbin /IMPORTS`, or
  `pe_imports.py`.
- **Call list.** `Vulkan calls are listed` runs `CheckVulkanCalls.cmake`.
- **Headless.** `Sandbox --null-render --unattended --frames 120 --seed 1`
  exits 0. It does not open the loader.
- **Missing runtime.** The `Linux no Vulkan runtime` CI job builds with the SDK
  installed, then runs Sandbox in a mount namespace where `libvulkan.so.1` and
  `libvulkan.so` are not loadable. The windowed run must exit 2 and log
  "Vulkan runtime not found". `LoadLoader()` runs before GLFW opens a window,
  so the test does not need a display. The same namespace runs the headless
  command and expects exit 0. The other CI jobs are unchanged.

## Phases

| Phase | Status | Work |
| --- | --- | --- |
| **L1** | Done | `VK_NO_PROTOTYPES`, `VulkanFunctions.inl`, `VulkanLoader`, `DynamicLibrary`. Link `Vulkan::Headers`. Remove `/DELAYLOAD`. No-import test and call-list test |
| **L2** | Done | `glfwInitVulkanLoader`. Log line and `ExitCode::Crash` when the runtime is missing. Linux CI job with the loader not loadable |
| **Later** | Not started | Generate `VulkanFunctions.inl` from `vk.xml` if the hand-kept list becomes a problem. A per-device table, if the engine uses more than one device |

## Decisions

- The hand-kept list is enough. A CTest fails when a first-party `vk[A-Z]` call
  is missing from it. Do not generate from `vk.xml` now.
- Device commands load with `vkGetDeviceProcAddr`. There is one `LogicalDevice`,
  so one table is correct. Physical-device commands stay on the instance list.
- A missing runtime logs one line and exits with code 2. There is no player
  message box.
