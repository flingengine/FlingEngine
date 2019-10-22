#include "pch.h"
#include "Sampler.hpp"
#include "Renderer.h"

namespace Fling
{
    void SamplerDeleter::operator()(Sampler* sampler)
    {
    }

    Sampler::~Sampler()
    {
        if (m_Sampler)
        {
            vkDestroySampler(Renderer::GetLogicalVkDevice(), m_Sampler, nullptr);
        }
    }
}   // namespace Fling