#include "ShaderPrograms\ShaderProgramPbr.h"
#include "GraphicsHelpers.h"
#include "Renderer.h"

namespace Fling
{
    namespace ShaderProgramPBR
    {
        void CreateDescriptorSets(
            MeshRenderer& t_MeshRend,
            Lighting& m_Lighting,
            VkDescriptorSetLayout m_DescriptorLayout)
        {
            const std::vector<VkImage>& Images = Renderer::Get().GetSwapChain()->GetImages();
            VkDevice Device = Renderer::Get().GetLogicalVkDevice();

            // Specify what descriptor pool to allocate from and how many
            std::vector<VkDescriptorSetLayout> layouts(Images.size(), m_DescriptorLayout);
            VkDescriptorSetAllocateInfo allocInfo = {};
            allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
            allocInfo.descriptorPool = t_MeshRend.m_DescriptorPool;
            allocInfo.descriptorSetCount = static_cast<UINT32>(Images.size());
            allocInfo.pSetLayouts = layouts.data();

            t_MeshRend.m_DescriptorSets.resize(Images.size());

            // Sets will be cleaned up when the descriptor pool is, no need for an explicit free call in cleanup
            if (vkAllocateDescriptorSets(Device, &allocInfo, t_MeshRend.m_DescriptorSets.data()) != VK_SUCCESS)
            {
                F_LOG_FATAL("Failed to allocate descriptor sets!");
            }


            for (size_t i = 0; i < Images.size(); ++i)
            {
                std::vector<VkWriteDescriptorSet> descriptorWrites;


                VkWriteDescriptorSet uniformSet = Initializers::WriteDescriptorSetUniform(
                    t_MeshRend.m_UniformBuffers[i],
                    t_MeshRend.m_DescriptorSets[i],
                    0
                );

                VkWriteDescriptorSet albedoSet = Initializers::WriteDescriptorSetImage(
                    t_MeshRend.m_Material->GetTexture().m_AlbedoTexture,
                    t_MeshRend.m_DescriptorSets[i],
                    2);

                VkWriteDescriptorSet normalMapSet = Initializers::WriteDescriptorSetImage(
                    t_MeshRend.m_Material->GetTexture().m_NormalTexture,
                    t_MeshRend.m_DescriptorSets[i],
                    3);

                VkWriteDescriptorSet metallicSet = Initializers::WriteDescriptorSetImage(
                    t_MeshRend.m_Material->GetTexture().m_MetalTexture,
                    t_MeshRend.m_DescriptorSets[i],
                    4);

                VkWriteDescriptorSet roughnessSet = Initializers::WriteDescriptorSetImage(
                    t_MeshRend.m_Material->GetTexture().m_RoughnessTexture,
                    t_MeshRend.m_DescriptorSets[i],
                    5);

                VkWriteDescriptorSet BRDFLookupSet = Initializers::WriteDescriptorSetImage(
                    t_MeshRend.m_Material->GetTexture().m_RoughnessTexture,
                    t_MeshRend.m_DescriptorSets[i],
                    7);

                VkWriteDescriptorSet lightUniformSet = Initializers::WriteDescriptorSetUniform(
                    m_Lighting.m_LightingUBOs[i],
                    t_MeshRend.m_DescriptorSets[i],
                    6
                );

                descriptorWrites.push_back(uniformSet);
                descriptorWrites.push_back(albedoSet);
                descriptorWrites.push_back(normalMapSet);
                descriptorWrites.push_back(metallicSet);
                descriptorWrites.push_back(roughnessSet);
                descriptorWrites.push_back(BRDFLookupSet);
                descriptorWrites.push_back(lightUniformSet);

                vkUpdateDescriptorSets(Device, static_cast<UINT32>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
            }
        }

        void CreateDescriptorPool(MeshRenderer& t_MeshRend)
        {

            const UINT32 SwapImageCount = static_cast<UINT32>(Renderer::Get().GetSwapChain()->GetImageCount());
            VkDevice Device = Renderer::Get().GetLogicalVkDevice();

            UINT32 DescriptorCount = 128;

            std::vector<VkDescriptorPoolSize> poolSizes =
            {
                Initializers::DescriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, DescriptorCount),
                Initializers::DescriptorPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, DescriptorCount),
                Initializers::DescriptorPoolSize(VK_DESCRIPTOR_TYPE_SAMPLER, DescriptorCount),
                Initializers::DescriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, DescriptorCount),
                Initializers::DescriptorPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, DescriptorCount)
            };

            VkDescriptorPoolCreateInfo poolInfo = {};
            poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
            poolInfo.poolSizeCount = static_cast<UINT32>(poolSizes.size());
            poolInfo.pPoolSizes = poolSizes.data();
            poolInfo.maxSets = SwapImageCount;


            if (vkCreateDescriptorPool(Device, &poolInfo, nullptr, &t_MeshRend.m_DescriptorPool) != VK_SUCCESS)
            {
                F_LOG_FATAL("Failed to create descriptor pool");
            }
        }

        void BindCmdBuffer(
            MeshRenderer& t_MeshRend,
            VkCommandBuffer t_CommandBuffer,
            GraphicsPipeline* t_GraphicsPipeline,
            UINT32 t_CommandBufferIndex)
        {
            Fling::Model* Model = t_MeshRend.m_Model;
            VkBuffer vertexBuffers[1] = { Model->GetVertexBuffer()->GetVkBuffer() };
            VkDeviceSize offsets[1] = { 0 };

            // Bind the descriptor set for rendering a mesh using the dynamic offset
            vkCmdBindDescriptorSets(
                t_CommandBuffer,
                VK_PIPELINE_BIND_POINT_GRAPHICS,
                t_GraphicsPipeline->GetPipelineLayout(),
                0,
                1,
                &t_MeshRend.m_DescriptorSets[t_CommandBufferIndex],
                0,
                nullptr);

            vkCmdBindVertexBuffers(t_CommandBuffer, 0, 1, vertexBuffers, offsets);
            vkCmdBindIndexBuffer(t_CommandBuffer, Model->GetIndexBuffer()->GetVkBuffer(), 0, Model->GetIndexType());
            vkCmdDrawIndexed(t_CommandBuffer, Model->GetIndexCount(), 1, 0, 0, 0);
        }

        void UpdateUniformBuffer(
            MeshRenderer& t_MeshRend,
            Transform& transform,
            UINT32 t_CurrentImage,
            Camera* t_Camera)
        {
            Camera* camera = t_Camera;
            Transform::CalculateWorldMatrix(transform);

            // Calculate the world matrix based on the given transform
            UboVS ubo = {};
            ubo.Model = transform.GetWorldMat();
            ubo.View = camera->GetViewMatrix();
            ubo.Projection = camera->GetProjectionMatrix();
            ubo.Projection[1][1] *= -1.0f;
            ubo.CamPos = camera->GetPosition();
            ubo.ObjPos = transform.GetPos();

            // Copy the ubo to the GPU
            Buffer* buf = t_MeshRend.m_UniformBuffers[t_CurrentImage];
            memcpy(buf->m_MappedMem, &ubo, buf->GetSize());
        }

    }
}
