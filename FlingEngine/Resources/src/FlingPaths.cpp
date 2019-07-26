#include "pch.h"
#include "FlingPaths.h"

namespace Fling
{
    std::string FlingPaths::EngineConfigDir()
    {
        return "Config";
    }

    std::string FlingPaths::EngineAssetsDir()
    {
        return "Assets";
    }

    std::string FlingPaths::BinaryDir()
    {
        return "";
    }
    
    std::string FlingPaths::EngineLogDir()
    {
        return "Logs";
    }
    
    std::string FlingPaths::EngineSourceDir()
    {
        return "";
    }
}   // namespace Fling 