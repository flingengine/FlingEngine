#include "Json.h"
#include "Localization.h"
#include "Logger.h"
#include "Strings/DisplayText.h"

#include <algorithm>
#include <regex>
#include <system_error>
#include <utility>

namespace Fling
{
    namespace
    {
        /** UTF-8 byte order mark. Spreadsheet programs need it to read CSV as UTF-8. */
        constexpr StringView Utf8Bom = "\xEF\xBB\xBF";

        Mutex g_ActiveMutex;
        std::shared_ptr<Localization> g_Active FLING_GUARDED_BY(g_ActiveMutex);

        bool ResolveWithActive(const Name t_Table, const Name t_Key, String& t_OutText)
        {
            const std::shared_ptr<Localization> Active = Localization::GetActive();
            return Active && Active->Find(t_Table, t_Key, t_OutText);
        }

        /** Sorts names by their text, so files and exports have the same order every run. */
        bool LexicalLess(const Name t_A, const Name t_B)
        {
            return t_A.ToView() < t_B.ToView();
        }

        void AppendCsvField(String& t_Out, const StringView t_Field)
        {
            const bool bQuote = t_Field.find_first_of(",\"\r\n") != StringView::npos;
            if (!bQuote)
            {
                t_Out += t_Field;
                return;
            }

            t_Out += '"';
            for (const char C : t_Field)
            {
                if (C == '"')
                {
                    t_Out += '"';
                }
                t_Out += C;
            }
            t_Out += '"';
        }

        /**
         * Splits CSV text into rows of fields (RFC 4180): fields in double quotes can
         * hold commas, line breaks, and doubled quotes. Returns false on a quote that is
         * not closed.
         */
        bool ParseCsv(StringView t_Csv, std::vector<std::vector<String>>& t_OutRows, String& t_OutError)
        {
            if (t_Csv.substr(0, Utf8Bom.size()) == Utf8Bom)
            {
                t_Csv.remove_prefix(Utf8Bom.size());
            }

            std::vector<String> Row;
            String Field;
            bool bInQuotes = false;
            bool bRowHasData = false;
            std::size_t Line = 1;

            auto EndField = [&]()
            {
                Row.push_back(std::move(Field));
                Field.clear();
            };
            auto EndRow = [&]()
            {
                EndField();
                if (bRowHasData)
                {
                    t_OutRows.push_back(std::move(Row));
                }
                Row.clear();
                bRowHasData = false;
            };

            for (std::size_t Index = 0; Index < t_Csv.size(); ++Index)
            {
                const char C = t_Csv[Index];
                if (bInQuotes)
                {
                    if (C == '"')
                    {
                        if (Index + 1 < t_Csv.size() && t_Csv[Index + 1] == '"')
                        {
                            Field += '"';
                            ++Index;
                        }
                        else
                        {
                            bInQuotes = false;
                        }
                    }
                    else
                    {
                        Line += (C == '\n') ? 1 : 0;
                        Field += C;
                    }
                    continue;
                }

                switch (C)
                {
                case '"':
                    bInQuotes = true;
                    bRowHasData = true;
                    break;
                case ',':
                    EndField();
                    bRowHasData = true;
                    break;
                case '\r':
                    break;
                case '\n':
                    EndRow();
                    ++Line;
                    break;
                default:
                    Field += C;
                    bRowHasData = true;
                    break;
                }
            }

            if (bInQuotes)
            {
                t_OutError = "A quoted field that starts before line " + std::to_string(Line) + " is not closed";
                return false;
            }
            EndRow();
            return true;
        }
    } // namespace

    Localization::Localization(std::filesystem::path t_Root, const Name t_SourceLanguage)
        : m_Root(std::move(t_Root)), m_SourceLanguage(t_SourceLanguage)
    {
        WriteScopedLock Lock(m_Mutex);
        m_Language = t_SourceLanguage;
    }

