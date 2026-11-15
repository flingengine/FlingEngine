#pragma once

#include "Strings/String.h"

#include <cstddef>
#include <initializer_list>

namespace Fling
{
    namespace Detail
    {
        /** The compiler's name for this function. It contains the name of T. */
        template <typename T>
        constexpr StringView RawTypeName()
        {
#if defined(_MSC_VER) && !defined(__clang__)
            return __FUNCSIG__;
#else
            return __PRETTY_FUNCTION__;
#endif
        }

        // The text before and after T in RawTypeName, found from a known type. "double"
        // does not occur anywhere else in the function name on any of the compilers.
        inline constexpr StringView TypeNameProbe = RawTypeName<double>();
        inline constexpr size_t TypeNamePrefix = TypeNameProbe.find("double");
        inline constexpr size_t TypeNameSuffix = TypeNameProbe.size() - TypeNamePrefix - StringView("double").size();
        static_assert(TypeNamePrefix != StringView::npos, "This compiler's function names are not supported");

        constexpr bool IsNameChar(const char c)
        {
            return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
        }

        /**
         * The length of the "class ", "struct ", "enum ", or "union " key that starts at
         * index i of a C++ type name, or 0. Only a whole word counts, so the "class " at
         * the end of "Subclass " is not a key.
         */
        constexpr size_t ElaboratedKeyAt(const StringView name, const size_t i)
        {
            if (i > 0 && IsNameChar(name[i - 1]))
            {
                return 0;
            }
            for (const StringView key :
                 {StringView("class "), StringView("struct "), StringView("enum "), StringView("union ")})
            {
                if (name.substr(i).starts_with(key))
                {
                    return key.size();
                }
            }
            return 0;
        }
    } // namespace Detail

    /**
     * The qualified C++ name of T, at compile time: "Fling::Transform". MSVC's
     * "struct ", "class ", "enum " or "union " in front is removed (Detail::ElaboratedKeyAt).
     * The text of anonymous namespaces and template arguments is different on each
     * compiler, so use it only to tell types apart in one build, or in messages. Do not
     * save it.
     */
    template <typename T>
    constexpr StringView TypeNameOf()
    {
        const StringView raw = Detail::RawTypeName<T>();
        StringView name =
            raw.substr(Detail::TypeNamePrefix, raw.size() - Detail::TypeNamePrefix - Detail::TypeNameSuffix);
        name.remove_prefix(Detail::ElaboratedKeyAt(name, 0));
        return name;
    }

    /**
     * The name of T with no namespace, enclosing class, or template arguments, at
     * compile time: "Transform" for Fling::Transform, and "Box" for Game::Box<int>.
     * It is the same on every compiler.
     */
    template <typename T>
    constexpr StringView ShortTypeNameOf()
    {
        StringView name = TypeNameOf<T>();
        name = name.substr(0, name.find('<'));
        const size_t scope = name.rfind("::");
        return scope == StringView::npos ? name : name.substr(scope + 2);
    }
} // namespace Fling
