#include "defines.h"
#include "vk_types.h"

#include <ember/gpu/resources.h>

em_result emgpu_buffer_create(
    emgpu_device* device, 
    em_allocator* allocator, 
    const emgpu_buffer_config* config, 
    emgpu_buffer* out_buffer) {
    vulkan_context* context = (vulkan_context*)device->internal_context;

    out_buffer->internal_data = mem_allocate(allocator, sizeof(vulkan_buffer));
    vulkan_buffer* vk_buffer = (vulkan_buffer*)out_buffer->internal_data;

    VkBufferCreateInfo buffer_create_info = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    buffer_create_info.size = config->buffer_size;
    
    // TODO: Not so sure about setting this, could simply expose it Ember.
    buffer_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    
    if (config->usage & EMBER_BUFFER_USAGE_VERTEX)  buffer_create_info.usage |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    if (config->usage & EMBER_BUFFER_USAGE_INDEX)   buffer_create_info.usage |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    if (config->usage & EMBER_BUFFER_USAGE_UNIFORM) buffer_create_info.usage |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    if (config->usage & EMBER_BUFFER_USAGE_STORAGE) buffer_create_info.usage |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    // TODO: Just add a transfer destination flag to Ember?
    if (config->usage & EMBER_BUFFER_USAGE_TRANSFER_SRC) buffer_create_info.usage |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    
    // A buffer is just 'some data', a completely raw, linear array of bytes used for any kind of data.
    // Unlike a image, buffers hold unstructured arrays of bytes, making them ideal for geometric data or arbitrary numbers.
    CHECK_VKRESULT(
        vkCreateBuffer(context->device.handle, &buffer_create_info, context->allocator, &vk_buffer->handle),
        "Failed to create buffer");
    
    // When you create a buffer, it does not actually assign any physical memory to the object. To make
    // the buffer usage you must query requirements like size, alignment, and compatiable memory type index.
    VkMemoryRequirements memory_requirements = {};
    vkGetBufferMemoryRequirements(context->device.handle, vk_buffer->handle, &memory_requirements);

    VkMemoryPropertyFlags memory_properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    if (config->usage & EMBER_BUFFER_USAGE_CPU_VISIBLE)
		memory_properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    
    // The memory type index is a identifier that points to a specific configuration of hardware
    // memory, which the GPU stores its local resources.
    i32 memory_index = vulkan_memory_index(context, &memory_requirements, memory_properties);
    if (memory_index == -1) {
        EM_ERROR("Vulkan", "Failed to find required memory type for buffer");
        return EMBER_RESULT_OUT_OF_MEMORY_GPU;
    }

    VkMemoryAllocateInfo memory_allocate_info = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
	memory_allocate_info.allocationSize = memory_requirements.size;
	memory_allocate_info.memoryTypeIndex = memory_index;
    
    // Create a VkDeviceMemory object by filling out a VkMemoryAllocateInfo structure with the byte size and memory type index retrieved.
    CHECK_VKRESULT(
        vkAllocateMemory(context->device.handle, &memory_allocate_info, context->allocator, &vk_buffer->memory),
        "Failed to allocate local GPU buffer memory");
    
    // Call vkBindBufferMemory to permanently link the VkDeviceMemory block to your VkBuffer handle. This binding cannot be changed for the lifetime of the buffer.
    CHECK_VKRESULT(
        vkBindBufferMemory(context->device.handle, vk_buffer->handle, vk_buffer->memory, 0),
        "Failed to bind local GPU memory to buffer");

    return EMBER_RESULT_OK;
}

em_result emgpu_buffer_copy(
    emgpu_device* device, 
    emgpu_buffer* src_buffer,
    emgpu_buffer* dst_buffer, 
    u64 src_offset, u64 dst_offset, 
    u64 region) {
    
}

em_result emgpu_buffer_upload(
    emgpu_device* device, 
    emgpu_buffer* buffer, 
    const void* data, 
    u64 offset, u64 region) {
    
}

void emgpu_buffer_destroy(
    emgpu_device* device, 
    em_allocator* allocator, 
    emgpu_buffer* buffer) {
    
}

em_result emgpu_texture_create(
    emgpu_device* device, 
    em_allocator* allocator, 
    const emgpu_texture_config* config, 
    emgpu_texture* out_texture) {
    
}

em_result emgpu_texture_upload(
    emgpu_device* device, 
    emgpu_texture* texture, 
    const void* data, 
    uvec2 start_offset, 
    uvec2 region) {
    
}

void emgpu_texture_destroy(
    emgpu_device* device, 
    em_allocator* allocator, 
    emgpu_texture* texture) {
    
}

em_result emgpu_pipeline_upload_descriptors(
    emgpu_device* device, 
    emgpu_pipeline* pipeline, 
    emgpu_update_descriptors* descriptors, 
    u32 descriptor_count) {
    
}
    
void emgpu_pipeline_destroy(
    emgpu_device* device, 
    em_allocator* allocator, 
    emgpu_pipeline* pipeline) {
    
}
