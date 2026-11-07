#include "Strings/DisplayText.h"
#include "Strings/Language.h"

#include <catch2/catch_all.hpp>


using namespace Fling;

namespace
{
    /** Sets a language for one test, and puts English back after. */
    struct LanguageScope
    {
        explicit LanguageScope(const Name t_Code) { SetDisplayLanguage(DefaultDisplayLanguageFor(t_Code)); }
        ~LanguageScope() { SetDisplayLanguage(DefaultDisplayLanguageFor("en"_name)); }
    };

    String Category(const char* t_Language, const double t_Number)
    {
        return String(ToString(GetPluralCategory(t_Language, t_Number)));
    }
} // namespace

TEST_CASE("Plural categories follow the CLDR rules of each language", "[Strings][Language]")
{
    // English: 1 is one, everything else is other, also 1.5
    CHECK(Category("en", 1) == "one");
    CHECK(Category("en", 0) == "other");
    CHECK(Category("en", 2) == "other");
    CHECK(Category("en", 1.5) == "other");
    CHECK(Category("en-GB", 1) == "one");

    // French: 0 and 1 are one
    CHECK(Category("fr", 0) == "one");
    CHECK(Category("fr", 1) == "one");
    CHECK(Category("fr", 2) == "other");

    // Russian: one, few, many
    CHECK(Category("ru", 1) == "one");
    CHECK(Category("ru", 21) == "one");
    CHECK(Category("ru", 11) == "many");
    CHECK(Category("ru", 2) == "few");
    CHECK(Category("ru", 24) == "few");
    CHECK(Category("ru", 12) == "many");
    CHECK(Category("ru", 5) == "many");
    CHECK(Category("ru", 1.5) == "other");

    // Polish: 1 is one, 22 is few, 21 and 12 are many
    CHECK(Category("pl", 1) == "one");
    CHECK(Category("pl", 22) == "few");
    CHECK(Category("pl", 21) == "many");
    CHECK(Category("pl", 12) == "many");

    // Czech: 2 to 4 are few, a fraction is many
    CHECK(Category("cs", 3) == "few");
    CHECK(Category("cs", 5) == "other");
    CHECK(Category("cs", 1.5) == "many");

    // Arabic uses all six
    CHECK(Category("ar", 0) == "zero");
    CHECK(Category("ar", 1) == "one");
    CHECK(Category("ar", 2) == "two");
    CHECK(Category("ar", 3) == "few");
    CHECK(Category("ar", 11) == "many");
    CHECK(Category("ar", 100) == "other");

    // Hebrew: one, two, other
    CHECK(Category("he", 2) == "two");
    CHECK(Category("he", 3) == "other");

    // Japanese and Chinese have only other
    CHECK(Category("ja", 1) == "other");
    CHECK(Category("zh-Hans", 1) == "other");
}

TEST_CASE("Numbers use the separators of the language", "[Strings][Language]")
{
    const NumberFormat English = DefaultDisplayLanguageFor("en"_name).Numbers;
    const NumberFormat German = DefaultDisplayLanguageFor("de"_name).Numbers;
    const NumberFormat Spanish = DefaultDisplayLanguageFor("es"_name).Numbers;
    const NumberFormat French = DefaultDisplayLanguageFor("fr"_name).Numbers;

    CHECK(FormatNumber(std::int64_t{0}, English) == "0");
    CHECK(FormatNumber(std::int64_t{999}, English) == "999");
    CHECK(FormatNumber(std::int64_t{1000}, English) == "1,000");
    CHECK(FormatNumber(std::int64_t{1234567}, English) == "1,234,567");
    CHECK(FormatNumber(std::int64_t{-1234567}, English) == "-1,234,567");
    CHECK(FormatNumber(std::int64_t{1234567}, German) == "1.234.567");

    // Spanish groups only from five digits
    CHECK(FormatNumber(std::int64_t{1000}, Spanish) == "1000");
    CHECK(FormatNumber(std::int64_t{10000}, Spanish) == "10.000");

    // French uses a narrow no-break space (U+202F)
    CHECK(FormatNumber(std::int64_t{12345}, French) == "12\xE2\x80\xAF"
                                                       "345");

    CHECK(FormatNumber(1234.5, English) == "1,234.5");
    CHECK(FormatNumber(1234.5, German) == "1.234,5");
    CHECK(FormatNumber(-0.25, German) == "-0,25");
}

