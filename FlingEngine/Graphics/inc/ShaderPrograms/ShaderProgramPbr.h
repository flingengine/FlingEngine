#pragma once

#include "MeshRenderer.h"
#include "Lighting/Lighting.hpp"
#include "Components/Transform.h"
#include "Camera.h"

namespace Fling
{
    namespace ShaderProgramPBR
    {
        FORCEINLINE void CreateDescriptorSets(
            MeshRenderer& t_MeshRend, 
            Lighting& m_Lighting,
            VkDescriptorSetLayout m_DescriptorLayout);

        FORCEINLINE void CreateDescriptorPool(MeshRenderer& t_MeshRend);

        FORCEINLINE void BindCmdBuffer(
            MeshRenderer& t_MeshRend,
            VkCommandBuffer t_CommandBuffer,
            GraphicsPipeline* t_GraphicsPipeline,
            UINT32 t_CommandBufferIndex);

        FORCEINLINE void UpdateUniformBuffer(
            MeshRenderer& t_MeshRend, 
            Transform& transform,
            UINT32 t_CurrentImage,
            Camera* t_Camera);
    }
}