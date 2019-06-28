# Engine Bay

This is a testing grounds for messing around with making a Vulkan game engine.
The goals of this project are as follows:

* Gain some insight on different low-level engine systems implementations
* See how a modern graphics API like Vulkan affects engine architecture
* Explore cross platform game engine architecture (in this case with GLFW) 

I am basing the core of the rendering pipeline off of the [Vulkan Tutorial](https://vulkan-tutorial.com/Drawing_a_triangle/Setup/Validation_layers)

# Getting Started

## Submodules

This project does use submoudles, so either clone it with the `--recursive` flag
or run `git submodule update --init --recursive` in the root directory once the 
repo is cloned. 

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

## CMake

Once you have all the proper submodules and the SDK setup, you can simply run CMake
on your device to get your project files. If you are on Windows and prefer the command 
line, you can just run `buildVS.bat` or `buildVS.sh` to generate Visual Studio project
files to a `build` directory. 