TEST_CASE("Dates use the pattern of the language", "[Strings][Language]")
{
    CHECK(FormatDate(2026, 11, 6, "M/d/yyyy") == "11/6/2026");
    CHECK(FormatDate(2026, 3, 6, "dd.MM.yyyy") == "06.03.2026");
    CHECK(FormatDate(2026, 3, 6, "yyyy-MM-dd") == "2026-03-06");
    CHECK(FormatDate(2026, 3, 6, "yyyy. M. d.") == "2026. 3. 6.");

    CHECK(DefaultDisplayLanguageFor("en"_name).DatePattern == "M/d/yyyy");
    CHECK(DefaultDisplayLanguageFor("en-GB"_name).DatePattern == "dd/MM/yyyy");
    CHECK(DefaultDisplayLanguageFor("de"_name).DatePattern == "dd.MM.yyyy");
    CHECK(DefaultDisplayLanguageFor("ja"_name).DatePattern == "yyyy/MM/dd");
}

TEST_CASE("DisplayText numbers and dates follow the current language", "[Strings][Language][DisplayText]")
{
    const DisplayText Big = DisplayText::AsNumber(std::int64_t{1234567});
    const DisplayText Date = DisplayText::AsDate(2026, 11, 6);
    const DisplayText Score = DisplayText::Format(DisplayText::AsInvariant("Score: {Score}"), {{"Score"_name, 1500}});

    CHECK(Big.Resolve() == "1,234,567");
    CHECK(Date.Resolve() == "11/6/2026");
    CHECK(Score.Resolve() == "Score: 1,500");
    CHECK(Big.IsNumber());
    CHECK(Big.GetNumber() == 1234567.0);

    LanguageScope German("de"_name);
    CHECK(Big.Resolve() == "1.234.567");
    CHECK(Date.Resolve() == "06.11.2026");
    CHECK(Score.Resolve() == "Score: 1.500");
}

TEST_CASE("DisplayText plural forms pick the case for the number", "[Strings][Language][DisplayText]")
{
    const DisplayText Pattern =
        DisplayText::AsInvariant("{Count, plural, =0 {No enemies left} one {# enemy left} other {# enemies left}}");
    auto Enemies = [&](const int t_Count)
    {
        return DisplayText::Format(Pattern, {{"Count"_name, t_Count}}).Resolve();
    };

    CHECK(Enemies(0) == "No enemies left");
    CHECK(Enemies(1) == "1 enemy left");
    CHECK(Enemies(2) == "2 enemies left");
    CHECK(Enemies(1000) == "1,000 enemies left");

    // A Russian translation uses the Russian categories.
    LanguageScope Russian("ru"_name);
    const DisplayText RussianPattern =
        DisplayText::AsInvariant("{Count, plural, one {# враг} few {# врага} many {# врагов} other {# врага}}");
    auto RussianEnemies = [&](const int t_Count)
    {
        return DisplayText::Format(RussianPattern, {{"Count"_name, t_Count}}).Resolve();
    };
    CHECK(RussianEnemies(1) == "1 враг");
    CHECK(RussianEnemies(3) == "3 врага");
    CHECK(RussianEnemies(5) == "5 врагов");
    CHECK(RussianEnemies(21) == "21 враг");
}

TEST_CASE("DisplayText select forms pick the case for the text", "[Strings][Language][DisplayText]")
{
    const DisplayText Pattern = DisplayText::AsInvariant(
        "{Gender, select, female {She won with {Score}} male {He won with {Score}} other {They won with {Score}}}");
    auto Won = [&](const char* t_Gender)
    {
        return DisplayText::Format(Pattern, {{"Gender"_name, DisplayText::AsInvariant(t_Gender)}, {"Score"_name, 7}})
            .Resolve();
    };

    CHECK(Won("female") == "She won with 7");
    CHECK(Won("male") == "He won with 7");
    CHECK(Won("unknown") == "They won with 7");
}

TEST_CASE("DisplayText keeps text it cannot read as a form", "[Strings][Language][DisplayText]")
{
    // No argument, a form that is not closed, and a plural with no matching case
    CHECK(DisplayText::Format(DisplayText::AsInvariant("{Missing, plural, one {x}}"), {}).Resolve() ==
          "{Missing, plural, one {x}}");
    CHECK(DisplayText::Format(DisplayText::AsInvariant("{Count, plural, one {x}"), {{"Count"_name, 1}}).Resolve() ==
          "{Count, plural, one {x}");
    CHECK(DisplayText::Format(DisplayText::AsInvariant("{Count, plural, one {x}}"), {{"Count"_name, 2}}).Resolve() ==
          "{Count, plural, one {x}}");
    CHECK(DisplayText::Format(DisplayText::AsInvariant("a } b"), {}).Resolve() == "a } b");
}
