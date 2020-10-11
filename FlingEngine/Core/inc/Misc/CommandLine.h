#pragma once

#include <string> // string, stoi, to_string
#include "FlingTypes.h"

namespace Fling
{
    /**
    * Holds onto the command line arguments passed to this application
    * Can be used to parse arguments into different types
    */
    class CommandLine
    {
    public:
        
        /** Sets the static command line */
        static void Set(const std::string& CmdLine);

        /** 
        * Builds a string with a space in between each argument passed in via the command
        * except for the first argument (the application name)
        */
        static std::string BuildFromArgs(int32 Argc, char* ArgV[]);

        static bool Parse(const std::string& InKey);
        
        static const std::string& Get() { return CurrentCommandLine; }

        /** Returns true if the given flag is set on the command line */
        static bool HasFlag(const std::string& Flag);
        
        static bool HasParam(const std::string& Param);

    private:

        static std::string CurrentCommandLine;
        
    };
}   // namespace Fling