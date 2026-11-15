#pragma once

#include "AssetPath.h"
#include "FlingVulkan.h"
#include "Material.h"
#include "MeshProxyTable.h"
#include "Model.h"

namespace Fling
{
    class Buffer;

    /** The GPU side of one mesh proxy in one render pass. Indexed by MeshProxyTable slot. */
    struct GpuMesh
    {
        Model* MeshModel = nullptr;
        Material* MeshMaterial = nullptr;
        Buffer* Ubo = nullptr;
        VkDescriptorSet Set = VK_NULL_HANDLE;

        /** The world matrix version in the UBO. 0 means write it on the next frame. */
        uint32 UploadedVersion = 0;

        /** True if this pass draws the mesh (its material type belongs to the pass) */
        bool Drawn = false;
    };

    /**
     * Load the model and material of a proxy from the resource manager. An empty
     * material path uses the default material.
     * @return False if the mesh path is empty, so there is nothing to draw
     */
    inline bool ResolveMeshAssets(const MeshProxyTable::Slot& t_Slot, GpuMesh& t_Out)
    {
        t_Out.MeshModel = nullptr;
        t_Out.MeshMaterial = nullptr;
        if (t_Slot.Mesh.empty())
        {
            return false;
        }
        t_Out.MeshModel = Model::Create(AssetPath(t_Slot.Mesh)).get();
        t_Out.MeshMaterial = t_Slot.Material.empty() ? Material::GetDefaultMat().get()
                                                     : Material::Create(AssetPath(t_Slot.Material)).get();
        return t_Out.MeshModel != nullptr && t_Out.MeshMaterial != nullptr;
    }
} // namespace Fling
