# Third-party notices

Fling Engine is built on open source work. Thank you to the people who write and
maintain these projects.

## SDL_GameControllerDB

[`Config/gamecontrollerdb.txt`](Config/gamecontrollerdb.txt) is a copy of the
community game controller database from
[SDL_GameControllerDB](https://github.com/mdqinc/SDL_GameControllerDB), a
database that its many contributors made together. It holds a button and
axis mapping for more than 2,000 controllers. The GLFW input interface loads it
so that those controllers work as gamepads (see
`GlfwInputDeviceInterface::LoadGamepadMappings`).

If a controller you own is not in the database, please add it upstream, so that
every game that uses the database gets it. The project's README has a mapping
guide and lists tools that make mappings.

The copy here was downloaded on 2026-10-25 and has not been changed, except for
the comment lines at the top of the file.

```
Copyright (C) 1997-2025 Sam Lantinga <slouken@libsdl.org>

This software is provided 'as-is', without any express or implied
warranty.  In no event will the authors be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not
   claim that you wrote the original software. If you use this software
   in a product, an acknowledgment in the product documentation would be
   appreciated but is not required.
2. Altered source versions must be plainly marked as such, and must not be
   misrepresented as being the original software.
3. This notice may not be removed or altered from any source distribution.
```

## Libraries in `external/`

These are git submodules. Each one keeps its own license file, listed here.

| Library | Used for | License | License file |
| --- | --- | --- | --- |
| [Catch2](https://github.com/catchorg/Catch2) | Unit tests | Boost Software License 1.0 | `external/Catch2/LICENSE.txt` |
| [cereal](https://github.com/USCiLab/cereal) | Serialization | BSD 3-Clause | `external/cereal/LICENSE` |
| [EnTT](https://github.com/skypjack/entt) | Entity component system, signals | MIT | `external/entt/LICENSE` |
| [GLFW](https://www.glfw.org/) | Windows and input | zlib | `external/glfw/LICENSE.md` |
| [GLM](https://github.com/g-truc/glm) | Math | MIT (or the Happy Bunny License) | `external/glm/copying.txt` |
| [Dear ImGui](https://github.com/ocornut/imgui) | Editor and debug UI | MIT | `external/imgui/LICENSE.txt` |
| [inih](https://github.com/benhoyt/inih) | INI config files | BSD 3-Clause | `external/inih/LICENSE.txt` |
| [JSON for Modern C++](https://github.com/nlohmann/json) | JSON files, input recordings | MIT | `external/json/LICENSE.MIT` |
| [spdlog](https://github.com/gabime/spdlog) | Logging | MIT | `external/spdlog/LICENSE` |
| [SPIRV-Cross](https://github.com/KhronosGroup/SPIRV-Cross) | Shader reflection | Apache 2.0 | `external/SPIRV-Cross/LICENSE` |
| [stb](https://github.com/nothings/stb) | Image loading | MIT or public domain | At the end of each header in `external/stb/` |
| [tinyobjloader](https://github.com/tinyobjloader/tinyobjloader) | OBJ model loading | MIT | `external/tinyobjloader/LICENSE` |

When you add a library or a data file from another project, add it here.
