#pragma once

#include "MeshRenderer.h"
#include "Lighting/Lighting.hpp"
#include "Components/Transform.h"
#include "Camera.h"

namespace Fling
{
    namespace ShaderProgramReflections
    {
        void CreateDescriptorSets(
            MeshRenderer& t_MeshRend,
            Lighting& t_Lighting,
            VkDescriptorImageInfo& t_SkyboxInfo,
            VkDescriptorSetLayout t_DescriptorLayout);

        void CreateDescriptorPool(MeshRenderer& t_MeshRend);

        void BindCmdBuffer(
            MeshRenderer& t_MeshRend,
            VkCommandBuffer t_CommandBuffer,
            GraphicsPipeline* t_GraphicsPipeline,
            UINT32 t_CommandBufferIndex);

        void UpdateUniformBuffer(
            MeshRenderer& t_MeshRend,
            Transform& transform,
            UINT32 t_CurrentImage,
            Camera* t_Camera);

    }
}