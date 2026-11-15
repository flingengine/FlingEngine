#include "Logger.h"
#include "Misc/SimulationCheck.h"

#include <atomic>
#include <cassert>

namespace Fling
{
    namespace
    {
        thread_local bool t_InSimulation = false;

        void DefaultViolation(const char* what)
        {
            F_LOG_ERROR("Simulation code called something it must not: {}", what);
            assert(false && "Simulation check: a fixed rate system called frame or wall clock code. See the log.");
        }

        std::atomic<SimulationCheck::ViolationHandler> g_Handler{&DefaultViolation};
    } // namespace

    bool SimulationCheck::IsActive()
    {
        return t_InSimulation;
    }

    SimulationCheck::ViolationHandler SimulationCheck::SetViolationHandler(const ViolationHandler handler)
    {
        return g_Handler.exchange(handler != nullptr ? handler : &DefaultViolation, std::memory_order_acq_rel);
    }

    void SimulationCheck::ReportViolation(const char* what)
    {
        g_Handler.load(std::memory_order_acquire)(what);
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