    bool Localization::LoadLanguage(const Name t_Language)
    {
        const std::filesystem::path Folder = m_Root / PathFromUtf8(t_Language.ToView());
        std::error_code Error;
        if (t_Language.IsNone() || !std::filesystem::is_directory(Folder, Error))
        {
            return false;
        }

        // Read the files before the lock, so lookups do not wait for the disk.
        Language Loaded;
        LanguageInfo Info = DefaultInfoFor(t_Language);
        for (const std::filesystem::directory_entry& Entry : std::filesystem::directory_iterator(Folder, Error))
        {
            if (!Entry.is_regular_file() || Entry.path().extension() != ".json")
            {
                continue;
            }

            const String FilePath = PathToUtf8(Entry.path());
            Json Doc;
            if (!Doc.LoadFromFile(FilePath) || !Doc.IsObject())
            {
                F_LOG_ERROR("String table {} is not a JSON object of key to text", FilePath);
                continue;
            }

            if (Entry.path().filename() == LanguageInfoFile)
            {
                Info.DisplayName = Doc.GetString("DisplayName");
                const String Direction = Doc.GetString("Direction");
                if (Direction == "RightToLeft")
                {
                    Info.Direction = TextDirection::RightToLeft;
                }
                else if (Direction == "LeftToRight")
                {
                    Info.Direction = TextDirection::LeftToRight;
                }
                else if (!Direction.empty())
                {
                    F_LOG_ERROR("{}: Direction must be LeftToRight or RightToLeft, not '{}'", FilePath, Direction);
                }
                if (Doc.Contains("DecimalSeparator"))
                {
                    Info.Numbers.DecimalSeparator = Doc.GetString("DecimalSeparator");
                }
                if (Doc.Contains("GroupSeparator"))
                {
                    Info.Numbers.GroupSeparator = Doc.GetString("GroupSeparator");
                }
                if (Doc.Contains("MinimumGroupingDigits"))
                {
                    Info.Numbers.MinimumGroupingDigits =
                        static_cast<std::uint8_t>(Doc.GetInt("MinimumGroupingDigits", Info.Numbers.MinimumGroupingDigits));
                }
                if (Doc.Contains("DatePattern"))
                {
                    Info.DatePattern = Doc.GetString("DatePattern");
                }
                continue;
            }

            Table& Entries = Loaded[Name(PathToUtf8(Entry.path().stem()))];
            for (const std::string& Key : Doc.Keys())
            {
                Entries[Name(Key)] = Doc.GetString(Key);
            }
        }

        {
            WriteScopedLock Lock(m_Mutex);
            m_Languages[t_Language] = std::move(Loaded);
            m_Infos[t_Language] = std::move(Info);
        }
        BumpRevision();
        PublishLanguage();
        return true;
    }

    bool Localization::SetLanguage(const Name t_Language)
    {
        bool bHasEntries = false;
        {
            ReadScopedLock Lock(m_Mutex);
            bHasEntries = m_Languages.find(t_Language) != m_Languages.end();
        }

        if (!bHasEntries && !LoadLanguage(t_Language))
        {
            F_LOG_WARN("No string tables for language '{}' in {}", t_Language, PathToUtf8(m_Root));
            return false;
        }

        {
            WriteScopedLock Lock(m_Mutex);
            m_Language = t_Language;
        }
        BumpRevision();
        PublishLanguage();
        return true;
    }

    Name Localization::GetLanguage() const
    {
        ReadScopedLock Lock(m_Mutex);
        return m_Language;
    }

    LanguageInfo Localization::GetLanguageInfo(const Name t_Language) const
    {
        {
            ReadScopedLock Lock(m_Mutex);
            const auto Found = m_Infos.find(t_Language);
            if (Found != m_Infos.end())
            {
                return Found->second;
            }
        }

        return DefaultInfoFor(t_Language);
    }

    LanguageInfo Localization::DefaultInfoFor(const Name t_Language)
    {
        const DisplayLanguage Defaults = DefaultDisplayLanguageFor(t_Language);
        LanguageInfo Info;
        Info.Code = t_Language;
        Info.Direction = Defaults.Direction;
        Info.Numbers = Defaults.Numbers;
        Info.DatePattern = Defaults.DatePattern;
        return Info;
    }

    TextDirection Localization::GetTextDirection() const
    {
        return GetLanguageInfo(GetLanguage()).Direction;
    }

    TextDirection Localization::DefaultDirectionFor(const Name t_Language)
    {
        return DefaultDisplayLanguageFor(t_Language).Direction;
    }

