#!/bin/bash
cppcheck -iexternal/json -iexternal/entt -iexternal/stb -iexternal/inih -iexternal/spdlog -iexternal/glm -iexternal/Catch2 -iexternal/glfw --enable=all . > cpp_check_res.txt

