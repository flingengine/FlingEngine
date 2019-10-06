//#pragma once
//#include "Buffer.h"
//
//namespace Fling
//{
//    /**
//     * @brief Access to vulkan buffer backed up by device memory
//     * 
//     */
//    class FLING_API MappedBuffer : public Buffer
//    {
//	public:
//		
//		MappedBuffer() {};
//
//		void CreateBuffer(
//			const VkDeviceSize& t_size,
//			const VkBufferUsageFlags& t_Usage,
//			const VkMemoryPropertyFlags& t_Properties,
//			const void* t_Data = nullptr);
//
//
//		void* m_mapped = nullptr;
//		
//        void MapMemory(VkDeviceSize t_size = VK_WHOLE_SIZE, VkDeviceSize t_offset = 0);
//        void UnmapMemory();
//        /**
//         * @brief Flush a memory range of the buffer to make it visible to the device 
//         * 
//         * @param size (optional) size of the memory range to invalidate  
//         * @param offset (optional) byte offset from the begginning
//         */
//        void Flush(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
//        /**
//         * @brief 
//         * 
//         * @param size (optional) size of the memory range to invalidate  
//         * @param offset (optional) byte offset from the begginning
//         */
//        void Invalidate(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
//    };
//}