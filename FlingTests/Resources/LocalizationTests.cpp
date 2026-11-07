#include "Localization.h"
#include "Logger.h"
#include "Strings/DisplayText.h"

#include <catch2/catch_all.hpp>
#include <filesystem>
#include <fstream>
#include <string>

using namespace Fling;

namespace
{
    void InitLogger()
    {
        Logger::Get().Init();
        // Keep startup log lines off the console so a passing run stays quiet.
        Logger::GetCurrentConsole()->set_level(spdlog::level::off);
        Logger::GetCurrentLogFile()->set_level(spdlog::level::off);
    }

    /** A folder of string tables that is deleted at the end of the test. */
    class TempTables
    {
    public:
        explicit TempTables(const char* t_Name)
            : m_Root(std::filesystem::temp_directory_path() / (std::string("FlingLocTests_") + t_Name))
        {
            std::filesystem::remove_all(m_Root);
            std::filesystem::create_directories(m_Root);
        }

        ~TempTables()
        {
            std::error_code Error;
            std::filesystem::remove_all(m_Root, Error);
        }

        void Write(const char* t_Language, const char* t_Table, const std::string& t_Json) const
        {
            std::filesystem::create_directories(m_Root / t_Language);
            std::ofstream File(m_Root / t_Language / (std::string(t_Table) + ".json"), std::ios::binary);
            File << t_Json;
        }

        const std::filesystem::path& GetRoot() const { return m_Root; }

    private:
        std::filesystem::path m_Root;
    };

    /** Makes a Localization active for one test, and removes it after. */
    struct ActiveScope
    {
        explicit ActiveScope(std::shared_ptr<Localization> t_Loc) { Localization::SetActive(std::move(t_Loc)); }
        ~ActiveScope() { Localization::SetActive(nullptr); }
    };
} // namespace

TEST_CASE("Localization finds entries in the current language, then the source language", "[Localization]")
{
    Localization Loc("unused", "en"_name);
    Loc.SetEntry("en"_name, "UI"_name, "Menu.Start"_name, "Start Game");
    Loc.SetEntry("en"_name, "UI"_name, "Menu.Quit"_name, "Quit");
    Loc.SetEntry("fr"_name, "UI"_name, "Menu.Start"_name, "Commencer");

    String Text;
    CHECK(Loc.GetLanguage() == "en"_name);
    REQUIRE(Loc.Find("UI"_name, "Menu.Start"_name, Text));
    CHECK(Text == "Start Game");

    REQUIRE(Loc.SetLanguage("fr"_name));
    REQUIRE(Loc.Find("UI"_name, "Menu.Start"_name, Text));
    CHECK(Text == "Commencer");

    // French has no Menu.Quit, so it falls back to English.
    REQUIRE(Loc.Find("UI"_name, "Menu.Quit"_name, Text));
    CHECK(Text == "Quit");

    CHECK(!Loc.Find("UI"_name, "Menu.Nope"_name, Text));
    CHECK(!Loc.Find("Other"_name, "Menu.Start"_name, Text));

    CHECK(Loc.FindIn("fr"_name, "UI"_name, "Menu.Quit"_name) == std::nullopt);
    CHECK(Loc.FindIn("en"_name, "UI"_name, "Menu.Quit"_name) == String("Quit"));
}

TEST_CASE("Localization keeps the current language when a language cannot be loaded", "[Localization]")
{
    InitLogger();
    TempTables Files("MissingLanguage");
    Localization Loc(Files.GetRoot(), "en"_name);
    CHECK(!Loc.SetLanguage("de"_name));
    CHECK(Loc.GetLanguage() == "en"_name);
}

TEST_CASE("Localization changes its revision when text or the language changes", "[Localization]")
{
    Localization Loc("unused", "en"_name);
    const std::uint64_t Start = Loc.GetRevision();
    Loc.SetEntry("fr"_name, "UI"_name, "Menu.Start"_name, "Commencer");
    const std::uint64_t AfterEntry = Loc.GetRevision();
    CHECK(AfterEntry != Start);
    REQUIRE(Loc.SetLanguage("fr"_name));
    CHECK(Loc.GetRevision() != AfterEntry);
}

