#pragma once

#include <string>
#include <string_view>
#include <sstream>
#include <type_traits>
#include <cstdlib>
#include "FlingTypes.h"

namespace Fling
{
    namespace StringUtils
    {
        /**
        * Attempts to parse the given string_view as an instance of T.
        *
        * This is the single place that knows how to turn a raw string into an engine
        * type (bool, int, float, ...), so that command line args, ini config values,
        * and any other string-serialized data all agree on what "true", "7", or
        * "3.14" mean.
        *
        * @param Str        The string to parse
        * @param Default    Value returned if Str is empty or cannot be converted to T
        * @return The parsed value, or Default if Str could not be parsed as a T
        */
        template<typename T>
        T ParseAs(const std::string_view Str, const T& Default = T{})
        {
            if (Str.empty())
            {
                return Default;
            }

            if constexpr (std::is_same_v<T, std::string>)
            {
                return std::string(Str);
            }
            else if constexpr (std::is_same_v<T, bool>)
            {
                if (Str == "true" || Str == "True" || Str == "TRUE" || Str == "1")
                {
                    return true;
                }
                if (Str == "false" || Str == "False" || Str == "FALSE" || Str == "0")
                {
                    return false;
                }
                return Default;
            }
            else if constexpr (std::is_integral_v<T>)
            {
                const std::string Temp(Str);
                char* End = nullptr;
                const long long Result = std::strtoll(Temp.c_str(), &End, 10);
                return (End != Temp.c_str()) ? static_cast<T>(Result) : Default;
            }
            else if constexpr (std::is_same_v<T, float>)
            {
                const std::string Temp(Str);
                char* End = nullptr;
                const float Result = std::strtof(Temp.c_str(), &End);
                return (End != Temp.c_str()) ? Result : Default;
            }
            else if constexpr (std::is_same_v<T, double>)
            {
                const std::string Temp(Str);
                char* End = nullptr;
                const double Result = std::strtod(Temp.c_str(), &End);
                return (End != Temp.c_str()) ? Result : Default;
            }
            else
            {
                // Fallback for any other stream-extractable type
                std::istringstream ValueStream{ std::string(Str) };
                T Value{};
                ValueStream >> Value;
                return ValueStream.fail() ? Default : Value;
            }
        }
    } // namespace StringUtils
} // namespace Fling
