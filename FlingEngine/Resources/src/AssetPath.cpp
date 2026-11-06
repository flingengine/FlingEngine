#include "AssetPath.h"

#include <vector>

namespace Fling
{
    AssetPath::AssetPath(const StringView t_Text) : m_Text(Canonicalize(t_Text))
    {
        if (!m_Text.empty())
        {
            m_Id = AssetId(Name(m_Text));
        }
    }

    String AssetPath::Canonicalize(const StringView t_Text)
    {
        // Split on both kinds of slash, then join the segments that are left.
        std::vector<StringView> Segments;
        std::size_t Start = 0;
        while (Start <= t_Text.size())
        {
            std::size_t End = t_Text.find_first_of("/\\", Start);
            if (End == StringView::npos)
            {
                End = t_Text.size();
            }

            const StringView Segment = t_Text.substr(Start, End - Start);
            if (Segment == "..")
            {
                if (!Segments.empty() && Segments.back() != "..")
                {
                    Segments.pop_back();
                }
                else
                {
                    // Nothing to remove: keep it, so the path still says where it points.
                    Segments.push_back(Segment);
                }
            }
            else if (!Segment.empty() && Segment != ".")
            {
                Segments.push_back(Segment);
            }

            Start = End + 1;
        }

        String Result;
        Result.reserve(t_Text.size());
        for (const StringView Segment : Segments)
        {
            if (!Result.empty())
            {
                Result += '/';
            }
            Result += Segment;
        }
        return Result;
    }
} // namespace Fling
