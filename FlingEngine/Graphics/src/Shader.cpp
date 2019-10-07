#include "pch.h"
#include "Shader.h"

namespace Fling
{
    Shader::Shader(Guid t_ID)
        : Resource(t_ID)
    {
        Compile();
    }

    std::vector<UINT32> Shader::LoadRawBytes()
    {
        const std::string FilePath = GetFilepathReleativeToAssets();
        std::ifstream File(FilePath, std::ios::ate | std::ios::binary);
        
        std::vector<UINT32> RawBytes;

        if (!File.is_open())
        {
            F_LOG_ERROR("Failed to open file: {}", FilePath);
        }
        else
        {
            size_t Filesize = static_cast<size_t>(File.tellg());
            RawBytes.resize(Filesize);
        
            File.seekg(0);
            File.read((char*)RawBytes.data(), Filesize);
            File.close();
        }

        return RawBytes;
    }

    void Shader::Compile()
    {
        std::vector<UINT32> SpirvCode = LoadRawBytes();

        spirv_cross::Compiler comp(std::move(SpirvCode));
    }
}   // namespace Fling