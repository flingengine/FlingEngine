#include "ShaderPrograms/ShaderProgramReflections.h"
#include "GraphicsHelpers.h"

void Fling::ShaderProgramReflections::CreateDescriptorSets(
    MeshRenderer& t_MeshRend, 
    Lighting& t_Lighting, 
    VkDescriptorSetLayout t_DescriptorLayout)
{
	//const std::vector<VkImage> Images;
 //   VkDevice Device = VK_NULL_HANDLE;

 //   // Specify what descriptor pool to allocate from and how many
 //   std::vector<VkDescriptorSetLayout> layouts(Images.size(), t_DescriptorLayout);
 //   VkDescriptorSetAllocateInfo allocInfo = {};
 //   allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
 //   allocInfo.descriptorPool = VK_NULL_HANDLE;
 //   allocInfo.descriptorSetCount = static_cast<uint32>(Images.size());
 //   allocInfo.pSetLayouts = layouts.data();

 //   t_MeshRend.m_DescriptorSet.resize(Images.size());

 //   // Sets will be cleaned up when the descriptor pool is, no need for an explicit free call in cleanup
 //   if (vkAllocateDescriptorSets(Device, &allocInfo, t_MeshRend.m_DescriptorSet.data()) != VK_SUCCESS)
 //   {
 //       F_LOG_FATAL("Failed to allocate descriptor sets!");
 //   }

 //   for (size_t i = 0; i < Images.size(); ++i)
 //   {
 //       std::vector<VkWriteDescriptorSet> descriptorWrites;

 //       //VkWriteDescriptorSet uniformSet = Initializers::WriteDescriptorSetUniform(
 //       //    t_MeshRend.m_UniformBuffer[i],
 //       //    t_MeshRend.m_DescriptorSets[i],
 //       //    0
 //       //);

 //       VkWriteDescriptorSet lightUniformSet = Initializers::WriteDescriptorSetUniform(
 //           t_Lighting.m_LightingUBOs[i],
 //           t_MeshRend.m_DescriptorSet[i],
 //           2
 //       );

 //       //VkWriteDescriptorSet skyboxImageSampelr = Initializers::WriteDescriptorSet(
 //       //    t_MeshRend.m_DescriptorSets[i],
 //       //    VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
 //       //    4,
 //       //    &skyboxImageInfo);

 //       //descriptorWrites.push_back(uniformSet);
 //       descriptorWrites.push_back(lightUniformSet);
 //       //descriptorWrites.push_back(skyboxImageSampelr);

 //       vkUpdateDescriptorSets(Device, static_cast<uint32>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
 //   }
}

void Fling::ShaderProgramReflections::CreateDescriptorPool(MeshRenderer& t_MeshRend)
{

}

void Fling::ShaderProgramReflections::BindCmdBuffer(
    MeshRenderer& t_MeshRend, 
    VkCommandBuffer t_CommandBuffer, 
    GraphicsPipeline* t_GraphicsPipeline, 
    uint32 t_CommandBufferIndex)
{
    //Fling::Model* Model = t_MeshRend.m_Model;
    //VkBuffer vertexBuffers[1] = { Model->GetVertexBuffer()->GetVkBuffer() };
    //VkDeviceSize offsets[1] = { 0 };

    //// Bind the descriptor set for rendering a mesh using the dynamic offset
    //vkCmdBindDescriptorSets(
    //    t_CommandBuffer,
    //    VK_PIPELINE_BIND_POINT_GRAPHICS,
    //    t_GraphicsPipeline->GetPipelineLayout(),
    //    0,
    //    1,
    //    &t_MeshRend.m_DescriptorSet[t_CommandBufferIndex],
    //    0,
    //    nullptr);

    //vkCmdBindVertexBuffers(t_CommandBuffer, 0, 1, vertexBuffers, offsets);
    //vkCmdBindIndexBuffer(t_CommandBuffer, Model->GetIndexBuffer()->GetVkBuffer(), 0, Model->GetIndexType());
    //vkCmdDrawIndexed(t_CommandBuffer, Model->GetIndexCount(), 1, 0, 0, 0);
}

void Fling::ShaderProgramReflections::UpdateUniformBuffer(
    MeshRenderer& t_MeshRend, 
    Transform& transform, 
    uint32 t_CurrentImage, 
    Camera* t_Camera)
{
    //Camera* camera = t_Camera;
    //Transform::CalculateWorldMatrix(transform);

    //// Calculate the world matrix based on the given transform
    //UboVS ubo = {};
    //ubo.Model = transform.GetWorldMat();
    //ubo.View = camera->GetViewMatrix();
    //ubo.Projection = camera->GetProjectionMatrix();
    //ubo.Projection[1][1] *= -1.0f;
    //ubo.CamPos = camera->GetPosition();
    //ubo.ObjPos = transform.GetPos();

    //// Copy the ubo to the GPU
    //Buffer* buf = t_MeshRend.m_UniformBuffers[t_CurrentImage];
    //memcpy(buf->m_MappedMem, &ubo, buf->GetSize());
}