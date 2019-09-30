#pragma once

#if FLING_WINDOWS

#	define FLING_NOT_EXPORTED

#	ifdef FLING_EXPORT		        // Export functions
#		define FLING_API        __declspec(dllexport) 
#       define FLING_EXTERN      
#	else					        // Not exporting function
#		define FLING_API        
#       define FLING_EXTERN        extern
#	endif

#endif	// FLING_WINDOWS