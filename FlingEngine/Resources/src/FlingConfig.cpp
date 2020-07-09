#include "pch.h"
#include "FlingConfig.h"

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

    bool FlingConfig::LoadConfigFile(const std::string& t_File)
    {
        m_IniReader = INIReader(t_File);

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

    std::string FlingConfig::GetStringImpl(const std::string& t_Section, const std::string& t_Key) const
    {
        return m_IniReader.Get(t_Section, t_Key, "UNKNOWN");
    }

    int FlingConfig::GetIntImpl(const std::string& t_Section, const std::string& t_Key, const int t_DefaultVal/*=-1*/) const
    {
        return m_IniReader.GetInteger(t_Section, t_Key, t_DefaultVal);
    }

    bool FlingConfig::GetBoolImpl(const std::string& t_Section, const std::string& t_Key, const bool t_DefaultVal /* =false */) const
    {
        return m_IniReader.GetBoolean(t_Section, t_Key, t_DefaultVal);
    }

    float FlingConfig::GetFloatImpl(const std::string& t_Section, const std::string& t_Key, const float t_DefaultVal /*=0.0f*/) const
    {
        return static_cast<float>(m_IniReader.GetReal(t_Section, t_Key, t_DefaultVal));
    }

    double FlingConfig::GetDoubleImpl(const std::string& t_Section, const std::string& t_Key, const double t_DefaultVal /*= 0.0*/) const
    {
        return m_IniReader.GetReal(t_Section, t_Key, t_DefaultVal);
    }

    //////////////////////////////////////////////////////////////////////////
    // Command line parsing

    uint32 FlingConfig::LoadCommandLineOpts(int argc, char* argv[])
    {
        uint32 ArgsLoaded = 0;
        
        // TODO: Use regex to try and parse out if things are a key/val etc

        for (int i = 0; i < argc; ++i)
        {
            // Parse out if this is a key or not
            std::string value = argv[i];
            (void)(value);
        }

        return ArgsLoaded;
    }

}   // namespace Fling