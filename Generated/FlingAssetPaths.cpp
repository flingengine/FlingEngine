#include "pch.h"
#include "FlingPaths.h"

namespace Fling
{
    const std::string& FlingPaths::BinaryDir()
    {
    #ifdef FLING_SHIPPING
        static std::string BinDir = "";
    #else
        static std::string BinDir = "";
    #endif
        return BinDir;
    }
    
    const std::string& FlingPaths::EngineAssetsDir()
    {
    #ifdef FLING_SHIPPING
        static std::string AssetPath = "Assets";
    #else
        static std::string AssetPath = "C:/Users/artur/OneDrive/Desktop/IS/FlingEngine/Assets";
    #endif
        return AssetPath;
    }

    const std::string& FlingPaths::EngineLogDir()
    {
    #ifdef FLING_SHIPPING
        static std::string LogPath = "Logs";
    #else
        static std::string LogPath = "C:/Users/artur/OneDrive/Desktop/IS/FlingEngine/Logs";
    #endif
        return LogPath;
    }

    const std::string& FlingPaths::EngineConfigDir()
    {
    #ifdef FLING_SHIPPING
        static std::string ConfigPath = "Config";
    #else
        static std::string ConfigPath = "C:/Users/artur/OneDrive/Desktop/IS/FlingEngine/Config";
    #endif
        return ConfigPath;
    }

    const std::string& FlingPaths::EngineSourceDir()
    {
    #ifdef FLING_SHIPPING
        static std::string SourceDir = "";
    #else
        static std::string SourceDir = "C:/Users/artur/OneDrive/Desktop/IS/FlingEngine/FlingEngine";
    #endif
        return SourceDir;
    }
}   // Fling
