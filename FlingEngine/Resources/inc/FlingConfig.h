#pragma once

#include "Singleton.hpp"
#include "INIReader.h"

namespace Fling
{
    /**
    * Provide simple access to engine configuration options from an INI file
    */
    class FlingConfig : public Singleton<FlingConfig>
    {

    public:

        virtual void Init() override;

        virtual void Shutdown() override;

        bool LoadConfigFile(const std::string& t_File);

        std::string GetString(const std::string& t_Section, const std::string& t_Key);

        int GetInt(const std::string& t_Section, const std::string& t_Key);

        bool GetBool(const std::string& t_Section, const std::string& t_Key);

        float GetFloat(const std::string& t_Section, const std::string& t_Key);

        double GetDouble(const std::string& t_Section, const std::string& t_Key);

    private:

        static INIReader m_IniReader;

    };

}   // namespace Fling