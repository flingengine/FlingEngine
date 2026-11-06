#include "Misc/SimulationCheck.h"

namespace Fling
{
    namespace
    {
        thread_local bool t_InSimulation = false;
    } // namespace

    bool SimulationCheck::IsActive()
    {
        return t_InSimulation;
    }

    SimulationCheck::Scope::Scope() : m_WasActive(t_InSimulation)
    {
        t_InSimulation = true;
    }

    SimulationCheck::Scope::~Scope()
    {
        t_InSimulation = m_WasActive;
    }
} // namespace Fling
