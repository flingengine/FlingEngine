#include "Strings/DisplayText.h"

#include <atomic>
#include <charconv>
#include <cstdio>
#include <system_error>
#include <utility>
#include <vector>

namespace Fling
{
    struct DisplayText::Data
    {
        enum class Kind : std::uint8_t
        {
            Table,
            Invariant,
            Format,
            Integer,
            Double,
            Date,
        };

        Kind Type = Kind::Invariant;
        Name Table;
        Name Key;
        /** Invariant text, or the source text of a table entry. */
        String Text;
        DisplayText Pattern;
        std::vector<DisplayTextArg> Args;
        std::int64_t Integer = 0;
        double Double = 0.0;
        int Year = 0;
        int Month = 0;
        int Day = 0;
    };

    namespace
    {
        std::atomic<DisplayTextTableResolver> g_TableResolver{nullptr};

        /** U+2068 FIRST STRONG ISOLATE and U+2069 POP DIRECTIONAL ISOLATE, in UTF-8. */
        constexpr StringView FirstStrongIsolate = "\xE2\x81\xA8";
        constexpr StringView PopDirectionalIsolate = "\xE2\x81\xA9";

        StringView Trim(StringView t_Text)
        {
            while (!t_Text.empty() && (t_Text.front() == ' ' || t_Text.front() == '\t'))
            {
                t_Text.remove_prefix(1);
            }
            while (!t_Text.empty() && (t_Text.back() == ' ' || t_Text.back() == '\t'))
            {
                t_Text.remove_suffix(1);
            }
            return t_Text;
        }

        const DisplayTextArg* FindArg(const StringView t_Key, const std::vector<DisplayTextArg>& t_Args)
        {
            for (const DisplayTextArg& Arg : t_Args)
            {
                if (Arg.Key.ToView() == t_Key)
                {
                    return &Arg;
                }
            }
            return nullptr;
        }

        /** The index after the '}' that closes the '{' at t_Open, or npos. */
        std::size_t FindClose(const StringView t_Pattern, const std::size_t t_Open)
        {
            int Depth = 0;
            for (std::size_t Index = t_Open; Index < t_Pattern.size(); ++Index)
            {
                if (t_Pattern[Index] == '{')
                {
                    ++Depth;
                }
                else if (t_Pattern[Index] == '}' && --Depth == 0)
                {
                    return Index + 1;
                }
            }
            return StringView::npos;
        }

        void AppendIsolated(String& t_Out, const StringView t_Text, const DisplayLanguage& t_Language)
        {
            if (t_Language.Direction == TextDirection::RightToLeft)
            {
                t_Out += FirstStrongIsolate;
                t_Out += t_Text;
                t_Out += PopDirectionalIsolate;
            }
            else
            {
                t_Out += t_Text;
            }
        }

        /**
         * Expands a message pattern into t_Out.
         *
         * @param t_PluralNumber  The text that '#' becomes, inside a plural case. Null
         *                        outside one.
         */
        void ExpandPattern(StringView t_Pattern,
                           const std::vector<DisplayTextArg>& t_Args,
                           const DisplayLanguage& t_Language,
                           const String* t_PluralNumber,
                           String& t_Out);

