#pragma once

#include "FlingVulkan.h"
#include "MeshRenderer.h"

namespace Fling
{
    struct MeshRenderer::GpuState
    {
        VkDescriptorSet m_DescriptorSet = VK_NULL_HANDLE;

        /** True when the UBO must be written even if the transform is not dirty */
        bool m_UboDirty = true;

        /** The LocalToWorld version in the UBO. The UBO is written again when it changes. */
        uint32 m_UploadedVersion = 0;
    };

    /** Descriptor set for a mesh, allocated the first time the renderer writes it. */
    inline VkDescriptorSet& MeshRendererDescriptorSet(MeshRenderer& t_Mesh)
    {
        if (t_Mesh.m_GpuState == nullptr)
        {
            t_Mesh.m_GpuState = new MeshRenderer::GpuState();
        }
        return t_Mesh.m_GpuState->m_DescriptorSet;
    }

    /** Force a UBO upload for this mesh on the next frame. */
    inline void MarkMeshRendererUboDirty(MeshRenderer& t_Mesh)
    {
        MeshRendererDescriptorSet(t_Mesh);
        t_Mesh.m_GpuState->m_UboDirty = true;
    }
} // namespace Fling
