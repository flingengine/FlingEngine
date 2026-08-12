#include "pch.h"
#include "FlingPaths.h"

namespace Fling
{

    int FlingPaths::MakeDir(const char* t_Dir)
    {
        int Res = 0;

#if FLING_WINDOWS
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
        // Normalize CWD to the directory that contains this executable.
        // Shipping builds use relative paths (Config/, Assets/, Logs/), so
        // loading must not depend on whichever directory the process was
        // launched from (IDE vs shell vs CI).
#if FLING_WINDOWS
        {
            GetModuleFileName(0, t_OutBuf, static_cast<DWORD>(t_BufSize));
            char* lastSlash = strrchr(t_OutBuf, '\\');
            if (lastSlash)
            {
                *lastSlash = 0; // End the string at the last slash character
                SetCurrentDirectory(t_OutBuf);
            }
        }
#elif FLING_LINUX
        {
            const ssize_t Len = readlink("/proc/self/exe", t_OutBuf, t_BufSize - 1);
            if (Len == -1)
            {
                F_LOG_FATAL("readlink(/proc/self/exe) error");
                return;
            }

            t_OutBuf[Len] = '\0';
            char* lastSlash = strrchr(t_OutBuf, '/');
            if (lastSlash)
            {
                *lastSlash = 0;
            }

            if (chdir(t_OutBuf) == -1)
            {
                F_LOG_FATAL("chdir() error");
                return;
            }

            F_LOG_TRACE("Current working dir: {}", t_OutBuf);
        }
#endif	// FLING_LINUX
    }
}   // namespace Fling 