    void Localization::PublishLanguage() const
    {
        if (GetActive().get() != this)
        {
            return;
        }

        const LanguageInfo Info = GetLanguageInfo(GetLanguage());
        DisplayLanguage Language;
        Language.Code = Info.Code;
        Language.Direction = Info.Direction;
        Language.Numbers = Info.Numbers;
        Language.DatePattern = Info.DatePattern;
        SetDisplayLanguage(std::move(Language));
    }

    std::vector<Name> Localization::FindLanguagesOnDisk() const
    {
        std::vector<Name> Languages;
        std::error_code Error;
        for (const std::filesystem::directory_entry& Entry : std::filesystem::directory_iterator(m_Root, Error))
        {
            if (Entry.is_directory())
            {
                Languages.push_back(Name(PathToUtf8(Entry.path().filename())));
            }
        }
        std::sort(Languages.begin(), Languages.end(), LexicalLess);
        return Languages;
    }

    std::vector<Name> Localization::GetLoadedLanguages() const
    {
        std::vector<Name> Languages;
        {
            ReadScopedLock Lock(m_Mutex);
            for (const auto& [Code, Tables] : m_Languages)
            {
                if (Code != m_SourceLanguage)
                {
                    Languages.push_back(Code);
                }
            }
        }
        std::sort(Languages.begin(), Languages.end(), LexicalLess);
        Languages.insert(Languages.begin(), m_SourceLanguage);
        return Languages;
    }

    void Localization::SetEntry(const Name t_Language, const Name t_Table, const Name t_Key, String t_Text)
    {
        {
            WriteScopedLock Lock(m_Mutex);
            m_Languages[t_Language][t_Table][t_Key] = std::move(t_Text);
        }
        BumpRevision();
    }

    const String* Localization::FindLocked(const Name t_Language, const Name t_Table, const Name t_Key) const
    {
        const auto LanguageIt = m_Languages.find(t_Language);
        if (LanguageIt == m_Languages.end())
        {
            return nullptr;
        }
        const auto TableIt = LanguageIt->second.find(t_Table);
        if (TableIt == LanguageIt->second.end())
        {
            return nullptr;
        }
        const auto EntryIt = TableIt->second.find(t_Key);
        return EntryIt == TableIt->second.end() ? nullptr : &EntryIt->second;
    }

    bool Localization::Find(const Name t_Table, const Name t_Key, String& t_OutText) const
    {
        ReadScopedLock Lock(m_Mutex);
        const String* Text = FindLocked(m_Language, t_Table, t_Key);
        if (Text == nullptr && m_Language != m_SourceLanguage)
        {
            Text = FindLocked(m_SourceLanguage, t_Table, t_Key);
        }
        if (Text == nullptr)
        {
            return false;
        }
        t_OutText = *Text;
        return true;
    }

    std::optional<String> Localization::FindIn(const Name t_Language, const Name t_Table, const Name t_Key) const
    {
        ReadScopedLock Lock(m_Mutex);
        const String* Text = FindLocked(t_Language, t_Table, t_Key);
        return Text ? std::optional<String>(*Text) : std::nullopt;
    }

    std::vector<MissingLocalizedEntry> Localization::FindMissing(const Name t_Language) const
    {
        std::vector<MissingLocalizedEntry> Missing;
        {
            ReadScopedLock Lock(m_Mutex);
            const auto SourceIt = m_Languages.find(m_SourceLanguage);
            if (SourceIt == m_Languages.end())
            {
                return Missing;
            }
            for (const auto& [TableName, Entries] : SourceIt->second)
            {
                for (const auto& [Key, Text] : Entries)
                {
                    if (FindLocked(t_Language, TableName, Key) == nullptr)
                    {
                        Missing.push_back({TableName, Key, Text});
                    }
                }
            }
        }

        std::sort(Missing.begin(),
                  Missing.end(),
                  [](const MissingLocalizedEntry& t_A, const MissingLocalizedEntry& t_B)
                  {
                      if (t_A.Table != t_B.Table)
                      {
                          return LexicalLess(t_A.Table, t_B.Table);
                      }
                      return LexicalLess(t_A.Key, t_B.Key);
                  });
        return Missing;
    }

