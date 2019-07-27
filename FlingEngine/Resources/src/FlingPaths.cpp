#include "pch.h"
#include "FlingPaths.h"

namespace Fling
{
    std::string FlingPaths::EngineConfigDir()
    {
        return "Config";
    }

    std::string FlingPaths::EngineAssetsDir()
    {
        return "Assets";
    }

    std::string FlingPaths::BinaryDir()
    {
        return "";
    }
    
    std::string FlingPaths::EngineLogDir()
    {
        return "Logs";
    }
    
    std::string FlingPaths::EngineSourceDir()
    {
        return "";
    }

    int FlingPaths::MakeDir(const char* t_Dir)
    {
        int Res = 0;

#ifdef FLING_WINDOWS
        Res = _mkdir(t_Dir);
#else
        Res = mkdir(t_Dir, 0755);
#endif

        return Res;
    }

    bool FlingPaths::DirExists(const char* t_Dir)
    {
        struct stat info;

        int statRC = stat( t_Dir, &info );
        if( statRC != 0 )
        {
            if (errno == ENOENT)  { return false; } // something along the path does not exist
            if (errno == ENOTDIR) { return false; } // something in path prefix is not a dir
            return false;
        }

        return ( info.st_mode & S_IFDIR ) ? 1 : 0;        
    }

    void FlingPaths::GetCurrentWorkingDir(char* t_OutBuf, size_t t_BufSize)
    {
#if defined(FLING_WINDOWS)
        {
            // Get the real, full path to this executable, end the string before
            // the filename itself and then set that as the current directory
            GetModuleFileName(0, t_OutBuf, t_BufSize);
            char* lastSlash = strrchr(t_OutBuf, '\\');
            if (lastSlash)
            {
                *lastSlash = 0; // End the string at the last slash character
                SetCurrentDirectory(t_OutBuf);
            }
        }
#elif defined(FLING_LINUX)
        {
            if (getcwd(t_OutBuf, t_BufSize) != nullptr) 
            {
                F_LOG_TRACE("Current working dir: {}\n", t_OutBuf);
            }
            else 
            {
                F_LOG_FATAL("getcwd() error");
            }

            if (chdir(t_OutBuf) == -1) 
            {
                F_LOG_FATAL("chdir() error :");
            }
        }
#endif
    }
}   // namespace Fling 