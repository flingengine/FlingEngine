#include "Strings/String.h"

namespace Fling
{
    std::filesystem::path PathFromUtf8(const StringView t_Text)
    {
        // char8_t is used only here, at the file system boundary: a path made from
        // char8_t text reads it as UTF-8 on every platform.
        const std::u8string Utf8(reinterpret_cast<const char8_t*>(t_Text.data()), t_Text.size());
        return std::filesystem::path(Utf8);
    }

    String PathToUtf8(const std::filesystem::path& t_Path)
    {
        const std::u8string Utf8 = t_Path.u8string();
        return String(reinterpret_cast<const char*>(Utf8.data()), Utf8.size());
    }
} // namespace Fling
