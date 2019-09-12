#include "pch.h"
#include "ResourceManager.h"

namespace Fling
{
	void ResourceManager::Init()
	{
		// Ensure "Current Directory" (relative path) is always the .exe's folder
		// - Without this, the relative path is different when running through VS
		//    and when running the .exe directly, which makes it a pain to load files
		//    - Running through VS: Current Dir is the *project folder*
		//    - Running from .exe:  Current Dir is the .exe's folder
		// - This has nothing to do with DEBUG and RELEASE modes - it's purely a 
		//    Visual Studio "thing", and isn't obvious unless you know to look 
		//    for it.  In fact, it could be fixed by changing a setting in VS, but
		//    the option is stored in a user file (.suo), which is ignored by most
		//    version control packages by default.  Meaning: the option must be
		//    changed every on every PC.  Ugh.  So instead, I fixed it here.
		// - This is a new change this year to simplify a long-standing headache.  
		//    If it breaks something on your end, feel free to comment this section out

		char currentDir[1024] = {};
		FlingPaths::GetCurrentWorkingDir(currentDir, 1024);
	}

	void ResourceManager::Shutdown()
	{
		// Unload all assets BB
		// This will remove all owning references to the shared_ptr's
		m_ResourceMap.clear();
	}

	std::shared_ptr<Resource> ResourceManager::GetResource(Guid_Handle t_ID) const
	{
		ResourceMapConstIt It = m_ResourceMap.find(t_ID);
		if (It != m_ResourceMap.end())
		{
			return It->second;
		}

		return nullptr;
	}

	bool ResourceManager::IsLoaded(Guid_Handle t_ID) const
	{
        return (m_ResourceMap.find(t_ID) != m_ResourceMap.end());
	}
}	// namespace Fling