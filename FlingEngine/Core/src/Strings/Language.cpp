#include "Strings/Language.h"
#include "Threading/Mutex.h"
#include "Threading/ThreadSafety.h"

#include <charconv>
#include <cmath>
#include <cstdlib>
#include <initializer_list>

namespace Fling
{
    namespace
    {
        /** The language part of a code: "pt" for "pt-BR". */
        StringView BaseLanguage(StringView t_Code)
        {
            const std::size_t Separator = t_Code.find_first_of("-_");
            return Separator == StringView::npos ? t_Code : t_Code.substr(0, Separator);
        }

        bool IsOneOf(const StringView t_Code, const std::initializer_list<StringView> t_Codes)
        {
            for (const StringView Code : t_Codes)
            {
                if (t_Code == Code)
                {
                    return true;
                }
            }
            return false;
        }

        bool InRange(const std::int64_t t_Value, const std::int64_t t_Low, const std::int64_t t_High)
        {
            return t_Value >= t_Low && t_Value <= t_High;
        }

        /** U+00A0 NO-BREAK SPACE and U+202F NARROW NO-BREAK SPACE, in UTF-8. */
        constexpr StringView NoBreakSpace = "\xC2\xA0";
        constexpr StringView NarrowNoBreakSpace = "\xE2\x80\xAF";

        SharedMutex g_LanguageMutex;
        DisplayLanguage g_Language FLING_GUARDED_BY(g_LanguageMutex) = DefaultDisplayLanguageFor("en"_name);

        /** Inserts the group separator into the digits of a whole number, with no sign. */
        String GroupDigits(const StringView t_Digits, const NumberFormat& t_Format)
        {
            const std::size_t Length = t_Digits.size();
            if (t_Format.GroupSeparator.empty() || Length < 4 || Length < 3u + t_Format.MinimumGroupingDigits)
            {
                return String(t_Digits);
            }

            String Out;
            const std::size_t FirstGroup = Length % 3 == 0 ? 3 : Length % 3;
            Out.append(t_Digits.substr(0, FirstGroup));
            for (std::size_t Index = FirstGroup; Index < Length; Index += 3)
            {
                Out += t_Format.GroupSeparator;
                Out.append(t_Digits.substr(Index, 3));
            }
            return Out;
        }
    } // namespace

    StringView ToString(const PluralCategory t_Category)
    {
        switch (t_Category)
        {
        case PluralCategory::Zero:
            return "zero";
        case PluralCategory::One:
            return "one";
        case PluralCategory::Two:
            return "two";
        case PluralCategory::Few:
            return "few";
        case PluralCategory::Many:
            return "many";
        case PluralCategory::Other:
            return "other";
        }
        return "other";
    }

