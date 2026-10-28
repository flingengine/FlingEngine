# Fail when the Sandbox binary imports the Vulkan loader.
#
#   cmake -DBINARY=<path to Sandbox> -P ExpectNoVulkanImport.cmake
#
# Linux reads DT_NEEDED with readelf. Windows reads the PE import table with
# dumpbin /IMPORTS, objdump -p, or pe_imports.py.

cmake_minimum_required(VERSION 3.21)

if(NOT DEFINED BINARY)
	message(FATAL_ERROR "Set -DBINARY= to the Sandbox executable")
endif()
if(NOT EXISTS "${BINARY}")
	message(FATAL_ERROR "Sandbox binary not found: ${BINARY}")
endif()

function(fling_reject_vulkan_imports _text)
	string(TOLOWER "${_text}" _lower)
	if(_lower MATCHES "libvulkan" OR _lower MATCHES "vulkan-1\\.dll")
		message(FATAL_ERROR "Sandbox imports the Vulkan loader:\n${_text}")
	endif()
endfunction()

if(CMAKE_HOST_WIN32 OR BINARY MATCHES "\\.[Ee][Xx][Ee]$")
	set(_import_text "")
	set(_found FALSE)

	find_program(OBJDUMP_EXE NAMES objdump llvm-objdump)
	if(OBJDUMP_EXE)
		execute_process(
			COMMAND "${OBJDUMP_EXE}" -p "${BINARY}"
			RESULT_VARIABLE _rc
			OUTPUT_VARIABLE _out
			ERROR_VARIABLE _err
		)
		if(_rc EQUAL 0)
			string(REGEX MATCHALL "[^\n]*DLL Name:[^\n]*" _lines "${_out}")
			string(JOIN "\n" _import_text ${_lines})
			set(_found TRUE)
		endif()
	endif()

	if(NOT _found)
		find_program(DUMPBIN_EXE NAMES dumpbin)
		if(NOT DUMPBIN_EXE)
			file(GLOB _dumpbins
				"C:/Program Files/Microsoft Visual Studio/2022/*/VC/Tools/MSVC/*/bin/Hostx64/x64/dumpbin.exe"
				"C:/Program Files (x86)/Microsoft Visual Studio/2022/*/VC/Tools/MSVC/*/bin/Hostx64/x64/dumpbin.exe"
			)
			if(_dumpbins)
				list(GET _dumpbins 0 DUMPBIN_EXE)
			endif()
		endif()
		if(DUMPBIN_EXE)
			execute_process(
				COMMAND "${DUMPBIN_EXE}" /IMPORTS "${BINARY}"
				RESULT_VARIABLE _rc
				OUTPUT_VARIABLE _out
				ERROR_VARIABLE _err
			)
			if(NOT _rc EQUAL 0)
				message(FATAL_ERROR "dumpbin /IMPORTS failed (${_rc}): ${_err}")
			endif()
			set(_import_text "${_out}")
			set(_found TRUE)
		endif()
	endif()

	if(NOT _found)
		find_program(PYTHON_EXE NAMES python3 python)
		if(NOT PYTHON_EXE)
			message(FATAL_ERROR "Need objdump, dumpbin, or python to read the PE import table")
		endif()
		execute_process(
			COMMAND "${PYTHON_EXE}" "${CMAKE_CURRENT_LIST_DIR}/pe_imports.py" "${BINARY}"
			RESULT_VARIABLE _rc
			OUTPUT_VARIABLE _out
			ERROR_VARIABLE _err
		)
		if(NOT _rc EQUAL 0)
			message(FATAL_ERROR "pe_imports.py failed (${_rc}): ${_err}")
		endif()
		set(_import_text "${_out}")
	endif()

	if(_import_text STREQUAL "")
		message(FATAL_ERROR "The PE import reader returned no DLL names")
	endif()
	fling_reject_vulkan_imports("${_import_text}")
	message(STATUS "Sandbox PE imports do not include vulkan-1.dll")
else()
	find_program(READELF_EXE NAMES readelf llvm-readelf)
	if(NOT READELF_EXE)
		message(FATAL_ERROR "readelf is required to check that Sandbox does not import libvulkan")
	endif()
	execute_process(
		COMMAND "${READELF_EXE}" -d "${BINARY}"
		RESULT_VARIABLE _rc
		OUTPUT_VARIABLE _out
		ERROR_VARIABLE _err
	)
	if(NOT _rc EQUAL 0)
		message(FATAL_ERROR "readelf -d failed (${_rc}): ${_err}")
	endif()
	string(REGEX MATCHALL "[^\n]*\\(NEEDED\\)[^\n]*" _needed "${_out}")
	if(NOT _needed)
		message(FATAL_ERROR "readelf reported no DT_NEEDED entries for ${BINARY}")
	endif()
	string(JOIN "\n" _needed_text ${_needed})
	fling_reject_vulkan_imports("${_needed_text}")
	message(STATUS "Sandbox DT_NEEDED does not include libvulkan")
endif()
