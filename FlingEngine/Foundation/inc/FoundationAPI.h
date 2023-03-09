#pragma once

// TODO: Per-platform definitions here
#ifndef FOUNDATION_API
#		define FOUNDATION_API        __declspec(dllexport) 
#else					        // Not exporting function
#		define FOUNDATION_API        
#endif