    PluralCategory GetPluralCategory(const StringView t_Language, const double t_Number)
    {
        // The rules use these CLDR operands: i is the whole part, and v is true when
        // the number has a fraction. Rules are for whole numbers; a fraction is Other
        // unless the language says more.
        const double Absolute = std::fabs(t_Number);
        const std::int64_t I = static_cast<std::int64_t>(Absolute);
        const bool V = Absolute != static_cast<double>(I);
        const std::int64_t Mod10 = I % 10;
        const std::int64_t Mod100 = I % 100;
        const StringView Code = BaseLanguage(t_Language);

        // Only Other: Chinese, Japanese, Korean, Thai, Vietnamese, Indonesian, Malay, Lao, Burmese, Khmer
        if (IsOneOf(Code, {"zh", "ja", "ko", "th", "vi", "id", "ms", "lo", "my", "km"}))
        {
            return PluralCategory::Other;
        }

        // French, Portuguese, Hindi, Persian, Bengali, Gujarati, Kannada, Marathi, Zulu, Amharic: 0 and 1 are One
        if (IsOneOf(Code, {"fr", "pt", "hi", "fa", "bn", "gu", "kn", "mr", "zu", "am"}))
        {
            return I == 0 || I == 1 ? PluralCategory::One : PluralCategory::Other;
        }

        // Russian, Ukrainian, Belarusian
        if (IsOneOf(Code, {"ru", "uk", "be"}))
        {
            if (V)
            {
                return PluralCategory::Other;
            }
            if (Mod10 == 1 && Mod100 != 11)
            {
                return PluralCategory::One;
            }
            if (InRange(Mod10, 2, 4) && !InRange(Mod100, 12, 14))
            {
                return PluralCategory::Few;
            }
            return PluralCategory::Many;
        }

        // Croatian, Serbian, Bosnian
        if (IsOneOf(Code, {"hr", "sr", "bs"}))
        {
            if (!V && Mod10 == 1 && Mod100 != 11)
            {
                return PluralCategory::One;
            }
            if (!V && InRange(Mod10, 2, 4) && !InRange(Mod100, 12, 14))
            {
                return PluralCategory::Few;
            }
            return PluralCategory::Other;
        }

        if (Code == "pl")
        {
            if (V)
            {
                return PluralCategory::Other;
            }
            if (I == 1)
            {
                return PluralCategory::One;
            }
            if (InRange(Mod10, 2, 4) && !InRange(Mod100, 12, 14))
            {
                return PluralCategory::Few;
            }
            return PluralCategory::Many;
        }

        // Czech, Slovak
        if (IsOneOf(Code, {"cs", "sk"}))
        {
            if (V)
            {
                return PluralCategory::Many;
            }
            if (I == 1)
            {
                return PluralCategory::One;
            }
            return InRange(I, 2, 4) ? PluralCategory::Few : PluralCategory::Other;
        }

        if (Code == "ar")
        {
            if (V)
            {
                return PluralCategory::Other;
            }
            if (I == 0)
            {
                return PluralCategory::Zero;
            }
            if (I == 1)
            {
                return PluralCategory::One;
            }
            if (I == 2)
            {
                return PluralCategory::Two;
            }
            if (InRange(Mod100, 3, 10))
            {
                return PluralCategory::Few;
            }
            if (InRange(Mod100, 11, 99))
            {
                return PluralCategory::Many;
            }
            return PluralCategory::Other;
        }

        // Hebrew
        if (IsOneOf(Code, {"he", "iw"}))
        {
            if (!V && I == 1)
            {
                return PluralCategory::One;
            }
            if (!V && I == 2)
            {
                return PluralCategory::Two;
            }
            return PluralCategory::Other;
        }

        if (Code == "ro")
        {
            if (!V && I == 1)
            {
                return PluralCategory::One;
            }
            if (V || I == 0 || InRange(Mod100, 2, 19))
            {
                return PluralCategory::Few;
            }
            return PluralCategory::Other;
        }

        if (Code == "lt")
        {
            if (V)
            {
                return PluralCategory::Many;
            }
            if (Mod10 == 1 && !InRange(Mod100, 11, 19))
            {
                return PluralCategory::One;
            }
            if (InRange(Mod10, 2, 9) && !InRange(Mod100, 11, 19))
            {
                return PluralCategory::Few;
            }
            return PluralCategory::Other;
        }

        if (Code == "lv")
        {
            if (!V && (Mod10 == 0 || InRange(Mod100, 11, 19)))
            {
                return PluralCategory::Zero;
            }
            if (!V && Mod10 == 1 && Mod100 != 11)
            {
                return PluralCategory::One;
            }
            return PluralCategory::Other;
        }

        // English, German, Dutch, Spanish, Italian, the Nordic languages, and most others: 1 is One
        return !V && I == 1 ? PluralCategory::One : PluralCategory::Other;
    }

    String FormatNumber(const std::int64_t t_Number, const NumberFormat& t_Format)
    {
        char Buffer[24] = {};
        const std::to_chars_result Result = std::to_chars(Buffer, Buffer + sizeof(Buffer), t_Number);
        const StringView Text(Buffer, static_cast<std::size_t>(Result.ptr - Buffer));
        const bool bNegative = !Text.empty() && Text.front() == '-';
        return (bNegative ? String("-") : String()) + GroupDigits(bNegative ? Text.substr(1) : Text, t_Format);
    }

    String FormatNumber(const double t_Number, const NumberFormat& t_Format)
    {
        char Buffer[64] = {};
        const std::to_chars_result Result = std::to_chars(Buffer, Buffer + sizeof(Buffer), t_Number);
        StringView Text(Buffer, static_cast<std::size_t>(Result.ptr - Buffer));

        // Exponent form ("1e+30"), infinity, and NaN are written as they are.
        if (Text.find_first_of("eEin") != StringView::npos)
        {
            return String(Text);
        }

        String Out;
        if (!Text.empty() && Text.front() == '-')
        {
            Out += '-';
            Text.remove_prefix(1);
        }
        const std::size_t Point = Text.find('.');
        Out += GroupDigits(Text.substr(0, Point), t_Format);
        if (Point != StringView::npos)
        {
            Out += t_Format.DecimalSeparator;
            Out.append(Text.substr(Point + 1));
        }
        return Out;
    }

