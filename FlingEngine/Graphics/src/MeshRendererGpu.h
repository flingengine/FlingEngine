#pragma once

#include "MeshRenderer.h"
#include "FlingVulkan.h"

namespace Fling {
    struct MeshRenderer::GpuState {
        VkDescriptorSet m_DescriptorSet = VK_NULL_HANDLE;
    };

    /** Descriptor set for a mesh, allocated the first time the renderer writes it. */
    inline VkDescriptorSet& MeshRendererDescriptorSet(MeshRenderer& t_Mesh) {
        if (t_Mesh.m_GpuState == nullptr) {
            t_Mesh.m_GpuState = new MeshRenderer::GpuState();
        }
        return t_Mesh.m_GpuState->m_DescriptorSet;
    }
} // namespace Fling
