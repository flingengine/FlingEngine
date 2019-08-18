#!/bin/bash
cppcheck -iexternal/inih -iexternal/spdlog -iexternal/glm -iexternal/Catch2 -iexternal/glfw --enable=all . > cpp_check_res.txt

