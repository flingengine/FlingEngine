#pragma once
#include <sys/types.h>
#include <sys/stat.h>

/** 
 * @see FlingAssetPaths.cpp.in
 */
namespace Fling
{
    /** Static set of useful engine paths and directories */
    struct FLING_API FlingPaths
    {
        /** Returns directory where engine config files are kept */
        static std::string EngineConfigDir();
        
        /** Returns directory where engine assets are kept */
        static std::string EngineAssetsDir();
        
        /** Returns directory where your current binary is */
        static std::string BinaryDir();

        /** Returns directory where engine log files are kept */
        static std::string EngineLogDir();

        /** Returns directory where the engine source files are kept */
        static std::string EngineSourceDir(); 

        /**
         * Makes a directory given the specified path. 
         * 
         * @param t_Dir     The directory to make
         * @return  0 if successful, error code if not
         */
        static int MakeDir(const char* t_Dir);

        /**
         * Checks if this directory exists
         * 
         * @param t_Dir     The directory to check
         * @return  True if the directory exists 
         */
        static bool DirExists(const char* t_Dir);

        /**
         * Get the current working directory
         * 
         * @param t_OutBuf      The out buffer
         * @param t_BufSize     Size of the out buffer
         */
        static void GetCurrentWorkingDir(char* t_OutBuf, size_t t_BufSize);

        // TODO: Add options to get the current user's home path
    };
}   // namespace Fling