TEST_CASE("Localization loads string tables from one folder per language", "[Localization]")
{
    InitLogger();
    TempTables Files("Load");
    Files.Write("en", "UI", R"({"Menu.Start": "Start Game", "Menu.Quit": "Quit"})");
    Files.Write("en", "Items", R"({"Sword": "Sword"})");
    // "Épée" as UTF-8 bytes
    Files.Write("fr",
                "Items",
                "{\"Sword\": \"\xC3\x89p\xC3\xA9"
                "e\"}");

    Localization Loc(Files.GetRoot(), "en"_name);
    CHECK(Loc.FindLanguagesOnDisk() == std::vector<Name>{"en"_name, "fr"_name});
    REQUIRE(Loc.LoadLanguage("en"_name));
    REQUIRE(Loc.SetLanguage("fr"_name));
    CHECK(Loc.GetLoadedLanguages() == std::vector<Name>{"en"_name, "fr"_name});

    String Text;
    REQUIRE(Loc.Find("Items"_name, "Sword"_name, Text));
    CHECK(Text == "\xC3\x89p\xC3\xA9"
                  "e");
    REQUIRE(Loc.Find("UI"_name, "Menu.Start"_name, Text));
    CHECK(Text == "Start Game");

    const std::vector<MissingLocalizedEntry> Missing = Loc.FindMissing("fr"_name);
    REQUIRE(Missing.size() == 2);
    CHECK(Missing[0].Table == "UI"_name);
    CHECK(Missing[0].Key == "Menu.Quit"_name);
    CHECK(Missing[0].SourceText == "Quit");
    CHECK(Missing[1].Key == "Menu.Start"_name);
}

TEST_CASE("Localization saves a language and loads the same entries back", "[Localization]")
{
    InitLogger();
    TempTables Files("Save");
    {
        Localization Loc(Files.GetRoot(), "en"_name);
        Loc.SetEntry("de"_name, "UI"_name, "Menu.Start"_name, "Spiel starten");
        Loc.SetEntry("de"_name, "Hud"_name, "Ammo"_name, "Munition: {Count}");
        String Error;
        REQUIRE(Loc.SaveLanguage("de"_name, Error));
        CHECK(!Loc.SaveLanguage("ja"_name, Error));
    }

    CHECK(std::filesystem::exists(Files.GetRoot() / "de" / "UI.json"));
    CHECK(std::filesystem::exists(Files.GetRoot() / "de" / "Hud.json"));

    Localization Loaded(Files.GetRoot(), "en"_name);
    REQUIRE(Loaded.LoadLanguage("de"_name));
    CHECK(Loaded.FindIn("de"_name, "UI"_name, "Menu.Start"_name) == String("Spiel starten"));
    CHECK(Loaded.FindIn("de"_name, "Hud"_name, "Ammo"_name) == String("Munition: {Count}"));
}

