#include "DynamicLibrary.h"

#include "Platform.h"

#include <cstring>
#include <limits>
#include <string>
#include <string_view>

#if FLING_WINDOWS

#include <cstdio>
#include <wintrust.h>
#include <softpub.h>

namespace
{
    std::wstring Utf8ToWide(std::string_view t_Text)
    {
        if (t_Text.empty() || t_Text.size() > static_cast<size_t>(std::numeric_limits<int>::max()))
        {
            return {};
        }

        const int Size = static_cast<int>(t_Text.size());
        const int Needed = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, t_Text.data(), Size, nullptr, 0);
        if (Needed <= 0)
        {
            return {};
        }

        std::wstring Wide(static_cast<size_t>(Needed), L'\0');
        const int Written =
            MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, t_Text.data(), Size, Wide.data(), Needed);
        if (Written <= 0)
        {
            return {};
        }
        return Wide;
    }

    std::string WideToUtf8(std::wstring_view t_Text)
    {
        if (t_Text.empty() || t_Text.size() > static_cast<size_t>(std::numeric_limits<int>::max()))
        {
            return {};
        }

        const int Size = static_cast<int>(t_Text.size());
        const int Needed = WideCharToMultiByte(CP_UTF8, 0, t_Text.data(), Size, nullptr, 0, nullptr, nullptr);
        if (Needed <= 0)
        {
            return {};
        }

        std::string Text(static_cast<size_t>(Needed), '\0');
        WideCharToMultiByte(CP_UTF8, 0, t_Text.data(), Size, Text.data(), Needed, nullptr, nullptr);
        return Text;
    }

    std::string WindowsErrorMessage(DWORD t_Code)
    {
        wchar_t* Buffer = nullptr;
        const DWORD Flags = FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS;
        const DWORD Length = FormatMessageW(Flags,
                                            nullptr,
                                            t_Code,
                                            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                                            reinterpret_cast<wchar_t*>(&Buffer),
                                            0,
                                            nullptr);

        std::string Message;
        if (Length > 0 && Buffer != nullptr)
        {
            Message = WideToUtf8(std::wstring_view(Buffer, Length));
            while (!Message.empty() && (Message.back() == '\n' || Message.back() == '\r' || Message.back() == ' '))
            {
                Message.pop_back();
            }
        }
        if (Buffer != nullptr)
        {
            LocalFree(Buffer);
        }
        if (Message.empty())
        {
            Message = "LoadLibraryExW failed (code " + std::to_string(t_Code) + ")";
        }
        return Message;
    }

    bool IsExplicitPath(std::wstring_view t_Name)
    {
        if (t_Name.find(L'\\') != std::wstring_view::npos || t_Name.find(L'/') != std::wstring_view::npos)
        {
            return true;
        }
        if (t_Name.size() >= 2 && t_Name[1] == L':')
        {
            const wchar_t Drive = t_Name[0];
            if ((Drive >= L'A' && Drive <= L'Z') || (Drive >= L'a' && Drive <= L'z'))
            {
                return true;
            }
        }
        return false;
    }

    std::wstring FullPath(const std::wstring& t_Path)
    {
        const DWORD Needed = GetFullPathNameW(t_Path.c_str(), 0, nullptr, nullptr);
        if (Needed == 0)
        {
            return {};
        }

        std::wstring Path(static_cast<size_t>(Needed), L'\0');
        const DWORD Written = GetFullPathNameW(t_Path.c_str(), Needed, Path.data(), nullptr);
        if (Written == 0 || Written >= Needed)
        {
            return {};
        }
        Path.resize(Written);
        return Path;
    }

    std::wstring SystemDirectory()
    {
        const UINT Needed = GetSystemDirectoryW(nullptr, 0);
        if (Needed == 0)
        {
            return {};
        }

        std::wstring Path(static_cast<size_t>(Needed), L'\0');
        const UINT Written = GetSystemDirectoryW(Path.data(), Needed);
        if (Written == 0 || Written >= Needed)
        {
            return {};
        }
        Path.resize(Written);
        return Path;
    }

    std::wstring ExecutableDirectory()
    {
        std::wstring Path(MAX_PATH, L'\0');
        DWORD Written = GetModuleFileNameW(nullptr, Path.data(), static_cast<DWORD>(Path.size()));
        while (Written == Path.size())
        {
            Path.resize(Path.size() * 2);
            Written = GetModuleFileNameW(nullptr, Path.data(), static_cast<DWORD>(Path.size()));
        }
        if (Written == 0)
        {
            return {};
        }

        Path.resize(Written);
        const size_t Slash = Path.find_last_of(L"\\/");
        if (Slash == std::wstring::npos)
        {
            return {};
        }
        return Path.substr(0, Slash);
    }

    std::wstring JoinPath(std::wstring_view t_Directory, std::wstring_view t_Name)
    {
        std::wstring Path;
        Path.reserve(t_Directory.size() + 1 + t_Name.size());
        Path.assign(t_Directory.begin(), t_Directory.end());
        if (!Path.empty() && Path.back() != L'\\' && Path.back() != L'/')
        {
            Path.push_back(L'\\');
        }
        Path.append(t_Name.begin(), t_Name.end());
        return Path;
    }

    enum class Candidate
    {
        Missing,
        Found,
        Failed
    };

    Candidate ProbeCandidate(const std::wstring& t_Path)
    {
        const DWORD Attributes = GetFileAttributesW(t_Path.c_str());
        if (Attributes != INVALID_FILE_ATTRIBUTES)
        {
            return Candidate::Found;
        }

        const DWORD Code = GetLastError();
        if (Code == ERROR_FILE_NOT_FOUND || Code == ERROR_PATH_NOT_FOUND)
        {
            return Candidate::Missing;
        }
        return Candidate::Failed;
    }

    enum class Signature
    {
        Valid,
        NotSigned,
        NotTrusted
    };

    struct SignatureResult
    {
        Signature Status = Signature::NotTrusted;
        LONG Code = 0;
    };

    SignatureResult VerifyAuthenticode(const std::wstring& t_Path)
    {
        WINTRUST_FILE_INFO FileInfo{};
        FileInfo.cbStruct = sizeof(FileInfo);
        FileInfo.pcwszFilePath = t_Path.c_str();

        GUID Action = WINTRUST_ACTION_GENERIC_VERIFY_V2;

        WINTRUST_DATA Trust{};
        Trust.cbStruct = sizeof(Trust);
        Trust.dwUIChoice = WTD_UI_NONE;
        Trust.fdwRevocationChecks = WTD_REVOKE_NONE;
        Trust.dwUnionChoice = WTD_CHOICE_FILE;
        Trust.dwStateAction = WTD_STATEACTION_VERIFY;
        Trust.dwProvFlags = WTD_SAFER_FLAG | WTD_CACHE_ONLY_URL_RETRIEVAL;
        Trust.pFile = &FileInfo;

        const LONG Status = WinVerifyTrust(nullptr, &Action, &Trust);

        Trust.dwStateAction = WTD_STATEACTION_CLOSE;
        WinVerifyTrust(nullptr, &Action, &Trust);

        SignatureResult Result;
        Result.Code = Status;
        if (Status == ERROR_SUCCESS)
        {
            Result.Status = Signature::Valid;
        }
        else if (Status == static_cast<LONG>(TRUST_E_NOSIGNATURE) ||
                 Status == static_cast<LONG>(TRUST_E_SUBJECT_FORM_UNKNOWN) ||
                 Status == static_cast<LONG>(TRUST_E_PROVIDER_UNKNOWN))
        {
            Result.Status = Signature::NotSigned;
        }
        else
        {
            Result.Status = Signature::NotTrusted;
        }
        return Result;
    }

    std::string UntrustedMessage(LONG t_Status)
    {
        char Hex[16] = {};
        std::snprintf(Hex, sizeof(Hex), "%08lX", static_cast<unsigned long>(t_Status));
        return std::string("The shared library signature is not trusted (0x") + Hex + ")";
    }
} // namespace

