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

		static std::string GetString(const std::string& t_Section, const std::string& t_Key) { return FlingConfig::Get().GetStringImpl(t_Section, t_Key); }

		static int GetInt(const std::string& t_Section, const std::string& t_Key, const int t_DefaultVal = -1) { return FlingConfig::Get().GetIntImpl(t_Section, t_Key); }

		static bool GetBool(const std::string& t_Section, const std::string& t_Key, const bool t_DefaultVal = false) { return FlingConfig::Get().GetBoolImpl(t_Section, t_Key); }

		static float GetFloat(const std::string& t_Section, const std::string& t_Key, const float t_DefaultVal = 0.0f) { return FlingConfig::Get().GetFloatImpl(t_Section, t_Key); }

		static double GetDouble(const std::string& t_Section, const std::string& t_Key, const double t_DefaultVal = 0.0) { return FlingConfig::Get().GetDoubleImpl(t_Section, t_Key); }

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

		std::string GetStringImpl(const std::string& t_Section, const std::string& t_Key) const;

		int GetIntImpl(const std::string& t_Section, const std::string& t_Key, const int t_DefaultVal = -1) const;

		bool GetBoolImpl(const std::string& t_Section, const std::string& t_Key, const bool t_DefaultVal = false) const;

		float GetFloatImpl(const std::string& t_Section, const std::string& t_Key, const float t_DefaultVal = 0.0f) const;

		double GetDoubleImpl(const std::string& t_Section, const std::string& t_Key, const double t_DefaultVal = 0.0) const;

    };

}   // namespace Fling