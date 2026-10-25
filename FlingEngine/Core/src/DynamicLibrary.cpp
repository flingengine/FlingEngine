#include "DynamicLibrary.h"

#include "Platform.h"

#include <cstring>
#include <limits>
#include <string>
#include <string_view>

#if FLING_WINDOWS

namespace {
    std::wstring Utf8ToWide(std::string_view t_Text) {
        if (t_Text.empty() || t_Text.size() > static_cast<size_t>(std::numeric_limits<int>::max())) {
            return {};
        }

        const int Size = static_cast<int>(t_Text.size());
        const int Needed = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, t_Text.data(), Size, nullptr, 0);
        if (Needed <= 0) {
            return {};
        }

        std::wstring Wide(static_cast<size_t>(Needed), L'\0');
        const int Written =
            MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, t_Text.data(), Size, Wide.data(), Needed);
        if (Written <= 0) {
            return {};
        }
        return Wide;
    }

    std::string WideToUtf8(std::wstring_view t_Text) {
        if (t_Text.empty() || t_Text.size() > static_cast<size_t>(std::numeric_limits<int>::max())) {
            return {};
        }

        const int Size = static_cast<int>(t_Text.size());
        const int Needed = WideCharToMultiByte(CP_UTF8, 0, t_Text.data(), Size, nullptr, 0, nullptr, nullptr);
        if (Needed <= 0) {
            return {};
        }

        std::string Text(static_cast<size_t>(Needed), '\0');
        WideCharToMultiByte(CP_UTF8, 0, t_Text.data(), Size, Text.data(), Needed, nullptr, nullptr);
        return Text;
    }

    std::string WindowsErrorMessage(DWORD t_Code) {
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
        if (Length > 0 && Buffer != nullptr) {
            Message = WideToUtf8(std::wstring_view(Buffer, Length));
            while (!Message.empty() && (Message.back() == '\n' || Message.back() == '\r' || Message.back() == ' ')) {
                Message.pop_back();
            }
        }
        if (Buffer != nullptr) {
            LocalFree(Buffer);
        }
        if (Message.empty()) {
            Message = "LoadLibraryW failed (code " + std::to_string(t_Code) + ")";
        }
        return Message;
    }
} // namespace

#elif FLING_LINUX

#include <dlfcn.h>

namespace {
    std::string LinuxErrorMessage() {
        const char* Error = dlerror();
        if (Error == nullptr || Error[0] == '\0') {
            return "dlopen failed";
        }
        return Error;
    }
} // namespace

#else

#error DynamicLibrary supports Windows and Linux only

#endif

namespace {
    void WriteError(std::string* t_Error, std::string t_Message) {
        if (t_Error != nullptr) {
            *t_Error = std::move(t_Message);
        }
    }

    void ClearError(std::string* t_Error) {
        if (t_Error != nullptr) {
            t_Error->clear();
        }
    }
} // namespace

namespace Fling {
    DynamicLibrary::~DynamicLibrary() {
        Close();
    }

    DynamicLibrary::DynamicLibrary(DynamicLibrary&& t_Other) noexcept : m_Handle(t_Other.m_Handle) {
        t_Other.m_Handle = nullptr;
    }

    DynamicLibrary& DynamicLibrary::operator=(DynamicLibrary&& t_Other) noexcept {
        if (this != &t_Other) {
            Close();
            m_Handle = t_Other.m_Handle;
            t_Other.m_Handle = nullptr;
        }
        return *this;
    }

    DynamicLibrary DynamicLibrary::Open(std::string_view t_Name, std::string* t_Error) {
        if (t_Name.empty()) {
            WriteError(t_Error, "The shared library name is empty");
            return {};
        }

#if FLING_WINDOWS
        const std::wstring WideName = Utf8ToWide(t_Name);
        if (WideName.empty()) {
            WriteError(t_Error, "The shared library name is not valid UTF-8");
            return {};
        }

        HMODULE Handle = LoadLibraryW(WideName.c_str());
        if (Handle == nullptr) {
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
        if (Handle == nullptr) {
            WriteError(t_Error, LinuxErrorMessage());
            return {};
        }

        DynamicLibrary Library;
        Library.m_Handle = Handle;
        ClearError(t_Error);
        return Library;
#endif
    }

    DynamicLibrary DynamicLibrary::OpenFirst(std::span<const std::string_view> t_Names, std::string* t_Error) {
        for (const std::string_view Name : t_Names) {
            DynamicLibrary Library = Open(Name, t_Error);
            if (Library) {
                return Library;
            }
        }

        if (t_Error != nullptr && t_Error->empty()) {
            *t_Error = "No shared library name was given";
        }
        return {};
    }

    DynamicLibrary::operator bool() const {
        return m_Handle != nullptr;
    }

    void* DynamicLibrary::Symbol(std::string_view t_Name) const {
        if (m_Handle == nullptr || t_Name.empty()) {
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

    void DynamicLibrary::Close() {
        if (m_Handle == nullptr) {
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
