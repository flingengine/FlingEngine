#pragma once

#include "Strings/String.h"

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

        constexpr StringView RemovePrefix(StringView text, const StringView prefix)
        {
            if (text.starts_with(prefix))
            {
                text.remove_prefix(prefix.size());
            }
            return text;
        }
    } // namespace Detail

    /**
     * The qualified C++ name of T, at compile time: "Fling::Transform". MSVC's
     * "struct " or "class " in front is removed. The text of anonymous namespaces and
     * template arguments is different on each compiler, so use it only to tell types
     * apart in one build, or in messages. Do not save it.
     */
    template <typename T>
    constexpr StringView TypeNameOf()
    {
        const StringView raw = Detail::RawTypeName<T>();
        StringView name =
            raw.substr(Detail::TypeNamePrefix, raw.size() - Detail::TypeNamePrefix - Detail::TypeNameSuffix);
        for (const StringView keyword :
             {StringView("struct "), StringView("class "), StringView("enum "), StringView("union ")})
        {
            name = Detail::RemovePrefix(name, keyword);
        }
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
