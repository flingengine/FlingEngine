#include "Ecs/RunnerWritten.h"
#include "Ecs/Schedule.h"
#include "ScheduleBuilder.h"
#include "Strings/String.h"

// Schedule::Dump: a text listing of a schedule. It prints fields of the schedule and computes nothing.

namespace Fling
{
    namespace
    {
        using ScheduleBuild::NameOf;

        String Pad(String text, const std::size_t width)
        {
            if (text.size() < width)
            {
                text.append(width - text.size(), ' ');
            }
            return text + " ";
        }

        const char* KindText(const ComponentKind kind)
        {
            switch (kind)
            {
            case ComponentKind::Table:
                return "table";
            case ComponentKind::Tag:
                return "tag";
            case ComponentKind::Singleton:
                return "singleton";
            case ComponentKind::Queue:
                return "queue";
            }
            return "component";
        }

        String NameList(const Schedule& schedule, const std::vector<uint32>& systems)
        {
            String out;
            for (const uint32 i : systems)
            {
                out += (out.empty() ? "" : ", ") + String(NameOf(schedule.Systems[i]->Info.Name));
            }
            return out;
        }

        /** The groups of one update rate: each under its parent, the children in run order of their begin. */
        void DumpGroups(const Schedule& schedule, const UpdateRate rate, String& out)
        {
            out += "  groups:\n";
            const auto addGroup = [&](const auto& self, const uint32 g, const std::size_t depth) -> void
            {
                const ScheduleGroup& group = schedule.Groups[g];
                out += String(4 + 2 * depth, ' ') + group.Info->Name + (group.Resim ? "  (resim)\n" : "\n");
                for (uint32 child = 0; child < schedule.Groups.size(); ++child)
                {
                    if (schedule.Groups[child].Parent == g)
                    {
                        self(self, child, depth + 1);
                    }
                }
            };
            for (uint32 g = 0; g < schedule.Groups.size(); ++g)
            {
                if (schedule.Groups[g].Rate == rate && schedule.Groups[g].Parent == Schedule::NoGroup)
                {
                    addGroup(addGroup, g, 0);
                }
            }
        }

        /** One system: its stage, group, and writes, then why it runs after others, and its late reads. */
        void DumpSystem(
            const Schedule& schedule, const uint32 i, const std::size_t stage, const UpdateRate rate, String& out)
        {
            const SystemDesc& desc = *schedule.Systems[i];
            String line = "  stage " + std::to_string(stage) + "  " + Pad(desc.Info.Name, 28) +
                          Pad("[" + String(schedule.Groups[schedule.SystemGroup[i]].Info->Name) + "]", 26);
            String writes;
            for (const AccessEntry& entry : desc.Access)
            {
                if (entry.Mode != AccessMode::Read)
                {
                    writes += (writes.empty() ? "" : ", ") + String(entry.Name);
                }
            }
            if (!writes.empty())
            {
                line += "writes " + writes;
            }
            while (!line.empty() && line.back() == ' ')
            {
                line.pop_back();
            }
            out += line + "\n";

            for (const ScheduleEdge& edge : schedule.Edges)
            {
                if (edge.To == i)
                {
                    out += "             after " + String(schedule.Systems[edge.From]->Info.Name) + ": " +
                           ScheduleBuild::EdgeReasonText(edge) + "\n";
                }
            }

            const String late =
                String("             one ") + (rate == UpdateRate::Fixed ? "tick" : "frame") + " late: ";
            for (const ScheduleLag& lag : schedule.Lags)
            {
                if (lag.Late == i)
                {
                    const String source = schedule.Systems[lag.Source]->Info.Name;
                    out += late + (lag.Queue
                                       ? "consumes " + String(lag.TypeName) + " before " + source + " appends to it\n"
                                       : "reads " + String(lag.TypeName) + " before " + source + " writes it\n");
                }
            }
        }

        /** What a resimulation runs, and what it reads that only the systems it skips make. */
        void DumpResim(const Schedule& schedule, String& out)
        {
            const String runs = NameList(schedule, schedule.ResimRunOrder());
            out += "  resim runs: " + (runs.empty() ? String("nothing") : runs) + "\n";
            for (const ResimInput& input : schedule.ResimInputs)
            {
                const String writers = NameList(schedule, input.Writers);
                const String from =
                    input.FromRunner ? (writers.empty() ? String("the runner") : "the runner, " + writers) : writers;
                out += "  resim reads from outside: " + String(NameOf(input.Name)) + " (from " + from + ")\n";
            }
        }

        void DumpReadOnly(const Schedule& schedule, String& out)
        {
            out += "Singletons that no system writes (the runner makes each with its default values, unless it is "
                   "External)\n";
            for (const ReadOnlySingleton& singleton : schedule.ReadOnlySingletons)
            {
                out += "  " + String(NameOf(singleton.Name)) + " (singleton), read by " +
                       NameList(schedule, singleton.Readers) + "\n";
            }
        }

        void DumpPublished(const Schedule& schedule, String& out)
        {
            out += "Published each tick, for Order::Frame\n";
            if (schedule.Published.empty())
            {
                out += "  nothing\n";
            }
            for (const PublishedType& type : schedule.Published)
            {
                out += "  " + String(NameOf(type.Name)) + " (" + KindText(type.Kind) + "), read by " +
                       NameList(schedule, type.Readers);
                if (type.IsBlended())
                {
                    out += "; held for the blend of " + NameList(schedule, type.BlendReaders);
                }
                out += "\n";
            }
        }
    } // namespace

    String Schedule::Dump() const
    {
        String out;
        for (std::size_t r = 0; r < UpdateRateCount; ++r)
        {
            const UpdateRate rate = static_cast<UpdateRate>(r);
            const std::vector<ScheduleSegment>& segments = GetSegments(rate);
            if (segments.empty())
            {
                continue;
            }
            out += String(UpdateRateName(rate)) +
                   (rate == UpdateRate::Fixed ? " (once per tick)\n" : " (once per frame)\n");
            out += "  the runner writes first:";
            const char* separator = " ";
            for (const RunnerWrittenType& type : RunnerWrittenTypes(rate))
            {
                out += separator + String(type.Name);
                separator = ", ";
            }
            out += "\n";
            DumpGroups(*this, rate, out);
            for (const ScheduleSegment& segment : segments)
            {
                for (std::size_t s = 0; s < segment.Stages.size(); ++s)
                {
                    for (const uint32 i : segment.Stages[s].Systems)
                    {
                        DumpSystem(*this, i, s, rate, out);
                    }
                }
                if (segment.FlushGroup != nullptr)
                {
                    out += "  flush: end of group " + String(segment.FlushGroup->Name) + "\n";
                }
            }
            if (rate == UpdateRate::Fixed)
            {
                out += "  flush: end of tick\n";
                DumpResim(*this, out);
            }
        }
        if (!ReadOnlySingletons.empty())
        {
            DumpReadOnly(*this, out);
        }
        if (!Systems.empty())
        {
            DumpPublished(*this, out);
        }
        return out;
    }
} // namespace Fling
