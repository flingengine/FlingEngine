#pragma once

#include "CoreAPI.h"

#include <span>
#include <string>
#include <string_view>

namespace Fling
{
    /**
     * Load and unload one shared library.
     *
     * This is the only Fling type that calls LoadLibrary or dlopen.
     * Move a DynamicLibrary. Do not copy it.
     *
     * Open loads a library with the platform search path.
     * It does not check a signature. Use it for game modules and plugins.
     * OpenSigned is the strict open. On Windows it checks Authenticode
     * before the load. Linux has no Authenticode check, so OpenSigned
     * calls Open there.
     */
    class CORE_API DynamicLibrary
    {
    public:
        DynamicLibrary() = default;

        /** Unload the library when this object is still open. */
        ~DynamicLibrary();

        DynamicLibrary(const DynamicLibrary&) = delete;
        DynamicLibrary& operator=(const DynamicLibrary&) = delete;

        DynamicLibrary(DynamicLibrary&& t_Other) noexcept;
        DynamicLibrary& operator=(DynamicLibrary&& t_Other) noexcept;

        /**
         * Open the shared library t_Name.
         *
         * An empty object means that the load failed. When t_Error is not
         * null, write the platform message there on failure. Clear t_Error
         * on success.
         */
        static DynamicLibrary Open(std::string_view t_Name, std::string* t_Error = nullptr);

        /**
         * Open t_Name only when Windows trusts its Authenticode signature.
         *
         * A valid signature is embedded or catalog. A bare name is resolved
         * in System32, then in the executable directory. The current
         * directory and PATH are not searched. An explicit path contains a
         * slash, a backslash, or a drive prefix. That path is verified and
         * loaded alone. A missing file sets "The shared library was not found".
         * On Linux this calls Open.
         */
        static DynamicLibrary OpenSigned(std::string_view t_Name, std::string* t_Error = nullptr);

        /**
         * Open the first library in t_Names that loads.
         *
         * When every name fails and t_Error is not null, write the last
         * platform message.
         */
        static DynamicLibrary OpenFirst(std::span<const std::string_view> t_Names, std::string* t_Error = nullptr);

        /** True when a library is open. */
        explicit operator bool() const;

        /**
         * Return the address of t_Name.
         * Return null when the library is closed or the symbol is missing.
         */
        void* Symbol(std::string_view t_Name) const;

        /**
         * Return the address of t_Name as T_Fn.
         * Return null when the library is closed or the symbol is missing.
         */
        template <typename T_Fn>
        T_Fn SymbolAs(std::string_view t_Name) const
        {
            return reinterpret_cast<T_Fn>(Symbol(t_Name));
        }

        /** Close the library. A second call does nothing. */
        void Close();

    private:
        void* m_Handle = nullptr;
    };
} // namespace Fling
