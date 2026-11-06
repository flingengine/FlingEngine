#include "Strings/DisplayText.h"

#include <atomic>
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
        /** Invariant text, or the source text of a table entry. */
        String Text;
        DisplayText Pattern;
        std::vector<DisplayTextArg> Args;
    };

    namespace
    {
        std::atomic<DisplayTextTableResolver> g_TableResolver {nullptr};
        std::atomic<TextDirection> g_Direction {TextDirection::LeftToRight};

        /** U+2068 FIRST STRONG ISOLATE and U+2069 POP DIRECTIONAL ISOLATE, in UTF-8. */
        constexpr StringView FirstStrongIsolate = "\xE2\x81\xA8";
        constexpr StringView PopDirectionalIsolate = "\xE2\x81\xA9";

        /** Appends the resolved text of the argument with this key, or returns false. */
        bool AppendArg(String& t_Out, const StringView t_Key, const std::vector<DisplayTextArg>& t_Args)
        {
            for (const DisplayTextArg& Arg : t_Args)
            {
                if (Arg.Key.ToView() == t_Key)
                {
                    if (g_Direction.load(std::memory_order_acquire) == TextDirection::RightToLeft)
                    {
                        t_Out += FirstStrongIsolate;
                        t_Out += Arg.Value.Resolve();
                        t_Out += PopDirectionalIsolate;
                    }
                    else
                    {
                        t_Out += Arg.Value.Resolve();
                    }
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

    void SetDisplayTextTableResolver(const DisplayTextTableResolver t_Resolver)
    {
        g_TableResolver.store(t_Resolver, std::memory_order_release);
    }

    void SetDisplayTextDirection(const TextDirection t_Direction)
    {
        g_Direction.store(t_Direction, std::memory_order_release);
    }

    TextDirection GetDisplayTextDirection()
    {
        return g_Direction.load(std::memory_order_acquire);
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
