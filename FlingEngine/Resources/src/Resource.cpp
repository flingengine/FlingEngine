#include "FlingPaths.h"
#include "Resource.h"

namespace Fling
{
    std::string Resource::GetFilepathReleativeToAssets() const
    {
        return (FlingPaths::EngineAssetsDir() + "/" + m_Path.GetText());
    }
}