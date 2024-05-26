#pragma once

#include <string> // string, stoi, to_string
#include <string_view>  // std::string_view
#include "FlingTypes.h"

// TODO: I think that we may gain a lot if we just use
// Boost. That certainly will have a better implementation then
// I can whip up, and I think has config file options as well.
// https://www.boost.org/doc/libs/1_85_0/doc/html/program_options.html

namespace Fling
{
    /**
    * Holds onto the command line arguments passed to this application
    * Can be used to parse arguments into different types
    */
    class CommandLine
    {
    public:
        /**
         * @return Instance of the current command line that the application was started with.
         */
        static CommandLine& Get();

        /** 
        * Initalize the command line instance with the given application args.
        * This will initalize the command line's internal data structure for keepting
        * track of the data passed into the command line
        *
        * @paran ArgC   The number of command line arguements provided
        * @param ArgV   The char values of those command line arguments
        * @return True if successfully initalized
        */
        bool Init(const int32 Argc, const char* ArgV[]);

        /**
         * Returns true if the given param had a value passed in via command line
         * @param Param
         * @return
         */
        [[nodiscard]] bool HasParam(const std::string_view Param) const;

        std::string_view GetCommandLineData() const;

    private:

        std::string CurrentCommandLineData;

        // TODO: A TMap of string_view's to some generic data container type
        // which we can use for quick checking of flags.
        
    };
}   // namespace Fling