    String Localization::ExportCsv(const std::vector<Name>& t_Languages) const
    {
        const std::vector<Name> Columns = t_Languages.empty() ? GetLoadedLanguages() : t_Languages;

        String Out(Utf8Bom);
        Out += "Table,Key";
        for (const Name Language : Columns)
        {
            Out += ',';
            AppendCsvField(Out, Language.ToView());
        }
        Out += "\r\n";

        ReadScopedLock Lock(m_Mutex);

        // Every entry that any of the columns has, sorted by table and key text.
        std::vector<std::pair<Name, Name>> Rows;
        for (const Name Language : Columns)
        {
            const auto LanguageIt = m_Languages.find(Language);
            if (LanguageIt == m_Languages.end())
            {
                continue;
            }
            for (const auto& [TableName, Entries] : LanguageIt->second)
            {
                for (const auto& [Key, Text] : Entries)
                {
                    Rows.emplace_back(TableName, Key);
                }
            }
        }
        std::sort(Rows.begin(),
                  Rows.end(),
                  [](const std::pair<Name, Name>& t_A, const std::pair<Name, Name>& t_B)
                  {
                      if (t_A.first != t_B.first)
                      {
                          return LexicalLess(t_A.first, t_B.first);
                      }
                      return LexicalLess(t_A.second, t_B.second);
                  });
        Rows.erase(std::unique(Rows.begin(), Rows.end()), Rows.end());

        for (const auto& [TableName, Key] : Rows)
        {
            AppendCsvField(Out, TableName.ToView());
            Out += ',';
            AppendCsvField(Out, Key.ToView());
            for (const Name Language : Columns)
            {
                Out += ',';
                if (const String* Text = FindLocked(Language, TableName, Key))
                {
                    AppendCsvField(Out, *Text);
                }
            }
            Out += "\r\n";
        }
        return Out;
    }

    bool Localization::ImportCsv(const StringView t_Csv, String& t_OutError, std::vector<Name>* t_OutLanguages)
    {
        std::vector<std::vector<String>> Rows;
        if (!ParseCsv(t_Csv, Rows, t_OutError))
        {
            return false;
        }
        if (Rows.empty() || Rows[0].size() < 3 || Rows[0][0] != "Table" || Rows[0][1] != "Key")
        {
            t_OutError = "The first row must be: Table,Key,<language>...";
            return false;
        }

        std::vector<Name> Languages;
        for (std::size_t Column = 2; Column < Rows[0].size(); ++Column)
        {
            if (Rows[0][Column].empty())
            {
                t_OutError = "Language column " + std::to_string(Column + 1) + " has no name";
                return false;
            }
            Languages.push_back(Name(Rows[0][Column]));
        }

        // Check every row before changing anything.
        for (std::size_t RowIndex = 1; RowIndex < Rows.size(); ++RowIndex)
        {
            const std::vector<String>& Row = Rows[RowIndex];
            if (Row.size() != Rows[0].size())
            {
                t_OutError = "Row " + std::to_string(RowIndex + 1) + " has " + std::to_string(Row.size()) +
                             " fields, not " + std::to_string(Rows[0].size());
                return false;
            }
            if (Row[0].empty() || Row[1].empty())
            {
                t_OutError = "Row " + std::to_string(RowIndex + 1) + " has no table or no key";
                return false;
            }
        }

        {
            WriteScopedLock Lock(m_Mutex);
            for (std::size_t RowIndex = 1; RowIndex < Rows.size(); ++RowIndex)
            {
                const std::vector<String>& Row = Rows[RowIndex];
                const Name TableName(Row[0]);
                const Name Key(Row[1]);
                for (std::size_t Column = 0; Column < Languages.size(); ++Column)
                {
                    const String& Text = Row[Column + 2];
                    if (!Text.empty())
                    {
                        m_Languages[Languages[Column]][TableName][Key] = Text;
                    }
                }
            }
        }
        BumpRevision();

        if (t_OutLanguages)
        {
            *t_OutLanguages = std::move(Languages);
        }
        return true;
    }

