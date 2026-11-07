#pragma once

#include "JsonArchive.h"
#include "Strings/Name.h"



namespace Fling
{
	/** The label of an entity, for the editor and for finding it by name. It is not the identity of the entity. */
	struct NameComponent
	{
		Fling::Name Name;

		void Serialize(JsonArchive& Ar)
		{
			Ar << MakeNVP("Name", Name);
		}
	};
}	// namespace Fling
