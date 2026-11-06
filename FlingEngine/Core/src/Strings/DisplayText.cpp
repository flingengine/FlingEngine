#include "Strings/DisplayText.h"

#include <charconv>
#include <cstdio>
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
        };

        Kind Type = Kind::Invariant;
        Name Table;
        Name Key;
        String Text;
        DisplayText Pattern;
        std::vector<DisplayTextArg> Args;
    };

    namespace
    {
        /** Appends the resolved text of the argument with this key, or returns false. */
        bool AppendArg(String& t_Out, const StringView t_Key, const std::vector<DisplayTextArg>& t_Args)
        {
            for (const DisplayTextArg& Arg : t_Args)
            {
                if (Arg.Key.ToView() == t_Key)
                {
                    t_Out += Arg.Value.Resolve();
                    return true;
                }
            }
            return false;
        }

        String ApplyArgs(const StringView t_Pattern, const std::vector<DisplayTextArg>& t_Args)
        {
            String Out;
            Out.reserve(t_Pattern.size());

            std::size_t Index = 0;
            while (Index < t_Pattern.size())
            {
                const char C = t_Pattern[Index];
                if (C == '{' && Index + 1 < t_Pattern.size() && t_Pattern[Index + 1] == '{')
                {
                    Out += '{';
                    Index += 2;
                    continue;
                }
                if (C == '}' && Index + 1 < t_Pattern.size() && t_Pattern[Index + 1] == '}')
                {
                    Out += '}';
                    Index += 2;
                    continue;
                }
                if (C == '{')
                {
                    const std::size_t Close = t_Pattern.find('}', Index + 1);
                    if (Close != StringView::npos)
                    {
                        const StringView Key = t_Pattern.substr(Index + 1, Close - Index - 1);
                        if (AppendArg(Out, Key, t_Args))
                        {
                            Index = Close + 1;
                            continue;
                        }
                    }
                }
                Out += C;
                ++Index;
            }
            return Out;
        }
    } // namespace

    DisplayText::DisplayText(std::shared_ptr<const Data> t_Data) : m_Data(std::move(t_Data))
    {
    }

    DisplayText DisplayText::FromTable(const Name t_Table, const Name t_Key)
    {
        auto NewData = std::make_shared<Data>();
        NewData->Type = Data::Kind::Table;
        NewData->Table = t_Table;
        NewData->Key = t_Key;
        return DisplayText(std::move(NewData));
    }

    DisplayText DisplayText::AsInvariant(String t_Text)
    {
        auto NewData = std::make_shared<Data>();
        NewData->Type = Data::Kind::Invariant;
        NewData->Text = std::move(t_Text);
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
            // String tables come with localization. Until then the key is the text.
            return String(m_Data->Key.ToView());
        case Data::Kind::Invariant:
            return m_Data->Text;
        case Data::Kind::Format:
            return ApplyArgs(m_Data->Pattern.Resolve(), m_Data->Args);
        }
        return String();
    }

    bool DisplayText::IsEmpty() const
    {
        return Resolve().empty();
    }

    DisplayTextArg::DisplayTextArg(const Name t_Key, DisplayText t_Value) : Key(t_Key), Value(std::move(t_Value))
    {
    }

    DisplayTextArg::DisplayTextArg(const Name t_Key, const std::int64_t t_Value) : Key(t_Key)
    {
        char Buffer[24] = {};
        const auto Result = std::to_chars(Buffer, Buffer + sizeof(Buffer), t_Value);
        Value = DisplayText::AsInvariant(String(Buffer, Result.ptr));
    }

    DisplayTextArg::DisplayTextArg(const Name t_Key, const int t_Value)
        : DisplayTextArg(t_Key, static_cast<std::int64_t>(t_Value))
    {
    }

    DisplayTextArg::DisplayTextArg(const Name t_Key, const double t_Value) : Key(t_Key)
    {
        // Number formats per language come with localization. Until then this uses the
        // shortest text that reads back as the same value, with no locale.
        char Buffer[32] = {};
        const auto Result = std::to_chars(Buffer, Buffer + sizeof(Buffer), t_Value);
        Value = DisplayText::AsInvariant(String(Buffer, Result.ptr));
    }
} // namespace Fling
