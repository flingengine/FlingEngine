#pragma once

#include "FlingTypes.h"
#include "Strings/String.h"

#include <charconv>
#include <cstdlib>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>

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
        T ParseAs(const StringView Str, const T& Default = T{})
        {
            if (Str.empty())
            {
                return Default;
            }

            if constexpr (std::is_same_v<T, String>)
            {
                return String(Str);
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
            else if constexpr (std::is_integral_v<T> || std::is_floating_point_v<T>)
            {
                // std::from_chars does not use the locale, so "3.14" reads the same on
                // every machine. Like strtoll, skip leading spaces and a '+' sign, and
                // read the number at the start ("776abc" reads 776).
                StringView Text = Str;
                while (!Text.empty() && (Text.front() == ' ' || Text.front() == '\t'))
                {
                    Text.remove_prefix(1);
                }
                if (!Text.empty() && Text.front() == '+')
                {
                    Text.remove_prefix(1);
                }

                T Result {};
                const std::from_chars_result Parsed = std::from_chars(Text.data(), Text.data() + Text.size(), Result);
                return Parsed.ec == std::errc() ? Result : Default;
            }
            else
            {
                // Fallback for any other stream-extractable type
                std::istringstream ValueStream{ String(Str) };
                T Value{};
                ValueStream >> Value;
                return ValueStream.fail() ? Default : Value;
            }
        }
    } // namespace StringUtils
} // namespace Fling
