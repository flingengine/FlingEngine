#pragma once

#include "LaunchOptions.h"
#include "Misc/CommandLine.h"
#include "Strings/String.h"

#include <catch2/catch_all.hpp>
#include <vector>

namespace TestArgs
{
    /** Read launch flags from t_Args, as the engine reads its command line (the executable name is added). */
    inline bool ParseLaunchArgs(std::vector<const char*> t_Args, Fling::LaunchOptions& t_Out, Fling::String& t_OutError)
    {
        t_Args.insert(t_Args.begin(), "FlingEngine.exe");
        REQUIRE(Fling::CommandLine::Get().Init(static_cast<int>(t_Args.size()), t_Args.data()));
        return Fling::LaunchOptions::Parse(Fling::CommandLine::Get(), t_Out, t_OutError);
    }
} // namespace TestArgs
