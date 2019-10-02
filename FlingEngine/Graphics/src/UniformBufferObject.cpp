#include "pch.h"
#include "UniformBufferObject.h"

namespace Fling
{
	void UboDataDynamic::Release()
	{
		if (Model)
		{
			Fling::AlignedFree(Model);
			Model = nullptr;
		}
		if (View)
		{
			delete View;
			View = nullptr;
		}
		if (Dynamic)
		{
			delete Dynamic;
			Dynamic = nullptr;
		}
	}

	UboDataDynamic::~UboDataDynamic()
	{
		Release();
	}
}   // namespace Fling