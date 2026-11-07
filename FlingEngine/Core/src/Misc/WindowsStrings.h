#pragma once

#include "Platform.h"
#include "Strings/String.h"

#if FLING_WINDOWS

#include <limits>
#include <string>
#include <string_view>

namespace Fling
{
    /** UTF-8 to UTF-16 for wide Windows calls. Return an empty string when t_Text is not valid UTF-8. */
    inline std::wstring Utf8ToWide(StringView t_Text)
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

    /** UTF-16 from a wide Windows call to UTF-8. */
    inline String WideToUtf8(std::wstring_view t_Text)
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

        String Text(static_cast<size_t>(Needed), '\0');
        WideCharToMultiByte(CP_UTF8, 0, t_Text.data(), Size, Text.data(), Needed, nullptr, nullptr);
        return Text;
    }
} // namespace Fling

#endif // FLING_WINDOWS
