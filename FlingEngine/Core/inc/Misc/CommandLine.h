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
        
        /** Sets the static command line based on  */
        static void Set(const std::string& CmdLine);

        /**  */
        static std::string BuildFromArgs(int32 Argc, char* ArgV[]);

        bool Parse(const std::string& InKey);
        
        static const std::string& Get() { return CurrentCommandLine; }
        
    private:

        static std::string CurrentCommandLine;
        
    };
}   // namespace Fling