#elif FLING_LINUX

#include <dlfcn.h>

namespace
{
    std::string LinuxErrorMessage()
    {
        const char* Error = dlerror();
        if (Error == nullptr || Error[0] == '\0')
        {
            return "dlopen failed";
        }
        return Error;
    }
} // namespace

#else

#error DynamicLibrary supports Windows and Linux only

#endif

namespace
{
    void WriteError(std::string* t_Error, std::string t_Message)
    {
        if (t_Error != nullptr)
        {
            *t_Error = std::move(t_Message);
        }
    }

    void ClearError(std::string* t_Error)
    {
        if (t_Error != nullptr)
        {
            t_Error->clear();
        }
    }
} // namespace

namespace Fling
{
    DynamicLibrary::~DynamicLibrary()
    {
        Close();
    }

    DynamicLibrary::DynamicLibrary(DynamicLibrary&& t_Other) noexcept : m_Handle(t_Other.m_Handle)
    {
        t_Other.m_Handle = nullptr;
    }

    DynamicLibrary& DynamicLibrary::operator=(DynamicLibrary&& t_Other) noexcept
    {
        if (this != &t_Other)
        {
            Close();
            m_Handle = t_Other.m_Handle;
            t_Other.m_Handle = nullptr;
        }
        return *this;
    }

