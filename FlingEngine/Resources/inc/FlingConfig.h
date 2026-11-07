#pragma once

#include "FlingWarnings.h"
#include "Singleton.hpp"
#include "Strings/String.h"

#include <string>

// inih's header-only parser uses a deliberately bounded strncpy. SYSTEM includes
// don't hide it because GCC reports it after inlining into our own code (seen on MinGW).
F_WARNING_PUSH
F_GCC_DISABLE_WARNING("-Wstringop-truncation")
#include "INIReader.h"
F_WARNING_POP

namespace Fling
{
    /**
    * Provide simple access to engine configuration options from an INI file.
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
        bool LoadConfigFile(StringView t_File);

		static std::string GetString(StringView t_Section, StringView t_Key, StringView t_Default = "INVALID") { return FlingConfig::Get().GetStringImpl(t_Section, t_Key, t_Default); }

		static int GetInt(StringView t_Section, StringView t_Key, const int t_DefaultVal = -1) { return FlingConfig::Get().GetIntImpl(t_Section, t_Key, t_DefaultVal); }

		static bool GetBool(StringView t_Section, StringView t_Key, const bool t_DefaultVal = false) { return FlingConfig::Get().GetBoolImpl(t_Section, t_Key, t_DefaultVal); }

		static float GetFloat(StringView t_Section, StringView t_Key, const float t_DefaultVal = 0.0f) { return FlingConfig::Get().GetFloatImpl(t_Section, t_Key, t_DefaultVal); }

		static double GetDouble(StringView t_Section, StringView t_Key, const double t_DefaultVal = 0.0) { return FlingConfig::Get().GetDoubleImpl(t_Section, t_Key, t_DefaultVal); }
        
    private:

        /** Ini config file reader */
        static INIReader m_IniReader;

		std::string GetStringImpl(StringView t_Section, StringView t_Key, StringView t_Default) const;

		int GetIntImpl(StringView t_Section, StringView t_Key, const int t_DefaultVal = -1) const;

		bool GetBoolImpl(StringView t_Section, StringView t_Key, const bool t_DefaultVal = false) const;

		float GetFloatImpl(StringView t_Section, StringView t_Key, const float t_DefaultVal = 0.0f) const;

		double GetDoubleImpl(StringView t_Section, StringView t_Key, const double t_DefaultVal = 0.0) const;

    };

}   // namespace Fling