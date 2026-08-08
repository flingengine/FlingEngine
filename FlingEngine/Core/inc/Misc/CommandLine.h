#pragma once

#include <string> // string, stoi, to_string
#include <string_view>  // std::string_view
#include <unordered_map>
#include "FlingTypes.h"
#include "Misc/StringUtils.h"

// TODO: I think that we may gain a lot if we just use
// Boost. That certainly will have a better implementation then
// I can whip up, and I think has config file options as well.
// https://www.boost.org/doc/libs/1_85_0/doc/html/program_options.html

namespace Fling
{
    /**
    * Holds onto the command line arguments passed to this application
    * Can be used to parse arguments into different types
    * 
    * The syntax for specifying a command line argument is a key-value pair with a "-".
    * For example, to specify the "foo" option with a value of 7, you could use:
    * 
    *   -foo=7
    * 
    * on the command line. 
    */
    class CommandLine
    {
    public:
        /** Maximum length, in characters, that a single command line argument may be. Any argument longer than this is ignored. */
        static constexpr std::size_t MaxArgLength = 256;

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

        /**
         * Gets the value of the given param as the given type.
         *
         * If the value as not specified on the command line,
         * then the given "Default" value will be returned.
         */
        template<typename T>
        T GetValueAs(const std::string_view Param, const T& Default) const;

        // TODO: make this a std::string_view
        const char* GetValueAsString(const std::string_view Param) const;

        [[nodiscard]] std::string_view GetCommandLineData() const;

        /**
         * Loads console variables from the "[ConsoleVariables]" section of an ini-style
         * config file, following the same "Key=Value" syntax used on the command line
         * (e.g. UE's ini config variables). Values loaded this way act as a fallback:
         * if the same key was also passed directly on the command line, the command
         * line value always takes precedence.
         *
         * @param FilePath   Path to the ini file to load
         * @return True if the file was opened and parsed successfully
         */
        bool LoadConfigFile(const std::string& FilePath);

        /**
         * Same as LoadConfigFile, but parses the ini data directly out of the given
         * string rather than from a file on disk. Exposed publicly so that this parsing
         * logic can be unit tested without touching the file system.
         *
         * @param IniContent   Contents of an ini file to parse
         * @return True if the content was parsed successfully
         */
        bool LoadConfigVarsFromString(const std::string_view IniContent);

    private:

        /** Looks up a param, checking command line values before config file (ConsoleVariables) values. */
        const std::string* FindValue(const std::string_view Param) const;

        std::string CurrentCommandLineData;

        /** Key/value pairs parsed out of the command line, e.g. "-foo=7" becomes ParsedArgs["foo"] = "7" */
        std::unordered_map<std::string, std::string> ParsedArgs;

        /** Key/value pairs parsed out of a config file's "[ConsoleVariables]" section. Lower priority than ParsedArgs. */
        std::unordered_map<std::string, std::string> ConfigArgs;
    };

    template<typename T>
    T CommandLine::GetValueAs(const std::string_view Param, const T& Default) const
    {
        const std::string* Value = FindValue(Param);
        if (Value == nullptr)
        {
            // Nothing was set on the command line or in a config file for this param,
            // so use the default value
            return Default;
        }

        return StringUtils::ParseAs<T>(*Value, Default);
    }
}   // namespace Fling