    String FormatDate(const int t_Year, const int t_Month, const int t_Day, const StringView t_Pattern)
    {
        auto AppendNumber = [](String& t_Out, const int t_Value, const int t_MinDigits)
        {
            const String Digits = std::to_string(std::abs(t_Value));
            if (t_Value < 0)
            {
                t_Out += '-';
            }
            for (int Pad = static_cast<int>(Digits.size()); Pad < t_MinDigits; ++Pad)
            {
                t_Out += '0';
            }
            t_Out += Digits;
        };

        String Out;
        std::size_t Index = 0;
        while (Index < t_Pattern.size())
        {
            const char C = t_Pattern[Index];
            std::size_t Run = 1;
            while (Index + Run < t_Pattern.size() && t_Pattern[Index + Run] == C)
            {
                ++Run;
            }

            if (C == 'y')
            {
                AppendNumber(Out, t_Year, static_cast<int>(Run));
            }
            else if (C == 'M')
            {
                AppendNumber(Out, t_Month, static_cast<int>(Run));
            }
            else if (C == 'd')
            {
                AppendNumber(Out, t_Day, static_cast<int>(Run));
            }
            else
            {
                Out.append(t_Pattern.substr(Index, Run));
            }
            Index += Run;
        }
        return Out;
    }

    DisplayLanguage DefaultDisplayLanguageFor(const Name t_Code)
    {
        DisplayLanguage Language;
        Language.Code = t_Code;

        const StringView Full = t_Code.ToView();
        const StringView Code = BaseLanguage(Full);

        if (IsOneOf(Code, {"ar", "he", "iw", "fa", "ur", "ps", "yi", "dv", "sd", "ug", "ckb"}))
        {
            Language.Direction = TextDirection::RightToLeft;
        }

        // Number separators, from the CLDR defaults of each language
        if (IsOneOf(Code, {"de", "es", "it", "nl", "pt", "id", "tr", "da", "el", "ro", "hr", "sr", "bs", "sl", "vi"}))
        {
            Language.Numbers.DecimalSeparator = ",";
            Language.Numbers.GroupSeparator = ".";
        }
        else if (Code == "fr")
        {
            Language.Numbers.DecimalSeparator = ",";
            Language.Numbers.GroupSeparator = String(NarrowNoBreakSpace);
        }
        else if (IsOneOf(
                     Code,
                     {"ru", "uk", "be", "pl", "cs", "sk", "sv", "fi", "nb", "no", "nn", "bg", "hu", "lt", "lv", "et"}))
        {
            Language.Numbers.DecimalSeparator = ",";
            Language.Numbers.GroupSeparator = String(NoBreakSpace);
        }
        if (IsOneOf(Code, {"es", "pl", "pt"}) && Full != "pt-BR")
        {
            Language.Numbers.MinimumGroupingDigits = 2;
        }

        // Short date patterns, from the CLDR defaults of each language
        if (Full == "en" || Full == "en-US" || Full == "en_US")
        {
            Language.DatePattern = "M/d/yyyy";
        }
        else if (IsOneOf(Code,
                         {"de",
                          "ru",
                          "uk",
                          "be",
                          "pl",
                          "cs",
                          "sk",
                          "fi",
                          "nb",
                          "no",
                          "nn",
                          "tr",
                          "ro",
                          "hr",
                          "sr",
                          "bs",
                          "sl",
                          "et",
                          "lv"}))
        {
            Language.DatePattern = "dd.MM.yyyy";
        }
        else if (IsOneOf(Code, {"sv", "lt"}))
        {
            Language.DatePattern = "yyyy-MM-dd";
        }
        else if (Code == "nl")
        {
            Language.DatePattern = "dd-MM-yyyy";
        }
        else if (IsOneOf(Code, {"ja", "zh"}))
        {
            Language.DatePattern = "yyyy/MM/dd";
        }
        else if (Code == "ko")
        {
            Language.DatePattern = "yyyy. M. d.";
        }
        else
        {
            // British English, French, Spanish, Italian, Portuguese, Arabic, Hebrew, and others
            Language.DatePattern = "dd/MM/yyyy";
        }
        return Language;
    }

    void SetDisplayLanguage(DisplayLanguage t_Language)
    {
        WriteScopedLock Lock(g_LanguageMutex);
        g_Language = std::move(t_Language);
    }

    DisplayLanguage GetDisplayLanguage()
    {
        ReadScopedLock Lock(g_LanguageMutex);
        return g_Language;
    }

    TextDirection GetDisplayTextDirection()
    {
        ReadScopedLock Lock(g_LanguageMutex);
        return g_Language.Direction;
    }
} // namespace Fling
