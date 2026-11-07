#include "FlingConfig.h"
#include "Logger.h"

namespace Fling
{

    INIReader FlingConfig::m_IniReader;

    void FlingConfig::Init()
    {
    }

    void FlingConfig::Shutdown()
    {

    }

    //////////////////////////////////////////////////////////////////////////
    // Config file settings

    bool FlingConfig::LoadConfigFile(const StringView t_File)
    {
        m_IniReader = INIReader(std::string(t_File));

        if (m_IniReader.ParseError() != 0)
        {
            F_LOG_ERROR("Cannot load config file: {} " , t_File);
            return false;
        }
        else
        {
            F_LOG_TRACE("Loaded config file {}", t_File);
        }
        return true;
    }

    std::string FlingConfig::GetStringImpl(const StringView t_Section, const StringView t_Key, const StringView t_Default) const
    {
        return m_IniReader.Get(std::string(t_Section), std::string(t_Key), std::string(t_Default));
    }

    int FlingConfig::GetIntImpl(const StringView t_Section, const StringView t_Key, const int t_DefaultVal/*=-1*/) const
    {
        return m_IniReader.GetInteger(std::string(t_Section), std::string(t_Key), t_DefaultVal);
    }

    bool FlingConfig::GetBoolImpl(const StringView t_Section, const StringView t_Key, const bool t_DefaultVal /* =false */) const
    {
        return m_IniReader.GetBoolean(std::string(t_Section), std::string(t_Key), t_DefaultVal);
    }

    float FlingConfig::GetFloatImpl(const StringView t_Section, const StringView t_Key, const float t_DefaultVal /*=0.0f*/) const
    {
        return static_cast<float>(m_IniReader.GetReal(std::string(t_Section), std::string(t_Key), t_DefaultVal));
    }

    double FlingConfig::GetDoubleImpl(const StringView t_Section, const StringView t_Key, const double t_DefaultVal /*= 0.0*/) const
    {
        return m_IniReader.GetReal(std::string(t_Section), std::string(t_Key), t_DefaultVal);
    }   

}   // namespace Fling