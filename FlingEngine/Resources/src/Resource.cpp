#include "FlingPaths.h"
#include "Logger.h"
#include "Resource.h"
#include "Strings/String.h"

namespace Fling
{
    String Resource::GetFilepathReleativeToAssets() const
    {
#ifndef FLING_SHIPPING
        // A path with the wrong case loads on Windows and fails on Linux. Warn on every
        // platform, so that it is fixed before it reaches a Linux build.
        String OnDisk;
        if (!m_Path.MatchesCaseOnDisk(PathFromUtf8(FlingPaths::EngineAssetsDir()), &OnDisk))
        {
            F_LOG_WARN("Asset path '{}' does not match the case of the file on disk, '{}'", m_Path, OnDisk);
        }
#endif
        return (FlingPaths::EngineAssetsDir() + "/" + m_Path.GetText());
    }
} // namespace Fling
