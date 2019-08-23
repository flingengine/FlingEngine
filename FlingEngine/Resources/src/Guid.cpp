#include "pch.h"

#include "Guid.h"


namespace Fling
{
	Guid Guid::NewGuid()
	{
		static UINT32 CurIndex = 1;
		Guid G(++CurIndex);
		return G;
	}

	bool Guid::IsValid() const
	{
		return (ID != INVALID_GUID);
	}

	void Guid::Invalidate()
	{
		ID = INVALID_GUID;
	}

	std::string Guid::ToString() const
	{
		return std::to_string(ID);
	}
}