#include "pch.h"
#include "Model.h"

namespace Fling
{
	Model::Model(Guid t_ID)
		: Resource(t_ID)
	{
		LoadModel();
	}

	Model::~Model()
	{
		delete m_VertexBuffer;
	}

	void Model::LoadModel()
	{
		const std::string FilePath = GetFilepathReleativeToAssets();

	}

}	// namespace Fling

