#pragma once

#include "AssetPath.h"
#include "FlingTypes.h"
#include "Platform.h"
#include "Strings/String.h"


#include <utility>

namespace Fling
{
	/**
	* Base class that represents a loaded resource in the engine
	*/
	class Resource
	{
		friend class ResourceManager;

	public:
        explicit Resource(AssetPath t_Path) : m_Path(std::move(t_Path)) {}

        virtual ~Resource() = default;

		/** The id of this resource. Use it to pass the resource around and to look it up. */
		AssetId GetAssetId() const { return m_Path.GetId(); }

		/** The canonical path of this resource, relative to its root folder. */
		const AssetPath& GetAssetPath() const { return m_Path; }

        /**
         * Returns the full file path that is relative to the assets path based on the path of this resource.
         */
        String GetFilepathReleativeToAssets() const;

    protected:

        AssetPath m_Path;
	};
}	// namespace Fling