        /**
         * Expands the cases of "{Name, plural|select, case {message} case {message} ...}".
         * t_Body is the text after the second comma. Returns false if the cases cannot be
         * read or none matches.
         */
        bool ExpandChoice(const StringView t_Body,
                          const bool t_bPlural,
                          const DisplayTextArg& t_Arg,
                          const std::vector<DisplayTextArg>& t_Args,
                          const DisplayLanguage& t_Language,
                          String& t_Out)
        {
            struct Case
            {
                StringView Selector;
                StringView Message;
            };

            // Read every case first, then pick one.
            std::vector<Case> Cases;
            std::size_t Index = 0;
            while (true)
            {
                while (Index < t_Body.size() && (t_Body[Index] == ' ' || t_Body[Index] == '\t'))
                {
                    ++Index;
                }
                if (Index >= t_Body.size())
                {
                    break;
                }
                const std::size_t Open = t_Body.find('{', Index);
                if (Open == StringView::npos)
                {
                    return false;
                }
                const std::size_t Close = FindClose(t_Body, Open);
                if (Close == StringView::npos)
                {
                    return false;
                }
                Cases.push_back({Trim(t_Body.substr(Index, Open - Index)), t_Body.substr(Open + 1, Close - Open - 2)});
                Index = Close;
            }

            const Case* Chosen = nullptr;
            String PluralNumber;
            if (t_bPlural)
            {
                const double Number = t_Arg.Value.GetNumber();
                PluralNumber = t_Arg.Value.Resolve();

                // An exact "=N" case first, then the category of the language, then "other"
                for (const Case& Each : Cases)
                {
                    if (Each.Selector.size() > 1 && Each.Selector.front() == '=')
                    {
                        double Exact = 0.0;
                        const StringView Digits = Each.Selector.substr(1);
                        const std::from_chars_result Parsed =
                            std::from_chars(Digits.data(), Digits.data() + Digits.size(), Exact);
                        if (Parsed.ec == std::errc() && Exact == Number)
                        {
                            Chosen = &Each;
                            break;
                        }
                    }
                }
                if (!Chosen)
                {
                    const StringView Category = ToString(GetPluralCategory(t_Language.Code.ToView(), Number));
                    for (const Case& Each : Cases)
                    {
                        if (Each.Selector == Category)
                        {
                            Chosen = &Each;
                            break;
                        }
                    }
                }
            }
            else
            {
                const String Value = t_Arg.Value.Resolve();
                for (const Case& Each : Cases)
                {
                    if (Each.Selector == Value)
                    {
                        Chosen = &Each;
                        break;
                    }
                }
            }
            if (!Chosen)
            {
                for (const Case& Each : Cases)
                {
                    if (Each.Selector == "other")
                    {
                        Chosen = &Each;
                        break;
                    }
                }
            }
            if (!Chosen)
            {
                return false;
            }

            ExpandPattern(Chosen->Message, t_Args, t_Language, t_bPlural ? &PluralNumber : nullptr, t_Out);
            return true;
        }

        void ExpandPattern(const StringView t_Pattern,
                           const std::vector<DisplayTextArg>& t_Args,
                           const DisplayLanguage& t_Language,
                           const String* t_PluralNumber,
                           String& t_Out)
        {
            std::size_t Index = 0;
            while (Index < t_Pattern.size())
            {
                const char C = t_Pattern[Index];
                if (C == '{' && Index + 1 < t_Pattern.size() && t_Pattern[Index + 1] == '{')
                {
                    t_Out += '{';
                    Index += 2;
                    continue;
                }
                if (C == '}' && Index + 1 < t_Pattern.size() && t_Pattern[Index + 1] == '}')
                {
                    t_Out += '}';
                    Index += 2;
                    continue;
                }
                if (C == '#' && t_PluralNumber)
                {
                    AppendIsolated(t_Out, *t_PluralNumber, t_Language);
                    ++Index;
                    continue;
                }
                if (C == '{')
                {
                    const std::size_t End = FindClose(t_Pattern, Index);
                    if (End != StringView::npos)
                    {
                        const StringView Inner = t_Pattern.substr(Index + 1, End - Index - 2);
                        const std::size_t FirstComma = Inner.find(',');
                        const StringView Key = Trim(Inner.substr(0, FirstComma));
                        const DisplayTextArg* Arg = FindArg(Key, t_Args);

                        if (Arg && FirstComma == StringView::npos)
                        {
                            AppendIsolated(t_Out, Arg->Value.Resolve(), t_Language);
                            Index = End;
                            continue;
                        }
                        if (Arg)
                        {
                            const std::size_t SecondComma = Inner.find(',', FirstComma + 1);
                            const StringView Type =
                                SecondComma == StringView::npos
                                    ? StringView()
                                    : Trim(Inner.substr(FirstComma + 1, SecondComma - FirstComma - 1));
                            if ((Type == "plural" || Type == "select") &&
                                ExpandChoice(
                                    Inner.substr(SecondComma + 1), Type == "plural", *Arg, t_Args, t_Language, t_Out))
                            {
                                Index = End;
                                continue;
                            }
                        }

                        // No argument for this key, or a form that cannot be read: keep the text.
                        t_Out.append(t_Pattern.substr(Index, End - Index));
                        Index = End;
                        continue;
                    }
                }
                t_Out += C;
                ++Index;
            }
        }
    } // namespace

    void SetDisplayTextTableResolver(const DisplayTextTableResolver t_Resolver)
    {
        g_TableResolver.store(t_Resolver, std::memory_order_release);
    }

    DisplayText::DisplayText(std::shared_ptr<const Data> t_Data) : m_Data(std::move(t_Data))
    {
    }

