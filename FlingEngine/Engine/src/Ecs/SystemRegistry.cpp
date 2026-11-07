#include "Ecs/SystemRegistry.h"

namespace Fling
{
    std::vector<const SystemDesc*> SystemRegistry::Descs() const
    {
        std::vector<const SystemDesc*> descs;
        descs.reserve(m_Systems.size());
        for (const std::unique_ptr<ISystem>& system : m_Systems)
        {
            descs.push_back(&system->Describe());
        }
        return descs;
    }

    ScheduleResult SystemRegistry::BuildSchedule() const
    {
        const std::vector<const SystemDesc*> descs = Descs();
        return Fling::BuildSchedule(ScheduleInput{descs, m_Phases, m_Orders});
    }
} // namespace Fling
