# Fling Engine

Fling Engine is a cross platform Vulkan game engine that will experiment with the following:

* Low-level engine systems such as render API abstraction, file systems, and custom allocators. 
* Multithreaded engine architecture 
* The Vulkan graphics API for real time rendering

I am basing the core of the rendering pipeline off of the [Vulkan Tutorial](https://vulkan-tutorial.com/Drawing_a_triangle/Setup/Validation_layers)

# Getting Started

## CMake

This project requires uses CMake 3.13 or higher, you can install it [here](https://cmake.org/download/).

## Vulkan SDK

You can download the SDK from the LunarG website [here](https://www.lunarg.com/vulkan-sdk/). 
You need to do this before the engine can compile. 

If you are having trouble with the Vulkan SDK then check out some of these resources: 
* [Vulkan Verify Install](https://vulkan.lunarg.com/doc/view/1.1.106.0/windows/getting_started.html#user-content-verify-the-installation)
* [Vulkan Tutorial FAQ](https://vulkan-tutorial.com/FAQ)

## `Init.bat` and `Init.sh`
Running either one of these scripts will simply get all submodules and external libraries
that the engine uses and create a folder called `build`. The `build` folder will have your
platform specific build files (Visual Studio, Makefiles, etc). 

## Config and Assets folders
As of right now (7/18/2019) you need to manually copy the `Assets` and `Config` folder to the same 
directory as your executable to avoid runtime errors. 

### Wanna contribute?

If you have any contributions or fixes that you want to contribute, then feel free to open 
an issue or a pull request! I'm happy to talk about the project, so feel free to reach out
to me on [Twitter](https://twitter.com/BenjaFriend?lang=en) or here on GitHub.