    DisplayText DisplayText::FromTable(const Name t_Table, const Name t_Key, const StringView t_SourceText)
    {
        auto NewData = std::make_shared<Data>();
        NewData->Type = Data::Kind::Table;
        NewData->Table = t_Table;
        NewData->Key = t_Key;
        NewData->Text = String(t_SourceText);
        return DisplayText(std::move(NewData));
    }

    DisplayText DisplayText::AsInvariant(String t_Text)
    {
        auto NewData = std::make_shared<Data>();
        NewData->Type = Data::Kind::Invariant;
        NewData->Text = std::move(t_Text);
        return DisplayText(std::move(NewData));
    }

    DisplayText DisplayText::AsNumber(const std::int64_t t_Number)
    {
        auto NewData = std::make_shared<Data>();
        NewData->Type = Data::Kind::Integer;
        NewData->Integer = t_Number;
        return DisplayText(std::move(NewData));
    }

    DisplayText DisplayText::AsNumber(const double t_Number)
    {
        auto NewData = std::make_shared<Data>();
        NewData->Type = Data::Kind::Double;
        NewData->Double = t_Number;
        return DisplayText(std::move(NewData));
    }

    DisplayText DisplayText::AsDate(const int t_Year, const int t_Month, const int t_Day)
    {
        auto NewData = std::make_shared<Data>();
        NewData->Type = Data::Kind::Date;
        NewData->Year = t_Year;
        NewData->Month = t_Month;
        NewData->Day = t_Day;
        return DisplayText(std::move(NewData));
    }

    DisplayText DisplayText::Format(const DisplayText& t_Pattern, const std::initializer_list<DisplayTextArg> t_Args)
    {
        auto NewData = std::make_shared<Data>();
        NewData->Type = Data::Kind::Format;
        NewData->Pattern = t_Pattern;
        NewData->Args.assign(t_Args.begin(), t_Args.end());
        return DisplayText(std::move(NewData));
    }

    String DisplayText::Resolve() const
    {
        if (!m_Data)
        {
            return String();
        }

        switch (m_Data->Type)
        {
        case Data::Kind::Table:
        {
            if (const DisplayTextTableResolver Resolver = g_TableResolver.load(std::memory_order_acquire))
            {
                String Text;
                if (Resolver(m_Data->Table, m_Data->Key, Text))
                {
                    return Text;
                }
            }
            return m_Data->Text.empty() ? String(m_Data->Key.ToView()) : m_Data->Text;
        }
        case Data::Kind::Invariant:
            return m_Data->Text;
        case Data::Kind::Format:
        {
            const String Pattern = m_Data->Pattern.Resolve();
            String Out;
            Out.reserve(Pattern.size());
            ExpandPattern(Pattern, m_Data->Args, GetDisplayLanguage(), nullptr, Out);
            return Out;
        }
        case Data::Kind::Integer:
            return FormatNumber(m_Data->Integer, GetDisplayLanguage().Numbers);
        case Data::Kind::Double:
            return FormatNumber(m_Data->Double, GetDisplayLanguage().Numbers);
        case Data::Kind::Date:
            return FormatDate(m_Data->Year, m_Data->Month, m_Data->Day, GetDisplayLanguage().DatePattern);
        }
        return String();
    }

    bool DisplayText::IsEmpty() const
    {
        return Resolve().empty();
    }

    bool DisplayText::IsNumber() const
    {
        return m_Data && (m_Data->Type == Data::Kind::Integer || m_Data->Type == Data::Kind::Double);
    }

    double DisplayText::GetNumber() const
    {
        if (!m_Data)
        {
            return 0.0;
        }
        if (m_Data->Type == Data::Kind::Integer)
        {
            return static_cast<double>(m_Data->Integer);
        }
        return m_Data->Type == Data::Kind::Double ? m_Data->Double : 0.0;
    }

    DisplayTextArg::DisplayTextArg(const Name t_Key, DisplayText t_Value) : Key(t_Key), Value(std::move(t_Value))
    {
    }

    DisplayTextArg::DisplayTextArg(const Name t_Key, const std::int64_t t_Value)
        : Key(t_Key), Value(DisplayText::AsNumber(t_Value))
    {
    }

    DisplayTextArg::DisplayTextArg(const Name t_Key, const int t_Value)
        : DisplayTextArg(t_Key, static_cast<std::int64_t>(t_Value))
    {
    }

    DisplayTextArg::DisplayTextArg(const Name t_Key, const double t_Value)
        : Key(t_Key), Value(DisplayText::AsNumber(t_Value))
    {
    }
} // namespace Fling
