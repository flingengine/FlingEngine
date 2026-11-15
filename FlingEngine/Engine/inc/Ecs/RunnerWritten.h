#pragma once

#include "Components/InputState.h"
#include "Components/PendingInput.h"
#include "Ecs/Access.h"
#include "Ecs/Component.h"
#include "Ecs/Order.h"
#include "Ecs/SimTime.h"
#include "Ecs/TypeHash.h"
#include "Ecs/TypeList.h"

#include <array>
#include <span>

namespace Fling
{
    namespace Detail
    {
        template <UpdateRate R>
        struct RunnerWrittenOf
        {
            using Type = TypeList<SimTime, InputState>;
        };

        template <>
        struct RunnerWrittenOf<UpdateRate::Variable>
        {
            using Type = TypeList<RealFrameTime, PendingInput>;
        };
    } // namespace Detail

    /**
     * The singletons that the runner writes before the systems of one update rate run: the
     * time and the input of the tick (Fixed) or of the frame (Variable). No system writes
     * them (Detail::CheckSystem), so a reader needs no edge to a writer. The runner makes
     * and removes them, the schedule builder lists the fixed ones in Schedule::ResimInputs,
     * and the dump prints them, all from this one list.
     */
    template <UpdateRate R>
    using RunnerWritten = typename Detail::RunnerWrittenOf<R>::Type;

    /** One type of RunnerWritten, as the schedule builder sees it. */
    struct RunnerWrittenType
    {
        TypeHash Hash = 0;
        /** The saved name of the component. */
        const char* Name = nullptr;
    };

    namespace Detail
    {
        template <typename... Ts>
        consteval std::array<RunnerWrittenType, sizeof...(Ts)> RunnerWrittenRows(TypeList<Ts...>)
        {
            return {RunnerWrittenType{TypeHashOf<Ts>, ComponentInfoOf<Ts>.Name}...};
        }

        template <UpdateRate R>
        inline constexpr auto RunnerWrittenTable = RunnerWrittenRows(RunnerWritten<R>{});

        template <typename A, typename... Ts>
        consteval bool WritesAnyOf(TypeList<Ts...>)
        {
            return (Allows<A, AccessMode::Write, Ts> || ...);
        }
    } // namespace Detail

    /** RunnerWritten<rate> as hashes and names, in list order. */
    constexpr std::span<const RunnerWrittenType> RunnerWrittenTypes(const UpdateRate rate)
    {
        if (rate == UpdateRate::Fixed)
        {
            return Detail::RunnerWrittenTable<UpdateRate::Fixed>;
        }
        return Detail::RunnerWrittenTable<UpdateRate::Variable>;
    }

    /** True if the access list A writes a type that the runner writes, at either update rate. */
    template <typename A>
    inline constexpr bool WritesRunnerWritten = Detail::WritesAnyOf<A>(RunnerWritten<UpdateRate::Fixed>{}) ||
                                                Detail::WritesAnyOf<A>(RunnerWritten<UpdateRate::Variable>{});
} // namespace Fling
