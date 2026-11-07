#pragma once

#include "CoreAPI.h"
#include "FlingIntegers.h"
#include "Strings/Name.h"
#include "Strings/String.h"

#include <cstdint>

namespace Fling
{
    /** The direction in which a language is read. */
    enum class TextDirection : uint8
    {
        LeftToRight,
        RightToLeft,
    };

    /**
     * The plural category of a number in a language, as in the Unicode CLDR plural
     * rules. English uses One ("1 enemy") and Other ("2 enemies"). Russian also uses
     * Few and Many. Arabic uses all six. Japanese uses only Other.
     */
    enum class PluralCategory : uint8
    {
        Zero,
        One,
        Two,
        Few,
        Many,
        Other,
    };

    /** The CLDR keyword of a category: "zero", "one", "two", "few", "many", or "other". */
    CORE_API StringView ToString(PluralCategory t_Category);

    /**
     * The plural category of a number in a language, from the CLDR cardinal plural
     * rules. A region or script after "-" or "_" ("pt-BR") does not change the rule.
     * A language with no rule here uses the English rule.
     */
    CORE_API PluralCategory GetPluralCategory(StringView t_Language, double t_Number);

    /** How a language writes numbers. */
    struct NumberFormat
    {
        /** Between the whole part and the fraction: "." in English, "," in German. */
        String DecimalSeparator = ".";
        /** Between groups of three digits: "," in English, "." in German. Empty for no groups. */
        String GroupSeparator = ",";
        /** The fewest digits a group of three needs before a separator is used: 2 in Spanish ("1000", "10.000"). */
        uint8 MinimumGroupingDigits = 1;
    };

    /** Writes a whole number with the group separator of t_Format: 12345 is "12,345" in English. */
    CORE_API String FormatNumber(int64 t_Number, const NumberFormat& t_Format);

    /** Writes a number with the separators of t_Format, with the fewest digits that read back as the same value. */
    CORE_API String FormatNumber(double t_Number, const NumberFormat& t_Format);

    /**
     * Writes a date with a pattern: "yyyy" is the year, "MM" and "dd" the month and day
     * with two digits, "M" and "d" with no leading zero. Other characters are copied.
     * "dd.MM.yyyy" gives "06.11.2026".
     */
    CORE_API String FormatDate(int t_Year, int t_Month, int t_Day, StringView t_Pattern);

    /** What DisplayText needs to know about the current language. */
    struct DisplayLanguage
    {
        /** The language code, for example "en" or "pt-BR". */
        Name Code;
        TextDirection Direction = TextDirection::LeftToRight;
        NumberFormat Numbers;
        /** The pattern for FormatDate. */
        String DatePattern = "M/d/yyyy";
    };

    /**
     * The direction, number format, and date pattern of a language code, from built-in
     * defaults. A language folder can change them in its "_language.json" (see
     * Localization).
     */
    CORE_API DisplayLanguage DefaultDisplayLanguageFor(Name t_Code);

    /**
     * Sets the current language for DisplayText: plural rules, number and date formats,
     * and direction. The localization service sets it when the language changes.
     */
    CORE_API void SetDisplayLanguage(DisplayLanguage t_Language);

    /** The current language. English until a language is set. */
    CORE_API DisplayLanguage GetDisplayLanguage();

    /** The direction of the current language. UI reads it to mirror its layout. */
    CORE_API TextDirection GetDisplayTextDirection();
} // namespace Fling
