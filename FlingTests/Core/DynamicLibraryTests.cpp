#include <catch2/catch_all.hpp>

#include "DynamicLibrary.h"
#include "Platform.h"

#include <cstddef>
#include <span>
#include <string>
#include <string_view>

namespace {
#if FLING_LINUX
    constexpr const char* LibraryName = "libc.so.6";
    constexpr const char* SymbolName = "strlen";
#elif FLING_WINDOWS
    constexpr const char* LibraryName = "kernel32.dll";
    constexpr const char* SymbolName = "GetCurrentProcessId";
#else
#error DynamicLibrary tests support Windows and Linux only
#endif

    void CallLoadedSymbol(const Fling::DynamicLibrary& t_Library) {
        REQUIRE(static_cast<bool>(t_Library));
        REQUIRE(t_Library.Symbol(SymbolName) != nullptr);

#if FLING_LINUX
        auto* Fn = t_Library.SymbolAs<std::size_t (*)(const char*)>(SymbolName);
        REQUIRE(Fn != nullptr);
        REQUIRE(Fn("abcd") == 4);
#elif FLING_WINDOWS
        auto* Fn = t_Library.SymbolAs<unsigned long(__stdcall*)()>(SymbolName);
        REQUIRE(Fn != nullptr);
        REQUIRE(Fn() != 0);
#endif
    }
} // namespace

TEST_CASE("DynamicLibrary", "[core][DynamicLibrary]") {
    SECTION("Open and call a symbol") {
        std::string Error = "stale";
        Fling::DynamicLibrary Library = Fling::DynamicLibrary::Open(LibraryName, &Error);
        REQUIRE(Error.empty());
        CallLoadedSymbol(Library);
        REQUIRE(Library.Symbol("fling_no_such_symbol_42") == nullptr);
        REQUIRE(static_cast<bool>(Library));
    }

    SECTION("A missing library returns an empty object and an error") {
        std::string Error;
        Fling::DynamicLibrary Library = Fling::DynamicLibrary::Open("fling_no_such_library_42", &Error);
        REQUIRE_FALSE(static_cast<bool>(Library));
        REQUIRE_FALSE(Error.empty());
        REQUIRE(Library.Symbol(SymbolName) == nullptr);

        Fling::DynamicLibrary NoError = Fling::DynamicLibrary::Open("fling_no_such_library_42", nullptr);
        REQUIRE_FALSE(static_cast<bool>(NoError));
    }

    SECTION("OpenFirst skips a bad name") {
        const std::string_view Names[] = {"fling_no_such_library_42", LibraryName};
        std::string Error = "stale";
        Fling::DynamicLibrary Library = Fling::DynamicLibrary::OpenFirst(Names, &Error);
        REQUIRE(Error.empty());
        CallLoadedSymbol(Library);
    }

    SECTION("Move leaves the source empty") {
        Fling::DynamicLibrary Source = Fling::DynamicLibrary::Open(LibraryName);
        REQUIRE(static_cast<bool>(Source));

        Fling::DynamicLibrary Dest = std::move(Source);
        REQUIRE(static_cast<bool>(Dest));
        REQUIRE_FALSE(static_cast<bool>(Source));
        REQUIRE(Source.Symbol(SymbolName) == nullptr);
        CallLoadedSymbol(Dest);

        Fling::DynamicLibrary Second = Fling::DynamicLibrary::Open(LibraryName);
        Second = std::move(Dest);
        REQUIRE(static_cast<bool>(Second));
        REQUIRE_FALSE(static_cast<bool>(Dest));
        REQUIRE(Dest.Symbol(SymbolName) == nullptr);
    }

    SECTION("Close then Symbol returns null") {
        Fling::DynamicLibrary Library = Fling::DynamicLibrary::Open(LibraryName);
        CallLoadedSymbol(Library);
        Library.Close();
        REQUIRE_FALSE(static_cast<bool>(Library));
        REQUIRE(Library.Symbol(SymbolName) == nullptr);
        Library.Close();
        REQUIRE(Library.Symbol(SymbolName) == nullptr);
    }

    SECTION("Destructor unloads a moved library") {
        Fling::DynamicLibrary Outer = Fling::DynamicLibrary::Open(LibraryName);
        REQUIRE(static_cast<bool>(Outer));
        {
            Fling::DynamicLibrary Inner = std::move(Outer);
            REQUIRE_FALSE(static_cast<bool>(Outer));
            CallLoadedSymbol(Inner);
        }
        REQUIRE_FALSE(static_cast<bool>(Outer));
        REQUIRE(Outer.Symbol(SymbolName) == nullptr);
    }
}
