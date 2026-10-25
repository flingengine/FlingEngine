#include "Input/InputKey.h"
#include <array>
#include <string_view>
#include <unordered_map>

namespace Fling
{
    namespace
    {
        constexpr std::size_t KeyCount = static_cast<std::size_t>(InputKeyId::Count);

        /** One entry for each InputKeyId. Index 0 is "Invalid". */
        const std::array<InputKeyDetails, KeyCount>& GetKeyTable()
        {
            static const std::array<InputKeyDetails, KeyCount> Table = {{
                {"Invalid", "Invalid", InputKeyFlags::None},
#define FLING_INPUT_KEY(Id, DisplayName, Flags) {#Id, DisplayName, Flags},
#include "Input/InputKeyList.inl"
#undef FLING_INPUT_KEY
            }};
            return Table;
        }
    } // namespace

    const InputKeyDetails& InputKey::GetDetails() const
    {
        const std::size_t Index = static_cast<std::size_t>(m_Id);
        if (Index == 0 || Index >= KeyCount)
        {
            return GetKeyTable()[0];
        }
        return GetKeyTable()[Index];
    }

    InputKey InputKey::FromName(std::string_view t_Name)
    {
        static const std::unordered_map<std::string_view, InputKeyId> NameMap = []
        {
            std::unordered_map<std::string_view, InputKeyId> Map;
            const auto& Table = GetKeyTable();
            for (std::size_t i = 1; i < KeyCount; ++i)
            {
                Map.emplace(Table[i].Name, static_cast<InputKeyId>(i));
            }
            return Map;
        }();

        const auto Found = NameMap.find(t_Name);
        if (Found == NameMap.end())
        {
            return InputKey();
        }
        return InputKey(Found->second);
    }
} // namespace Fling
