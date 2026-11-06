#pragma once

#include "Strings/DisplayText.h"
#include "Strings/Name.h"
#include "Strings/String.h"
#include "Threading/Mutex.h"
#include "Threading/ThreadSafety.h"

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>

namespace Fling
{
    /** A string table entry that one language does not have. */
    struct MissingLocalizedEntry
    {
        Name Table;
        Name Key;
        /** The text of the entry in the source language. */
        String SourceText;
    };

    /** A language and how it is shown. */
    struct LanguageInfo
    {
        /** The language code, which is also its folder name, for example "fr" or "ar". */
        Name Code;
        /** The name of the language in that language, for a language menu. Empty if not set. */
        String DisplayName;
        TextDirection Direction = TextDirection::LeftToRight;
    };

    /**
     * The string tables of every language, and the current language. DisplayText
     * resolves table entries through the active Localization (see SetActive).
     *
     * Files: each language is a folder under the root folder, named by its language
     * code, and each table is a JSON object of key to text in that folder:
     *
     *     Localization/en/UI.json    {"Menu.Start": "Start Game"}
     *     Localization/fr/UI.json    {"Menu.Start": "Commencer"}
     *
     * The source language has every key. The other languages translate them. A lookup
     * uses the current language, then the source language.
     *
     * A language folder can also have a "_language.json" file, which is not a table:
     *
     *     Localization/ar/_language.json    {"DisplayName": "...", "Direction": "RightToLeft"}
     *
     * Without it, the direction comes from the language code (see DefaultDirectionFor).
     * The direction of the current language goes to SetDisplayTextDirection, so UI can
     * mirror its layout. Text is always stored in logical order, never reversed: the
     * renderer applies the Unicode bidirectional algorithm.
     *
     * Translators work in CSV: ExportCsv writes one row per entry, with a column per
     * language, and ImportCsv reads that file back.
     *
     * All functions are safe to call from any thread.
     */
    class Localization
    {
    public:
        /**
         * @param t_Root            The folder that holds one folder per language.
         * @param t_SourceLanguage  The language that has every key, for example "en".
         */
        Localization(std::filesystem::path t_Root, Name t_SourceLanguage);

        /**
         * Reads every table of a language from its folder, and replaces the entries that
         * were loaded before. Returns false if the folder does not exist.
         */
        bool LoadLanguage(Name t_Language);

        /**
         * Makes t_Language the current language. Loads it first if it is not loaded.
         * Returns false, and keeps the current language, if it cannot be loaded and has
         * no entries.
         */
        bool SetLanguage(Name t_Language);

        Name GetLanguage() const;
        Name GetSourceLanguage() const { return m_SourceLanguage; }
        const std::filesystem::path& GetRoot() const { return m_Root; }

        /** The languages that have a folder under the root folder, sorted by code. */
        std::vector<Name> FindLanguagesOnDisk() const;

        /** The languages that have entries in memory, source language first. */
        std::vector<Name> GetLoadedLanguages() const;

        /** The display name and direction of a language. */
        LanguageInfo GetLanguageInfo(Name t_Language) const;

        /** The direction of the current language. */
        TextDirection GetTextDirection() const;

        /**
         * The direction of a language code with no "_language.json": RightToLeft for
         * Arabic, Hebrew, Persian, Urdu, Pashto, Yiddish, Dhivehi, Sindhi, Uyghur, and
         * Central Kurdish, otherwise LeftToRight. A region or script after "-" or "_"
         * ("ar-EG") does not change it.
         */
        static TextDirection DefaultDirectionFor(Name t_Language);

        /**
         * A number that changes each time the current language or any text changes. UI
         * that keeps resolved text can compare it to know when to resolve again.
         */
        std::uint64_t GetRevision() const { return m_Revision.load(std::memory_order_acquire); }

        /** Sets the text of one entry in memory. SaveLanguage writes it to disk. */
        void SetEntry(Name t_Language, Name t_Table, Name t_Key, String t_Text);

        /** The text of an entry in the current language, then in the source language. */
        bool Find(Name t_Table, Name t_Key, String& t_OutText) const;

        /** The text of an entry in one language only. */
        std::optional<String> FindIn(Name t_Language, Name t_Table, Name t_Key) const;

        /** The source language entries that t_Language does not have, sorted by table and key. */
        std::vector<MissingLocalizedEntry> FindMissing(Name t_Language) const;

        /**
         * A CSV table for translators, in UTF-8 with a byte order mark: a header row
         * "Table,Key,<language>...", then one row per entry, sorted by table and key.
         * An empty cell means that language has no entry.
         *
         * @param t_Languages  The language columns. Empty means every loaded language,
         *                     source language first.
         */
        String ExportCsv(const std::vector<Name>& t_Languages = {}) const;

        /**
         * Reads a CSV table in the form that ExportCsv writes, and sets each non-empty
         * cell as an entry. Returns false, and changes nothing, if the file is not valid.
         *
         * @param t_OutLanguages  Set to the language columns of the file.
         */
        bool ImportCsv(StringView t_Csv, String& t_OutError, std::vector<Name>* t_OutLanguages = nullptr);

        /** Writes every table of a language to its folder, one JSON file per table. */
        bool SaveLanguage(Name t_Language, String& t_OutError) const;

        /**
         * Makes t_Localization the one that DisplayText uses. nullptr removes it, and
         * table entries then show their source text.
         */
        static void SetActive(std::shared_ptr<Localization> t_Localization);

        static std::shared_ptr<Localization> GetActive();

    private:
        using Table = std::unordered_map<Name, String>;
        using Language = std::unordered_map<Name, Table>;

        const String* FindLocked(Name t_Language, Name t_Table, Name t_Key) const FLING_REQUIRES_SHARED(m_Mutex);

        void BumpRevision() { m_Revision.fetch_add(1, std::memory_order_acq_rel); }

        const std::filesystem::path m_Root;
        const Name m_SourceLanguage;

        mutable SharedMutex m_Mutex;
        std::unordered_map<Name, Language> m_Languages FLING_GUARDED_BY(m_Mutex);
        Name m_Language FLING_GUARDED_BY(m_Mutex);
        std::unordered_map<Name, LanguageInfo> m_Infos FLING_GUARDED_BY(m_Mutex);

        /** The file in a language folder that holds its LanguageInfo, not a table. */
        static constexpr const char* LanguageInfoFile = "_language.json";

        /** Sends the direction of the current language to DisplayText, if this is the active Localization. */
        void PublishDirection() const;

        std::atomic<std::uint64_t> m_Revision{0};
    };
} // namespace Fling
