#pragma once

#include "CoreAPI.h"
#include "spdlog/fmt/fmt.h"
#include "Strings/Name.h"
#include "Strings/String.h"

#include <cstdint>
#include <initializer_list>
#include <memory>

namespace Fling
{
    struct DisplayTextArg;

    /**
     * Finds the text of a string table entry in the current language. Returns false if
     * there is no entry. The localization service (Resources) sets it at start-up, so
     * Core does not depend on Resources.
     */
    using DisplayTextTableResolver = bool (*)(Name t_Table, Name t_Key, String& t_OutText);

    /** Sets the resolver that DisplayText uses for table entries. nullptr removes it. */
    CORE_API void SetDisplayTextTableResolver(DisplayTextTableResolver t_Resolver);

    /** The direction in which a language is read. */
    enum class TextDirection : std::uint8_t
    {
        LeftToRight,
        RightToLeft,
    };

    /**
     * Sets the direction of the current language. The localization service sets it
     * when the language changes. UI reads it to mirror its layout.
     */
    CORE_API void SetDisplayTextDirection(TextDirection t_Direction);

    /** The direction of the current language. LeftToRight until a language sets it. */
    CORE_API TextDirection GetDisplayTextDirection();

    /**
     * Text that the player sees. UI code takes only this type for visible text.
     *
     * A DisplayText keeps its source, not its result: a string table key, a text that
     * must not be translated, or a pattern and its arguments. Resolve makes the text in
     * the current language, so the text changes when the language changes.
     *
     * There is no conversion from String or from a literal, so text in code cannot
     * become visible text by accident. There is no operator==, because the text depends
     * on the language. Compare the Name keys instead.
     *
     * Copies share the same immutable data, so a copy does not allocate.
     */
    class CORE_API DisplayText
    {
    public:
        /** Empty text. */
        DisplayText() = default;

        /**
         * The entry t_Key of the string table t_Table. It resolves to the entry in the
         * current language, then in the source language. If no table has the entry, it
         * resolves to t_SourceText, or to the text of t_Key when t_SourceText is empty.
         *
         * @param t_SourceText  The text in the source language, written where the text
         *                      is used. It is the fallback, so text still shows before
         *                      the tables have the entry.
         */
        static DisplayText FromTable(Name t_Table, Name t_Key, StringView t_SourceText = {});

        /** Text that must not be translated, such as a player's name. */
        static DisplayText AsInvariant(String t_Text);

        /**
         * A pattern with named arguments, such as "{Count} enemies left". Each {Key} in
         * the resolved pattern is replaced with the resolved argument that has that key.
         * A key with no argument is kept as it is. "{{" and "}}" give "{" and "}".
         *
         * Text is always in logical order (the order in which it is read), never
         * reversed. When the current language is RightToLeft, each argument is put
         * between the Unicode marks FIRST STRONG ISOLATE (U+2068) and POP DIRECTIONAL
         * ISOLATE (U+2069). Then a left-to-right argument, such as a player name or a
         * number, does not change the order of the right-to-left text around it. A
         * renderer that does the Unicode bidirectional algorithm does not draw these
         * marks.
         */
        static DisplayText Format(const DisplayText& t_Pattern, std::initializer_list<DisplayTextArg> t_Args);

        /** The text in the current language. */
        String Resolve() const;

        bool IsEmpty() const;

    private:
        struct Data;

        explicit DisplayText(std::shared_ptr<const Data> t_Data);

        std::shared_ptr<const Data> m_Data;
    };

    /** One named argument of DisplayText::Format. */
    struct CORE_API DisplayTextArg
    {
        DisplayTextArg(Name t_Key, DisplayText t_Value);
        DisplayTextArg(Name t_Key, std::int64_t t_Value);
        DisplayTextArg(Name t_Key, int t_Value);
        DisplayTextArg(Name t_Key, double t_Value);

        Name Key;
        DisplayText Value;
    };
} // namespace Fling

/** Logs a DisplayText as its text in the current language. */
template <>
struct fmt::formatter<Fling::DisplayText> : fmt::formatter<fmt::string_view>
{
    auto format(const Fling::DisplayText& t_Text, fmt::format_context& t_Context) const
    {
        const Fling::String Resolved = t_Text.Resolve();
        return fmt::formatter<fmt::string_view>::format(fmt::string_view(Resolved.data(), Resolved.size()), t_Context);
    }
};
