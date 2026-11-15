#include "Logger.h"
#include "Threading/Lane.h"

#include <atomic>
#include <cassert>

namespace Fling
{
    namespace
    {
        thread_local Lane t_Lane = Lane::None;

        void DefaultViolation(const Lane owner, const Lane current)
        {
            F_LOG_ERROR("A thread on the {} lane touched data that the {} lane owns.",
                        LaneCheck::NameOf(current),
                        LaneCheck::NameOf(owner));
            assert(false && "Lane violation: a thread touched data that another lane owns. See the log.");
        }

        std::atomic<LaneCheck::ViolationHandler> g_Handler{&DefaultViolation};
    } // namespace

    Lane LaneCheck::Current()
    {
        return t_Lane;
    }

    LaneCheck::ViolationHandler LaneCheck::SetViolationHandler(const ViolationHandler handler)
    {
        return g_Handler.exchange(handler != nullptr ? handler : &DefaultViolation, std::memory_order_acq_rel);
    }

    void LaneCheck::ReportViolation(const Lane owner)
    {
        g_Handler.load(std::memory_order_acquire)(owner, t_Lane);
    }

    const char* LaneCheck::NameOf(const Lane lane)
    {
        switch (lane)
        {
        case Lane::Game:
            return "Game";
        case Lane::Frame:
            return "Frame";
        case Lane::None:
        default:
            return "None";
        }
    }

    LaneCheck::Scope::Scope(const Lane lane) : m_Previous(t_Lane)
    {
        t_Lane = lane;
    }

    LaneCheck::Scope::~Scope()
    {
        t_Lane = m_Previous;
    }
} // namespace Fling
