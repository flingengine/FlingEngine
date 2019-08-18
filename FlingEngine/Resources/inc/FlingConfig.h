#pragma once

#include "Singleton.hpp"
#include "INIReader.h"

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

        std::string GetString(const std::string& t_Section, const std::string& t_Key) const;

        int GetInt(const std::string& t_Section, const std::string& t_Key, const int t_DefaultVal = -1) const;

        bool GetBool(const std::string& t_Section, const std::string& t_Key, const bool t_DefaultVal = false) const;

        float GetFloat(const std::string& t_Section, const std::string& t_Key, const float t_DefaultVal = 0.0f) const;

        double GetDouble(const std::string& t_Section, const std::string& t_Key, const double t_DefaultVal = 0.0) const;

        /**
        * Load in the command line options and store them somewhere that is 
        * globally accessible
        * 
        * @param argc   Argument count
        * @param argv   Command line args
        * @return Number of options loaded
        */
        UINT32 LoadCommandLineOpts( int argc, char* argv[] );
        
    private:

        /** Ini config file reader */
        static INIReader m_IniReader;

    };

}   // namespace Fling