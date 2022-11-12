#pragma once

// TODO: Per-platform definitions here
#ifndef FOUNDATION_API
#		define FOUNDATION_API        __declspec(dllexport) 
#       define FLING_EXTERN      
#else					        // Not exporting function
#		define FOUNDATION_API        
#       define FOUNDATION_EXTERN        extern
#endif