TEST_CASE("Localization exports a CSV table for translators and imports it back", "[Localization]")
{
    Localization Loc("unused", "en"_name);
    Loc.SetEntry("en"_name, "UI"_name, "Menu.Start"_name, "Start Game");
    Loc.SetEntry("en"_name, "UI"_name, "Hud.Quote"_name, "He said \"go\", then left\nfast");
    Loc.SetEntry("en"_name, "Items"_name, "Sword"_name, "Sword");
    Loc.SetEntry("fr"_name, "UI"_name, "Menu.Start"_name, "Commencer");

    const String Csv = Loc.ExportCsv();
    CHECK(Csv == "\xEF\xBB\xBF"
                 "Table,Key,en,fr\r\n"
                 "Items,Sword,Sword,\r\n"
                 "UI,Hud.Quote,\"He said \"\"go\"\", then left\nfast\",\r\n"
                 "UI,Menu.Start,Start Game,Commencer\r\n");

    // A translator fills in the French column and adds German.
    const String Translated = "Table,Key,en,fr,de\r\n"
                              "Items,Sword,Sword,\"\xC3\x89p\xC3\xA9"
                              "e\",Schwert\r\n"
                              "UI,Hud.Quote,\"He said \"\"go\"\", then left\nfast\",\"Il a dit \"\"vas-y\"\"\",\r\n"
                              "UI,Menu.Start,Start Game,Commencer,Spiel starten\r\n";
    Localization Imported("unused", "en"_name);
    String Error;
    std::vector<Name> Languages;
    REQUIRE(Imported.ImportCsv(Translated, Error, &Languages));
    CHECK(Languages == std::vector<Name>{"en"_name, "fr"_name, "de"_name});
    CHECK(Imported.FindIn("en"_name, "UI"_name, "Hud.Quote"_name) == String("He said \"go\", then left\nfast"));
    CHECK(Imported.FindIn("fr"_name, "UI"_name, "Hud.Quote"_name) == String("Il a dit \"vas-y\""));
    CHECK(Imported.FindIn("fr"_name, "Items"_name, "Sword"_name) == String("\xC3\x89p\xC3\xA9"
                                                                           "e"));
    CHECK(Imported.FindIn("de"_name, "Items"_name, "Sword"_name) == String("Schwert"));
    // An empty cell is no entry.
    CHECK(Imported.FindIn("de"_name, "UI"_name, "Hud.Quote"_name) == std::nullopt);

    // Importing an export changes nothing.
    CHECK(Loc.ImportCsv(Loc.ExportCsv(), Error));
    CHECK(Loc.ExportCsv() == Csv);
}

TEST_CASE("Localization rejects a CSV table that is not valid, and changes nothing", "[Localization]")
{
    Localization Loc("unused", "en"_name);
    String Error;

    CHECK(!Loc.ImportCsv("Key,Table,en\r\nUI,Start,Start\r\n", Error));
    CHECK(!Error.empty());

    CHECK(!Loc.ImportCsv("Table,Key,en\r\nUI,Start\r\n", Error));
    CHECK(Error.find("Row 2") != String::npos);

    CHECK(!Loc.ImportCsv("Table,Key,en\r\nUI,Start,\"not closed\r\n", Error));
    CHECK(!Loc.ImportCsv("Table,Key,en\r\n,Start,Start\r\n", Error));

    // A good row before a bad row is not imported either.
    CHECK(!Loc.ImportCsv("Table,Key,en\r\nUI,Good,Good\r\nUI,Bad\r\n", Error));
    CHECK(Loc.FindIn("en"_name, "UI"_name, "Good"_name) == std::nullopt);
}

TEST_CASE("DisplayText resolves table entries through the active Localization", "[Localization][DisplayText]")
{
    const DisplayText Start = DisplayText::FromTable("UI"_name, "Menu.Start"_name, "Start Game (source)");
    const DisplayText KeyOnly = DisplayText::FromTable("UI"_name, "Menu.Unknown"_name);

    // With no active Localization, the source text shows, then the key.
    CHECK(Start.Resolve() == "Start Game (source)");
    CHECK(KeyOnly.Resolve() == "Menu.Unknown");

    auto Loc = std::make_shared<Localization>("unused", "en"_name);
    Loc->SetEntry("en"_name, "UI"_name, "Menu.Start"_name, "Start Game");
    Loc->SetEntry("en"_name, "UI"_name, "Hud.EnemiesLeft"_name, "{Count} enemies left");
    Loc->SetEntry("fr"_name, "UI"_name, "Menu.Start"_name, "Commencer");
    Loc->SetEntry("fr"_name, "UI"_name, "Hud.EnemiesLeft"_name, "Il reste {Count} ennemis");
    ActiveScope Scope(Loc);

    const DisplayText Enemies =
        DisplayText::Format(DisplayText::FromTable("UI"_name, "Hud.EnemiesLeft"_name), {{"Count"_name, 3}});

    CHECK(Start.Resolve() == "Start Game");
    CHECK(Enemies.Resolve() == "3 enemies left");

    // The same DisplayText objects show the new language after a change.
    REQUIRE(Loc->SetLanguage("fr"_name));
    CHECK(Start.Resolve() == "Commencer");
    CHECK(Enemies.Resolve() == "Il reste 3 ennemis");
    CHECK(KeyOnly.Resolve() == "Menu.Unknown");
}

