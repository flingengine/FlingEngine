#include "AssetPath.h"

#include <algorithm>
#include <cctype>
#include <system_error>
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
    bool AssetPath::MatchesCaseOnDisk(const std::filesystem::path& t_Root, String* t_OutOnDisk) const
    {
        auto EqualIgnoringCase = [](const StringView t_A, const StringView t_B)
        {
            return t_A.size() == t_B.size() && std::equal(t_A.begin(),
                                                          t_A.end(),
                                                          t_B.begin(),
                                                          [](const char t_X, const char t_Y) {
                                                              return std::tolower(static_cast<unsigned char>(t_X)) ==
                                                                     std::tolower(static_cast<unsigned char>(t_Y));
                                                          });
        };

        std::filesystem::path Folder = t_Root;
        String OnDisk;
        bool bMatches = true;
        StringView Rest = m_Text;
        // The parts not found on disk, which the suggested path keeps as they are
        StringView Unchecked;
        while (!Rest.empty())
        {
            Unchecked = Rest;
            const std::size_t Slash = Rest.find('/');
            const StringView Part = Rest.substr(0, Slash);
            Rest = Slash == StringView::npos ? StringView() : Rest.substr(Slash + 1);

            // Compare with the names in the folder, because on Windows a path with the
            // wrong case still exists.
            String Found;
            std::error_code Error;
            for (const std::filesystem::directory_entry& Entry : std::filesystem::directory_iterator(Folder, Error))
            {
                const String EntryName = PathToUtf8(Entry.path().filename());
                if (EntryName == Part)
                {
                    Found = EntryName;
                    break;
                }
                if (Found.empty() && EqualIgnoringCase(EntryName, Part))
                {
                    Found = EntryName;
                }
            }
            if (Found.empty())
            {
                // The part does not exist with any case. Loading reports that.
                break;
            }

            bMatches = bMatches && Found == Part;
            OnDisk += (OnDisk.empty() ? "" : "/") + Found;
            Folder /= PathFromUtf8(Found);
            Unchecked = StringView();
        }

        if (!bMatches && t_OutOnDisk)
        {
            *t_OutOnDisk = OnDisk + (Unchecked.empty() ? "" : "/" + String(Unchecked));
        }
        return bMatches;
    }
} // namespace Fling
