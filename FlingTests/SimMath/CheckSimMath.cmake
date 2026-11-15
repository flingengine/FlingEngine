# Fail when simulation code names a math function that the platform's C library rounds
# its own way (std::sin, cos, atan2, exp, log, pow, ...) or a glm helper built on them
# (glm::angleAxis, glm::rotate, glm::slerp, ...). Simulation code uses SimMath instead
# (FlingEngine/Core/inc/SimMath.h), so a Linux server and a Windows client simulate the
# same bits. +, -, *, /, sqrt, floor, fmod and comparisons are exact everywhere and allowed.
#
# Every first-party source under FlingEngine/ and Sandbox/ is checked, except the
# presentation code listed below, which may use std and glm freely. Tests are not checked:
# they use std as a reference.
#
#   cmake -DSOURCE_ROOT=<repo> -P CheckSimMath.cmake
#   cmake -DSELF_TEST=ON -P CheckSimMath.cmake      (checks the patterns themselves)

cmake_minimum_required(VERSION 3.21)

# Presentation only: never runs in a fixed system, never writes simulation state
set(_exempt_dirs
	"FlingEngine/Graphics"   # the renderer
	"FlingEngine/Editor"     # the editor UI
)
set(_exempt_files
	"FlingEngine/Engine/inc/Camera.h"                         # projection matrices
	"FlingEngine/Engine/src/FirstPersonCamera.cpp"            # the main-thread debug camera
	"FlingEngine/Engine/inc/Interpolate.h"                    # frame-side blend between two ticks
	"Sandbox/Gameplay/src/Systems/MotionProbeSystem.cpp"      # a frame system (Order::Render) that measures motion
)

# Functions of <cmath> that are not exact, with their f and l forms
set(_libm "sin|cos|tan|asin|acos|atan|atan2|sinh|cosh|tanh|asinh|acosh|atanh|exp|exp2|expm1|log|log2|log10|log1p|pow|cbrt|hypot|erf|erfc|tgamma|lgamma")
# glm helpers that call them inside (glm::sin and the others are caught by the first pattern)
set(_glm "angleAxis|rotate|rotateX|rotateY|rotateZ|rotateNormalizedAxis|yawPitchRoll|eulerAngle[A-Z]*|extractEulerAngle[A-Z]*|orientate[234]|eulerAngles|slerp|squad|angle|axis|pitch|yaw|roll|perspective[A-Za-z]*|infinitePerspective[A-Za-z]*|tweakedInfinitePerspective")

# A call: not a member (. or ->) and not part of a longer name, optionally std:: or glm:: or ::
set(_libm_pattern "(^|[^A-Za-z0-9_.>:])((std|glm)?::)?(${_libm})[fl]?[ \t\r\n]*[(<]")
set(_glm_pattern "glm::(${_glm})[ \t\r\n]*[(<]")

function(fling_strip_inactive _input _output)
	set(_text "${_input}")
	string(REGEX REPLACE "/\\*([^*]|\\*+[^*/])*\\*+/" "" _text "${_text}")
	string(REGEX REPLACE "//[^\n]*" "" _text "${_text}")
	string(REGEX REPLACE "\"([^\"\\\\\n]|\\\\.)*\"" "\"\"" _text "${_text}")
	set(${_output} "${_text}" PARENT_SCOPE)
endfunction()

# The banned names a text calls, as a list
function(fling_find_banned _input _output)
	fling_strip_inactive("${_input}" _text)
	set(_found "")
	string(REGEX MATCHALL "${_libm_pattern}" _matches "${_text}")
	foreach(_match IN LISTS _matches)
		string(REGEX MATCH "[A-Za-z:][A-Za-z0-9_:]*" _name "${_match}")
		list(APPEND _found "${_name}")
	endforeach()
	string(REGEX MATCHALL "${_glm_pattern}" _matches "${_text}")
	foreach(_match IN LISTS _matches)
		string(REGEX MATCH "glm::[A-Za-z0-9_]+" _name "${_match}")
		list(APPEND _found "${_name}")
	endforeach()
	set(${_output} "${_found}" PARENT_SCOPE)
endfunction()

