#pragma once

#if _WIN32

// TODO: Per-platform definitions here
    #ifndef FOUNDATION_API
        #define FOUNDATION_API        __declspec(dllexport) 
    #else					        // Not exporting function
        #define FOUNDATION_API        
    #endif

#endif  // _WIN32

// TODO: make an actual export for this in the CMakeLists.txt file instead of doing it in C++
// Or just make one exports file in C++ and properly do it for each platform
// which means that we should figure a nice way to include different directories based on your platform...
// ah geez
#if __linux__

    #if __GNUC__ >= 4
        #define FOUNDATION_API    __attribute__ ((visibility ("default")))
    #else
        #define FOUNDATION_API
    #endif

#endif	// __linux__