TEST_CASE("Localization knows which languages read right to left", "[Localization][RightToLeft]")
{
    CHECK(Localization::DefaultDirectionFor("en"_name) == TextDirection::LeftToRight);
    CHECK(Localization::DefaultDirectionFor("fr"_name) == TextDirection::LeftToRight);
    CHECK(Localization::DefaultDirectionFor("ja"_name) == TextDirection::LeftToRight);
    CHECK(Localization::DefaultDirectionFor("ar"_name) == TextDirection::RightToLeft);
    CHECK(Localization::DefaultDirectionFor("he"_name) == TextDirection::RightToLeft);
    CHECK(Localization::DefaultDirectionFor("fa"_name) == TextDirection::RightToLeft);
    CHECK(Localization::DefaultDirectionFor("ur"_name) == TextDirection::RightToLeft);
    CHECK(Localization::DefaultDirectionFor("ar-EG"_name) == TextDirection::RightToLeft);
    CHECK(Localization::DefaultDirectionFor("he_IL"_name) == TextDirection::RightToLeft);
    // A code that only starts with the letters of a right-to-left code is not one.
    CHECK(Localization::DefaultDirectionFor("arn"_name) == TextDirection::LeftToRight);
}

TEST_CASE("_language.json sets the name and direction of a language, and is not a table", "[Localization][RightToLeft]")
{
    InitLogger();
    TempTables Files("LanguageInfo");
    // "Start" in Hebrew, and "Hebrew" in Hebrew, as UTF-8 bytes
    Files.Write("he", "UI", "{\"Menu.Start\": \"\xD7\x94\xD7\xAA\xD7\x97\xD7\x9C\"}");
    Files.Write("he", "_language", "{\"DisplayName\": \"\xD7\xA2\xD7\x91\xD7\xA8\xD7\x99\xD7\xAA\"}");
    // A made-up code that reads right to left only because its file says so
    Files.Write("xr", "UI", R"({"Menu.Start": "Start"})");
    Files.Write("xr", "_language", R"({"Direction": "RightToLeft"})");

    Localization Loc(Files.GetRoot(), "en"_name);
    REQUIRE(Loc.LoadLanguage("he"_name));
    REQUIRE(Loc.LoadLanguage("xr"_name));

    const LanguageInfo Hebrew = Loc.GetLanguageInfo("he"_name);
    CHECK(Hebrew.Code == "he"_name);
    CHECK(Hebrew.DisplayName == "\xD7\xA2\xD7\x91\xD7\xA8\xD7\x99\xD7\xAA");
    CHECK(Hebrew.Direction == TextDirection::RightToLeft);
    CHECK(Loc.GetLanguageInfo("xr"_name).Direction == TextDirection::RightToLeft);
    CHECK(Loc.GetLanguageInfo("fr"_name).Direction == TextDirection::LeftToRight);

    // _language.json is not a table, so it is not exported.
    CHECK(Loc.FindIn("he"_name, "_language"_name, "DisplayName"_name) == std::nullopt);
    CHECK(Loc.ExportCsv({"he"_name}).find("_language") == String::npos);

    CHECK(Loc.GetTextDirection() == TextDirection::LeftToRight);
    REQUIRE(Loc.SetLanguage("he"_name));
    CHECK(Loc.GetTextDirection() == TextDirection::RightToLeft);
}