    DynamicLibrary DynamicLibrary::Open(std::string_view t_Name, std::string* t_Error)
    {
        if (t_Name.empty())
        {
            WriteError(t_Error, "The shared library name is empty");
            return {};
        }

#if FLING_WINDOWS
        const std::wstring WideName = Utf8ToWide(t_Name);
        if (WideName.empty())
        {
            WriteError(t_Error, "The shared library name is not valid UTF-8");
            return {};
        }

        std::wstring Path;
        if (IsExplicitPath(WideName))
        {
            Path = FullPath(WideName);
            if (Path.empty())
            {
                WriteError(t_Error, WindowsErrorMessage(GetLastError()));
                return {};
            }
            const Candidate Lookup = ProbeCandidate(Path);
            if (Lookup == Candidate::Failed)
            {
                WriteError(t_Error, WindowsErrorMessage(GetLastError()));
                return {};
            }
            if (Lookup != Candidate::Found)
            {
                WriteError(t_Error, "The shared library was not found");
                return {};
            }
        }
        else
        {
            const std::wstring SystemDir = SystemDirectory();
            const std::wstring SystemCandidate = SystemDir.empty() ? std::wstring{} : JoinPath(SystemDir, WideName);
            const Candidate SystemLookup =
                SystemCandidate.empty() ? Candidate::Missing : ProbeCandidate(SystemCandidate);
            if (SystemLookup == Candidate::Failed)
            {
                WriteError(t_Error, WindowsErrorMessage(GetLastError()));
                return {};
            }
            if (SystemLookup == Candidate::Found)
            {
                Path = SystemCandidate;
            }
            else
            {
                const std::wstring ExeDir = ExecutableDirectory();
                const std::wstring ExeCandidate = ExeDir.empty() ? std::wstring{} : JoinPath(ExeDir, WideName);
                const Candidate ExeLookup = ExeCandidate.empty() ? Candidate::Missing : ProbeCandidate(ExeCandidate);
                if (ExeLookup == Candidate::Failed)
                {
                    WriteError(t_Error, WindowsErrorMessage(GetLastError()));
                    return {};
                }
                if (ExeLookup != Candidate::Found)
                {
                    WriteError(t_Error, "The shared library was not found");
                    return {};
                }
                Path = ExeCandidate;
            }
        }

        const SignatureResult Checked = VerifyAuthenticode(Path);
        if (Checked.Status == Signature::NotSigned)
        {
            WriteError(t_Error, "The shared library is not signed");
            return {};
        }
        if (Checked.Status != Signature::Valid)
        {
            WriteError(t_Error, UntrustedMessage(Checked.Code));
            return {};
        }

        // LoadLibraryExW does not open a dialog when the DLL is missing.
        HMODULE Handle =
            LoadLibraryExW(Path.c_str(), nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
        if (Handle == nullptr)
        {
            WriteError(t_Error, WindowsErrorMessage(GetLastError()));
            return {};
        }

        DynamicLibrary Library;
        Library.m_Handle = static_cast<void*>(Handle);
        ClearError(t_Error);
        return Library;

#elif FLING_LINUX
        const std::string Name(t_Name);
        dlerror();
        void* Handle = dlopen(Name.c_str(), RTLD_NOW | RTLD_LOCAL);
        if (Handle == nullptr)
        {
            WriteError(t_Error, LinuxErrorMessage());
            return {};
        }

        DynamicLibrary Library;
        Library.m_Handle = Handle;
        ClearError(t_Error);
        return Library;
#endif
    }

    DynamicLibrary DynamicLibrary::OpenFirst(std::span<const std::string_view> t_Names, std::string* t_Error)
    {
        for (const std::string_view Name : t_Names)
        {
            DynamicLibrary Library = Open(Name, t_Error);
            if (Library)
            {
                return Library;
            }
        }

        if (t_Error != nullptr && t_Error->empty())
        {
            *t_Error = "No shared library name was given";
        }
        return {};
    }

    DynamicLibrary::operator bool() const
    {
        return m_Handle != nullptr;
    }

    void* DynamicLibrary::Symbol(std::string_view t_Name) const
    {
        if (m_Handle == nullptr || t_Name.empty())
        {
            return nullptr;
        }

        const std::string Name(t_Name);

#if FLING_WINDOWS
        const FARPROC Proc = GetProcAddress(static_cast<HMODULE>(m_Handle), Name.c_str());
        void* Address = nullptr;
        static_assert(sizeof(Address) == sizeof(Proc), "FARPROC must be the same size as void*");
        std::memcpy(&Address, &Proc, sizeof(Address));
        return Address;
#elif FLING_LINUX
        return dlsym(m_Handle, Name.c_str());
#endif
    }

    void DynamicLibrary::Close()
    {
        if (m_Handle == nullptr)
        {
            return;
        }

#if FLING_WINDOWS
        FreeLibrary(static_cast<HMODULE>(m_Handle));
#elif FLING_LINUX
        dlclose(m_Handle);
#endif
        m_Handle = nullptr;
    }
} // namespace Fling
