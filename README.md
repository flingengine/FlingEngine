# Fling Engine

This project is a testing grounds for messing around with making a Vulkan game engine.
The goals of this project are as follows:

* Gain some insight on different low-level engine systems implementations
* See how a modern graphics API like Vulkan affects engine architecture
* Explore cross platform game engine architecture (in this case with GLFW) 

I am basing the core of the rendering pipeline off of the [Vulkan Tutorial](https://vulkan-tutorial.com/Drawing_a_triangle/Setup/Validation_layers)

# Getting Started

## Vulkan SDK

You can download the SDK from the LunarG website [here](https://www.lunarg.com/vulkan-sdk/). 
After installing, set your enviornment variables to the the `Include` and `Lib` folders of 
the SDK according to your installation.  

If you are having trouble with the Vulkan SDK then check out some of these resources: 
* [Vulkan Verify Install](https://vulkan.lunarg.com/doc/view/1.1.106.0/windows/getting_started.html#user-content-verify-the-installation)
* [Vulkan Tutorial FAQ](https://vulkan-tutorial.com/FAQ)

## `Init.bat` and `Init.sh`
To get started just run `Init.bat` on Windows and it will clone all external SDKs and 
compile the ones that are necessary. If you are using Visual Studio then you project 
files will be in the `build` folder, and you can start editing! 

## Config and Assets folders
As of right now (7/18/2019) you need to manually copy the `Assets` and `Config` folder to the same 
directory as your executeable for the shaders to get loaded properly. 