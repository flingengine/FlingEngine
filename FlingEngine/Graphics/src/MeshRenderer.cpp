#include "pch.h"
#include "MeshRenderer.h"
#include "Model.h"

namespace Fling
{
	bool MeshRenderer::operator==(const MeshRenderer& other) const
	{
		return m_ModelMatrixOffset == other.m_ModelMatrixOffset && m_Model == other.m_Model;
	}

	bool MeshRenderer::operator!=(const MeshRenderer& other) const
	{
		return !(*this == other);
	}

	void MeshRenderer::Initalize(Model* t_Model, UINT32 t_ModelMatrixOffset)
	{
		m_Model = t_Model;
		m_ModelMatrixOffset = t_ModelMatrixOffset;
	}
}   // namespace Fling