#pragma once
#include "CoreAPI.h"
#include "Platform.h"
#include "Strings/String.h"

#include <string>
#include <sys/stat.h>
#include <sys/types.h>

/** 
 * @see FlingAssetPaths.cpp.in
 */
namespace Fling
{
    /** Static set of useful engine paths and directories */
    struct CORE_API FlingPaths
    {
        /** Returns directory where engine config files are kept */
        static const String& EngineConfigDir();
        
        /** Returns directory where engine assets are kept */
        static const String& EngineAssetsDir();

        /**
         * Returns directory where the build writes compiled SPIR-V shaders.
         * @see CMake/FlingShaders.cmake
         */
        static const String& CompiledShaderDir();

        /** Returns directory where your current binary is */
        static const String& BinaryDir();

        /** Returns directory where engine log files are kept */
        static const String& EngineLogDir();

        /** Returns directory where the engine source files are kept */
        static const String& EngineSourceDir();

        /** The project root. Relative project paths start here. Empty in shipping builds (the working directory). */
        static const String& ProjectDir();

        /**
         * The project's Saved folder, for files the engine writes at runtime (input
         * recordings, caches). It is not in source control.
         */
        static const String& ProjectSavedDir();

        /** An absolute path is returned as it is. A relative path is made relative to ProjectDir. */
        static String ResolveProjectPath(StringView t_Path);

        /**
         * Convert a full absolute path to one relative to the engine assets directory.
         */
        static String ConvertAbsolutePathToRelative(StringView t_FullPath);

        /**
         * Makes a directory given the specified path. 
         * 
         * @param t_Dir     The directory to make
         * @return  0 if successful, error code if not
         */
        static int MakeDir(StringView t_Dir);

        /**
         * Checks if this directory exists
         * 
         * @param t_Dir     The directory to check
         * @return  True if the directory exists 
         */
        static bool DirExists(StringView t_Dir);

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