if(SELF_TEST)
	# No ';' in a sample: it would split the CMake list
	set(_must_find
		"std::sin(x)" "float a = cos(b)" "std::atan2f(y, x)" "::powl(a, b)" "x = exp (y)"
		"glm::sin(v)" "glm::atan<float>(y, x)" "glm::angleAxis(a, axis)" "glm::rotate(m, a, v)"
		"glm::slerp(a, b, t)" "glm::yawPitchRoll(y, p, r)" "glm::extractEulerAngleYXZ(m, a, b, c)"
		"glm::eulerAngles(q)" "glm::perspective(f, a, n, z)" "return log(x)+1")
	set(_must_pass
		"SimMath::Sin(x)" "SimMath::Atan2(y, x)" "std::sqrt(x)" "std::floor(x)" "std::fmod(a, b)"
		"glm::radians(d)" "glm::degrees(r)" "glm::normalize(q)" "glm::quat_cast(m)" "glm::lerp(a, b, t)"
		"// std::sin(x) in a comment" "/* glm::slerp(a, b) */" "const char* t = \"std::cos(x)\""
		"Logger.log(x)" "ptr->pow(2)" "Fling::Exp(x)" "catalog(x)" "MyLog(x)" "costs(x)" "SetPitch(x)")
	set(_failures "")
	foreach(_sample IN LISTS _must_find)
		fling_find_banned("${_sample}" _hits)
		if(NOT _hits)
			list(APPEND _failures "not found in: ${_sample}")
		endif()
	endforeach()
	foreach(_sample IN LISTS _must_pass)
		fling_find_banned("${_sample}" _hits)
		if(_hits)
			list(APPEND _failures "wrongly found ${_hits} in: ${_sample}")
		endif()
	endforeach()
	if(_failures)
		list(JOIN _failures "\n  " _text)
		message(FATAL_ERROR "The simulation math check's patterns are wrong:\n  ${_text}")
	endif()
	message(STATUS "The simulation math check finds every banned sample and passes every allowed one")
	return()
endif()

if(NOT DEFINED SOURCE_ROOT)
	message(FATAL_ERROR "Set -DSOURCE_ROOT= to the repository root")
endif()

# An exemption for a file that is gone is a mistake: fix the list
foreach(_path IN LISTS _exempt_dirs _exempt_files)
	if(NOT EXISTS "${SOURCE_ROOT}/${_path}")
		message(FATAL_ERROR "CheckSimMath.cmake exempts ${_path}, which does not exist")
	endif()
endforeach()

set(_problems "")
set(_checked 0)
foreach(_root IN ITEMS FlingEngine Sandbox)
	file(GLOB_RECURSE _files RELATIVE "${SOURCE_ROOT}"
		"${SOURCE_ROOT}/${_root}/*.cpp"
		"${SOURCE_ROOT}/${_root}/*.cc"
		"${SOURCE_ROOT}/${_root}/*.h"
		"${SOURCE_ROOT}/${_root}/*.hpp"
		"${SOURCE_ROOT}/${_root}/*.inl"
	)
	foreach(_file IN LISTS _files)
		if(_file IN_LIST _exempt_files)
			continue()
		endif()
		set(_skip FALSE)
		foreach(_dir IN LISTS _exempt_dirs)
			string(FIND "${_file}" "${_dir}/" _at)
			if(_at EQUAL 0)
				set(_skip TRUE)
			endif()
		endforeach()
		if(_skip)
			continue()
		endif()
		math(EXPR _checked "${_checked} + 1")
		file(READ "${SOURCE_ROOT}/${_file}" _text)
		fling_find_banned("${_text}" _hits)
		if(_hits)
			list(REMOVE_DUPLICATES _hits)
			list(JOIN _hits ", " _names)
			list(APPEND _problems "${_file}: ${_names}")
		endif()
	endforeach()
endforeach()

if(_problems)
	list(JOIN _problems "\n  " _text)
	message(FATAL_ERROR
		"Simulation code calls math that differs between platforms. Use SimMath (Core/inc/SimMath.h), "
		"or, for presentation-only code, add the file to the exempt list in this script with the reason:\n  ${_text}")
endif()
message(STATUS "No platform math in ${_checked} simulation source files")
