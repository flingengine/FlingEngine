#pragma once

namespace Fling
{

    struct FlingPaths
    {
        /** Returns directory where engine config files are kept */
        static std::string EngineConfigDir();
        
        /** Returns directory where engine assets are kept */
        static std::string EngineAssetsDir();
        
        /** Returns directory where your current binary is */
        static std::string BinaryDir();

        /** Returns directory where engine log files are kept */
        static std::string EngineLogDir();

        /** Returns directory where the engine source files are kept */
        static std::string EngineSourceDir();

        // TODO: Add options to get the current user's home path

    };

}   // namespace Fling