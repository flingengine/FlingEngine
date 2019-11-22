#include "Subpass.h"

namespace Fling
{
	Subpass::Subpass(std::shared_ptr<Fling::Shader> t_Vert, std::shared_ptr<Fling::Shader> t_Frag)
		: m_VertexShader(t_Vert)
		, m_FragShader(t_Frag)
	{
	}
}
