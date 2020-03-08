#!/bin/bash

cppcheck --library=vulkan.cfg --library=lua.cfg -iexternal/ --enable=all .. \ > cppcheck_res.txt 
