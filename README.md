# Fling Engine

This project is a testing grounds for messing around with making a Vulkan game engine.
The goals of this project are as follows:

* Gain some insight on different low-level engine systems implementations
* See how a modern graphics API like Vulkan affects engine architecture
* Explore cross platform game engine architecture (in this case with GLFW) 


I am basing the core of the rendering pipeline off of the [Vulkan Tutorial](https://vulkan-tutorial.com/Drawing_a_triangle/Setup/Validation_layers)

# Getting Started

## Vulkan SDK

Fling uses two enviornment variables, `VULKAN_INCLUDE` and `VULKAN_LIB` to 
include and link Vulkan respectively. You can download the SDK from the LunarG
website [here](https://www.lunarg.com/vulkan-sdk/). After installing, set your 
enviornment variables to the the `Include` and `Lib` folders of the SDK according
to your installation. They might look something like this: 

```
VULKAN_LIB = D:\VulkanSDK\1.1.108.0\Lib
VULKAN_INCLUDE = D:\VulkanSDK\1.1.108.0\Include
```

On Linux you can either `export` these variables or set them in your `.bashrc` 

## `Init.bat`
To get started just run `Init.bat` on Windows and it will clone all external SDKs and 
compile the ones that are necessary. If you are using Visual Studio then you project 
files will be in the `build` folder, and you can start editing! 

## Shaders
As of right now (7/7/2019) the shaders are relative paths to the engine, make sure that you 
copy the `FlingEngine/Shaders` folder to the directory of your executable! 
