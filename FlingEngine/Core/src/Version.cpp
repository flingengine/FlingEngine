#include "Strings/String.h"
#include "Version.h"
namespace Fling
{
    Version Version::EngineVersion = {0, 0, 1};

    String Version::ToString() const
    {
        String VersionString = std::to_string(Major) + "." + std::to_string(Minor) + "." + std::to_string(Patch);
        return VersionString;
    }

} // namespace Fling