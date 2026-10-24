#include "Resource.h"
#include "FlingPaths.h"

namespace Fling
{
    std::string Resource::GetFilepathReleativeToAssets() const
    {
        return (FlingPaths::EngineAssetsDir() + "/" + GetGuidString());
    }
}