#pragma once

#include "CoreAPI.h"

#include <filesystem>
#include <string>
#include <string_view>

namespace Fling
{
    /**
     * A mutable, owning UTF-8 text buffer. Use it to build, edit, parse, and format
     * text. It has the small string optimization, so short text does not allocate.
     *
     * Use Name for identifiers and DisplayText for text that the player sees.
     */
    using String = std::string;

    /**
     * A non-owning view of UTF-8 text. It is the parameter type for all read-only
     * text: it takes a literal, a String, or part of one, without a copy.
     */
    using StringView = std::string_view;

    /**
     * Makes a file system path from UTF-8 text. On Windows, a path made directly from
     * a std::string reads the text in the ANSI code page, so always use this.
     */
    CORE_API std::filesystem::path PathFromUtf8(StringView t_Text);

    /** The UTF-8 text of a file system path. */
    CORE_API String PathToUtf8(const std::filesystem::path& t_Path);
} // namespace Fling
