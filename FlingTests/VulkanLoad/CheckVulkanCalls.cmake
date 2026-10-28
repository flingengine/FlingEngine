# Fail when a first-party vk[A-Z] call is missing from VulkanFunctions.inl.
#
#   cmake -DSOURCE_ROOT=<repo> -P CheckVulkanCalls.cmake

cmake_minimum_required(VERSION 3.21)

if(NOT DEFINED SOURCE_ROOT)
	message(FATAL_ERROR "Set -DSOURCE_ROOT= to the repository root")
endif()

set(_inl "${SOURCE_ROOT}/FlingEngine/Graphics/src/VulkanFunctions.inl")
if(NOT EXISTS "${_inl}")
	message(FATAL_ERROR "Vulkan function list not found: ${_inl}")
endif()

file(READ "${_inl}" _inl_text)
string(REGEX MATCHALL "\\(vk[A-Za-z0-9]+\\)" _listed_raw "${_inl_text}")
set(_listed "")
foreach(_raw IN LISTS _listed_raw)
	string(REGEX REPLACE "[()]" "" _name "${_raw}")
	list(APPEND _listed "${_name}")
endforeach()
list(REMOVE_DUPLICATES _listed)

function(fling_strip_inactive _input _output)
	set(_text "${_input}")
	# Block comments, then line comments. A vk* name inside a string is not a call:
	# the pattern below requires the '(' of a call.
	string(REGEX REPLACE "/\\*([^*]|\\*+[^*/])*\\*+/" "" _text "${_text}")
	string(REGEX REPLACE "//[^\n]*" "" _text "${_text}")
	set(${_output} "${_text}" PARENT_SCOPE)
endfunction()

set(_calls "")
foreach(_root IN ITEMS FlingEngine Sandbox FlingTests)
	file(GLOB_RECURSE _files
		"${SOURCE_ROOT}/${_root}/*.cpp"
		"${SOURCE_ROOT}/${_root}/*.cc"
		"${SOURCE_ROOT}/${_root}/*.h"
		"${SOURCE_ROOT}/${_root}/*.hpp"
		"${SOURCE_ROOT}/${_root}/*.inl"
	)
	foreach(_file IN LISTS _files)
		file(READ "${_file}" _text)
		fling_strip_inactive("${_text}" _text)
		string(REGEX MATCHALL "(^|[^A-Za-z0-9_])vk[A-Z][A-Za-z0-9]*[ \t\r\n]*\\(" _matches "${_text}")
		foreach(_match IN LISTS _matches)
			string(REGEX MATCH "vk[A-Z][A-Za-z0-9]*" _name "${_match}")
			if(_name)
				list(APPEND _calls "${_name}")
			endif()
		endforeach()
	endforeach()
endforeach()
list(REMOVE_DUPLICATES _calls)
list(SORT _calls)

set(_missing "")
foreach(_call IN LISTS _calls)
	if(NOT _call IN_LIST _listed)
		list(APPEND _missing "${_call}")
	endif()
endforeach()

if(_missing)
	list(JOIN _missing "\n  " _missing_text)
	message(FATAL_ERROR "Vulkan calls missing from VulkanFunctions.inl:\n  ${_missing_text}")
endif()

list(LENGTH _calls _call_count)
list(LENGTH _listed _listed_count)
message(STATUS "Vulkan function list covers ${_call_count} first-party calls (${_listed_count} names listed)")
