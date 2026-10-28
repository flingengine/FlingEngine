#include <catch2/catch_all.hpp>

#include "DynamicLibrary.h"
#include "Platform.h"

#include <cstddef>
#include <cstdio>
#include <span>
#include <string>
#include <string_view>

#if FLING_LINUX
#include <unistd.h>
#endif

namespace
{
#if FLING_LINUX
    constexpr const char* LibraryName = "libc.so.6";
    constexpr const char* SymbolName = "strlen";
#elif FLING_WINDOWS
    constexpr const char* LibraryName = "kernel32.dll";
    constexpr const char* SymbolName = "GetCurrentProcessId";
#else
#error DynamicLibrary tests support Windows and Linux only
#endif

    void CallLoadedSymbol(const Fling::DynamicLibrary& t_Library)
    {
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

#if FLING_WINDOWS
    std::string WideToUtf8(std::wstring_view t_Text)
    {
        if (t_Text.empty())
        {
            return {};
        }
        const int Size = static_cast<int>(t_Text.size());
        const int Needed = WideCharToMultiByte(CP_UTF8, 0, t_Text.data(), Size, nullptr, 0, nullptr, nullptr);
        REQUIRE(Needed > 0);
        std::string Text(static_cast<size_t>(Needed), '\0');
        WideCharToMultiByte(CP_UTF8, 0, t_Text.data(), Size, Text.data(), Needed, nullptr, nullptr);
        return Text;
    }

    std::wstring TempDirectory()
    {
        wchar_t Buffer[MAX_PATH] = {};
        const DWORD Length = GetTempPathW(MAX_PATH, Buffer);
        REQUIRE(Length > 0);
        REQUIRE(Length < MAX_PATH);
        return std::wstring(Buffer, Length);
    }

    void WriteTempBytes(const std::wstring& t_Path)
    {
        const HANDLE File =
            CreateFileW(t_Path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        REQUIRE(File != INVALID_HANDLE_VALUE);
        const char Bytes[] = {'M', 'Z', 0, 0};
        DWORD Written = 0;
        const BOOL Ok = WriteFile(File, Bytes, sizeof(Bytes), &Written, nullptr);
        CloseHandle(File);
        REQUIRE(Ok);
        REQUIRE(Written == sizeof(Bytes));
    }

    /** Delete one file when this guard leaves scope. */
    class DeleteFileGuard
    {
    public:
        explicit DeleteFileGuard(std::wstring t_Path) : m_Path(std::move(t_Path)) {}

        ~DeleteFileGuard() { DeleteFileW(m_Path.c_str()); }

        DeleteFileGuard(const DeleteFileGuard&) = delete;
        DeleteFileGuard& operator=(const DeleteFileGuard&) = delete;

    private:
        std::wstring m_Path;
    };

    /** Delete one empty directory when this guard leaves scope. */
    class DeleteDirectoryGuard
    {
    public:
        explicit DeleteDirectoryGuard(std::wstring t_Path) : m_Path(std::move(t_Path)) {}

        ~DeleteDirectoryGuard() { RemoveDirectoryW(m_Path.c_str()); }

        DeleteDirectoryGuard(const DeleteDirectoryGuard&) = delete;
        DeleteDirectoryGuard& operator=(const DeleteDirectoryGuard&) = delete;

    private:
        std::wstring m_Path;
    };

    /**
     * Restore the process current directory when this guard leaves scope.
     * A failed REQUIRE still runs the destructor.
     */
    class CurrentDirectoryGuard
    {
    public:
        CurrentDirectoryGuard()
        {
            const DWORD Needed = GetCurrentDirectoryW(0, nullptr);
            if (Needed == 0)
            {
                return;
            }
            m_Previous.resize(Needed);
            const DWORD Written = GetCurrentDirectoryW(Needed, m_Previous.data());
            if (Written == 0 || Written >= Needed)
            {
                m_Previous.clear();
                return;
            }
            m_Previous.resize(Written);
        }

        ~CurrentDirectoryGuard()
        {
            if (!m_Previous.empty())
            {
                SetCurrentDirectoryW(m_Previous.c_str());
            }
        }

        CurrentDirectoryGuard(const CurrentDirectoryGuard&) = delete;
        CurrentDirectoryGuard& operator=(const CurrentDirectoryGuard&) = delete;

        /** True when the previous directory was saved. */
        bool Captured() const { return !m_Previous.empty(); }

    private:
        std::wstring m_Previous;
    };
#endif
} // namespace

TEST_CASE("DynamicLibrary", "[core][DynamicLibrary]")
{
    SECTION("Open and call a symbol")
    {
        std::string Error = "stale";
        Fling::DynamicLibrary Library = Fling::DynamicLibrary::Open(LibraryName, &Error);
        REQUIRE(Error.empty());
        CallLoadedSymbol(Library);
        REQUIRE(Library.Symbol("fling_no_such_symbol_42") == nullptr);
        REQUIRE(static_cast<bool>(Library));
    }

    SECTION("A missing library returns an empty object and an error")
    {
        std::string Error;
        Fling::DynamicLibrary Library = Fling::DynamicLibrary::Open("fling_no_such_library_42", &Error);
        REQUIRE_FALSE(static_cast<bool>(Library));
        REQUIRE_FALSE(Error.empty());
        REQUIRE(Library.Symbol(SymbolName) == nullptr);

        Fling::DynamicLibrary NoError = Fling::DynamicLibrary::Open("fling_no_such_library_42", nullptr);
        REQUIRE_FALSE(static_cast<bool>(NoError));
    }

    SECTION("OpenFirst skips a bad name")
    {
        const std::string_view Names[] = {"fling_no_such_library_42", LibraryName};
        std::string Error = "stale";
        Fling::DynamicLibrary Library = Fling::DynamicLibrary::OpenFirst(Names, &Error);
        REQUIRE(Error.empty());
        CallLoadedSymbol(Library);
    }

    SECTION("Move leaves the source empty")
    {
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

    SECTION("Close then Symbol returns null")
    {
        Fling::DynamicLibrary Library = Fling::DynamicLibrary::Open(LibraryName);
        CallLoadedSymbol(Library);
        Library.Close();
        REQUIRE_FALSE(static_cast<bool>(Library));
        REQUIRE(Library.Symbol(SymbolName) == nullptr);
        Library.Close();
        REQUIRE(Library.Symbol(SymbolName) == nullptr);
    }

    SECTION("Destructor unloads a moved library")
    {
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

#if FLING_WINDOWS
    SECTION("An unsigned absolute path is rejected")
    {
        const std::wstring Path =
            TempDirectory() + L"fling_unsigned_" + std::to_wstring(GetCurrentProcessId()) + L".dll";
        const DeleteFileGuard RemoveFile(Path);
        WriteTempBytes(Path);

        std::string Error;
        Fling::DynamicLibrary Signed = Fling::DynamicLibrary::OpenSigned(WideToUtf8(Path), &Error);
        REQUIRE_FALSE(static_cast<bool>(Signed));
        REQUIRE(Error.find("not signed") != std::string::npos);

        std::string OpenError;
        Fling::DynamicLibrary Unchecked = Fling::DynamicLibrary::Open(WideToUtf8(Path), &OpenError);
        REQUIRE_FALSE(static_cast<bool>(Unchecked));
        REQUIRE(OpenError.find("not signed") == std::string::npos);
        REQUIRE(OpenError.find("not trusted") == std::string::npos);
    }

    SECTION("A bare name does not search the current directory")
    {
        const DWORD ProcessId = GetCurrentProcessId();
        const std::wstring Dir = TempDirectory() + L"fling_dynlib_cwd_" + std::to_wstring(ProcessId);
        const BOOL Created = CreateDirectoryW(Dir.c_str(), nullptr);
        REQUIRE((Created != FALSE || GetLastError() == ERROR_ALREADY_EXISTS));
        const DeleteDirectoryGuard RemoveDir(Dir);

        const std::wstring Bare = L"fling_planted_" + std::to_wstring(ProcessId) + L".dll";
        const DeleteFileGuard RemoveFile(Dir + L"\\" + Bare);
        WriteTempBytes(Dir + L"\\" + Bare);

        const CurrentDirectoryGuard Cwd;
        REQUIRE(Cwd.Captured());
        REQUIRE(SetCurrentDirectoryW(Dir.c_str()));

        std::string Error;
        Fling::DynamicLibrary Library = Fling::DynamicLibrary::OpenSigned(WideToUtf8(Bare), &Error);
        REQUIRE_FALSE(static_cast<bool>(Library));
        REQUIRE(Error.find("not signed") == std::string::npos);
    }
#endif

#if FLING_LINUX
    SECTION("OpenSigned calls Open")
    {
        std::string Error = "stale";
        Fling::DynamicLibrary Library = Fling::DynamicLibrary::OpenSigned(LibraryName, &Error);
        REQUIRE(Error.empty());
        CallLoadedSymbol(Library);
    }

    SECTION("An absolute path to a text file fails")
    {
        const std::string Path = "/tmp/fling_dynlib_" + std::to_string(static_cast<long>(::getpid())) + ".txt";
        struct RemoveFile
        {
            std::string Path;
            ~RemoveFile() { std::remove(Path.c_str()); }
        } const Guard{Path};

        FILE* File = std::fopen(Path.c_str(), "wb");
        REQUIRE(File != nullptr);
        const char Bytes[] = "not a shared library\n";
        const std::size_t Written = std::fwrite(Bytes, 1, sizeof(Bytes) - 1, File);
        std::fclose(File);
        REQUIRE(Written == sizeof(Bytes) - 1);

        std::string Error;
        Fling::DynamicLibrary Library = Fling::DynamicLibrary::Open(Path, &Error);
        REQUIRE_FALSE(static_cast<bool>(Library));
        REQUIRE_FALSE(Error.empty());
    }
#endif
}
