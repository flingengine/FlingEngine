#pragma once

#include "Singleton.hpp"
#include "INIReader.h"
#include <regex>

namespace Fling
{
    /**
    * Provide simple access to engine configuration options from an INI file
    * #TODO Parse command line options as well
    */
    class FlingConfig : public Singleton<FlingConfig>
    {

    public:

        virtual void Init() override;

        virtual void Shutdown() override;

        /**
        * Attempt to load a config file (.ini) for the engine
        * 
        * @param t_File     File path to the config file
        * 
        * @return True if file was read successfully
        */
        bool LoadConfigFile(const std::string& t_File);

        std::string GetString(const std::string& t_Section, const std::string& t_Key);

        int GetInt(const std::string& t_Section, const std::string& t_Key);

        bool GetBool(const std::string& t_Section, const std::string& t_Key);

        float GetFloat(const std::string& t_Section, const std::string& t_Key);

        double GetDouble(const std::string& t_Section, const std::string& t_Key);

        UINT32 LoadCommandLineOpts( int argc, char* argv[] );

        int GetCmdLineInt(const std::string& t_Key);

        bool GetCmdLineBool(const std::string& t_Key);

        double GetCmdLineDouble(const std::string& t_Key);

        float GetCmdLineFloat(const std::string& t_Key);

        const std::string& GetCmdLineString(const std::string& t_Key);

    private:

        /** Ini config file reader */
        static INIReader m_IniReader;

        /** A map of the passed in command line vars */
        std::unordered_map<std::string, std::string> m_CommandLineVars;

    };

}   // namespace Fling