TEST_CASE("Right-to-left text is kept in logical order through files and CSV", "[Localization][RightToLeft]")
{
    InitLogger();
    TempTables Files("RightToLeftOrder");
    // "ابدأ" ("Start" in Arabic) in logical order: the first letter read is the first byte.
    const String Arabic = "\xD8\xA7\xD8\xA8\xD8\xAF\xD8\xA3";
    {
        Localization Loc(Files.GetRoot(), "en"_name);
        Loc.SetEntry("ar"_name, "UI"_name, "Menu.Start"_name, Arabic);
        String Error;
        REQUIRE(Loc.SaveLanguage("ar"_name, Error));

        const String Csv = Loc.ExportCsv({"ar"_name});
        CHECK(Csv.find(Arabic) != String::npos);

        Localization FromCsv("unused", "en"_name);
        REQUIRE(FromCsv.ImportCsv(Csv, Error));
        CHECK(FromCsv.FindIn("ar"_name, "UI"_name, "Menu.Start"_name) == Arabic);
    }

    Localization Loaded(Files.GetRoot(), "en"_name);
    REQUIRE(Loaded.LoadLanguage("ar"_name));
    CHECK(Loaded.FindIn("ar"_name, "UI"_name, "Menu.Start"_name) == Arabic);
}

TEST_CASE("Format arguments are isolated when the current language reads right to left",
          "[Localization][RightToLeft][DisplayText]")
{
    const String Isolate = "\xE2\x81\xA8";
    const String Pop = "\xE2\x81\xA9";

    auto Loc = std::make_shared<Localization>("unused", "en"_name);
    Loc->SetEntry("en"_name, "Hud"_name, "Score"_name, "{Player}: {Score}");
    // "النقاط" ("points" in Arabic)
    Loc->SetEntry(
        "ar"_name, "Hud"_name, "Score"_name, "{Player} \xD8\xA7\xD9\x84\xD9\x86\xD9\x82\xD8\xA7\xD8\xB7 {Score}");
    ActiveScope Scope(Loc);

    const DisplayText Score =
        DisplayText::Format(DisplayText::FromTable("Hud"_name, "Score"_name),
                            {{"Player"_name, DisplayText::AsInvariant("Ben")}, {"Score"_name, 42}});

    // Left to right: no marks, so a renderer with no bidirectional support shows no boxes.
    CHECK(GetDisplayTextDirection() == TextDirection::LeftToRight);
    CHECK(Score.Resolve() == "Ben: 42");

    REQUIRE(Loc->SetLanguage("ar"_name));
    CHECK(GetDisplayTextDirection() == TextDirection::RightToLeft);
    CHECK(Score.Resolve() ==
          Isolate + "Ben" + Pop + " \xD8\xA7\xD9\x84\xD9\x86\xD9\x82\xD8\xA7\xD8\xB7 " + Isolate + "42" + Pop);

    REQUIRE(Loc->SetLanguage("en"_name));
    CHECK(GetDisplayTextDirection() == TextDirection::LeftToRight);

    // Removing the active Localization goes back to left to right.
    REQUIRE(Loc->SetLanguage("ar"_name));
    Localization::SetActive(nullptr);
    CHECK(GetDisplayTextDirection() == TextDirection::LeftToRight);
}

TEST_CASE("_language.json can set number and date formats", "[Localization][Language]")
{
    InitLogger();
    TempTables Files("Formats");
    Files.Write("de", "UI", R"({"Menu.Start": "Spiel starten"})");
    Files.Write("de-CH", "UI", R"({"Menu.Start": "Spiel starten"})");
    // Swiss German uses an apostrophe to group digits
    Files.Write("de-CH", "_language", R"({"GroupSeparator": "'", "DecimalSeparator": ".", "DatePattern": "d.M.yyyy"})");

    auto Loc = std::make_shared<Localization>(Files.GetRoot(), "en"_name);
    ActiveScope Scope(Loc);
    const DisplayText Big = DisplayText::AsNumber(1234567.5);
    const DisplayText Date = DisplayText::AsDate(2026, 3, 6);

    REQUIRE(Loc->SetLanguage("de"_name));
    CHECK(GetDisplayLanguage().Code == "de"_name);
    CHECK(Big.Resolve() == "1.234.567,5");
    CHECK(Date.Resolve() == "06.03.2026");

    REQUIRE(Loc->SetLanguage("de-CH"_name));
    CHECK(Big.Resolve() == "1'234'567.5");
    CHECK(Date.Resolve() == "6.3.2026");
}