    bool Localization::SaveLanguage(const Name t_Language, String& t_OutError) const
    {
        const std::filesystem::path Folder = m_Root / PathFromUtf8(t_Language.ToView());
        std::error_code Error;
        std::filesystem::create_directories(Folder, Error);
        if (Error)
        {
            t_OutError = "Cannot make folder " + PathToUtf8(Folder) + ": " + Error.message();
            return false;
        }

        ReadScopedLock Lock(m_Mutex);
        const auto LanguageIt = m_Languages.find(t_Language);
        if (LanguageIt == m_Languages.end())
        {
            t_OutError = "Language '" + String(t_Language.ToView()) + "' has no entries";
            return false;
        }

        for (const auto& [TableName, Entries] : LanguageIt->second)
        {
            // The JSON object sorts its keys, so the file is the same each time it is saved.
            Json Doc = Json::Object();
            for (const auto& [Key, Text] : Entries)
            {
                Doc.Set(String(Key.ToView()), Text);
            }

            const String FilePath = PathToUtf8(Folder / PathFromUtf8(String(TableName.ToView()) + ".json"));
            if (!Doc.SaveToFile(FilePath))
            {
                t_OutError = "Cannot write " + FilePath;
                return false;
            }
        }
        return true;
    }

    std::vector<GatheredEntry> Localization::GatherFromCode(const StringView t_Code, const StringView t_Location)
    {
        // FromTable("Table"_name, "Key"_name) or FromTable("Table"_name, "Key"_name, "Source text")
        static const std::regex Call(
            R"re(FromTable\s*\(\s*"([^"\\]*)"_name\s*,\s*"([^"\\]*)"_name\s*(?:,\s*"((?:[^"\\]|\\.)*)"\s*)?\))re");

        std::vector<GatheredEntry> Entries;
        const String Code(t_Code);
        for (auto It = std::sregex_iterator(Code.begin(), Code.end(), Call); It != std::sregex_iterator(); ++It)
        {
            const std::smatch& Match = *It;
            GatheredEntry Entry;
            Entry.Table = Name(Match[1].str());
            Entry.Key = Name(Match[2].str());

            // Read the escapes that a C++ string literal can have in game text.
            const String Literal = Match[3].str();
            for (std::size_t Index = 0; Index < Literal.size(); ++Index)
            {
                if (Literal[Index] == '\\' && Index + 1 < Literal.size())
                {
                    const char Escaped = Literal[++Index];
                    Entry.SourceText += Escaped == 'n' ? '\n' : Escaped == 't' ? '\t' : Escaped;
                }
                else
                {
                    Entry.SourceText += Literal[Index];
                }
            }

            const std::size_t Line =
                1 + static_cast<std::size_t>(std::count(Code.begin(), Code.begin() + Match.position(0), '\n'));
            Entry.Location = String(t_Location) + ":" + std::to_string(Line);
            Entries.push_back(std::move(Entry));
        }
        return Entries;
    }

    GatherResult Localization::AddGathered(const std::vector<GatheredEntry>& t_Entries)
    {
        GatherResult Result;
        for (const GatheredEntry& Entry : t_Entries)
        {
            const std::optional<String> Existing = FindIn(m_SourceLanguage, Entry.Table, Entry.Key);
            if (Existing)
            {
                if (!Entry.SourceText.empty() && *Existing != Entry.SourceText)
                {
                    ++Result.Mismatched;
                    F_LOG_WARN("{}: {}.{} has the text \"{}\" in code, and \"{}\" in the {} table. The table is kept.",
                               Entry.Location, Entry.Table, Entry.Key, Entry.SourceText, *Existing, m_SourceLanguage);
                }
            }
            else if (Entry.SourceText.empty())
            {
                ++Result.MissingText;
                F_LOG_WARN("{}: {}.{} is not in the {} tables, and the code gives no source text", Entry.Location,
                           Entry.Table, Entry.Key, m_SourceLanguage);
            }
            else
            {
                SetEntry(m_SourceLanguage, Entry.Table, Entry.Key, Entry.SourceText);
                ++Result.Added;
            }
        }
        return Result;
    }

    void Localization::SetActive(std::shared_ptr<Localization> t_Localization)
    {
        const bool bHasActive = t_Localization != nullptr;
        {
            ScopedLock Lock(g_ActiveMutex);
            g_Active = std::move(t_Localization);
        }
        SetDisplayTextTableResolver(bHasActive ? &ResolveWithActive : nullptr);

        if (const std::shared_ptr<Localization> Active = GetActive())
        {
            Active->PublishLanguage();
        }
        else
        {
            SetDisplayLanguage(DefaultDisplayLanguageFor("en"_name));
        }
    }

    std::shared_ptr<Localization> Localization::GetActive()
    {
        ScopedLock Lock(g_ActiveMutex);
        return g_Active;
    }
} // namespace Fling
