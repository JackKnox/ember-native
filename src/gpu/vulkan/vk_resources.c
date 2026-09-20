#include "defines.h"
#include "vk_types.h"

#include <ember/gpu/resources.h>

em_result emgpu_buffer_create(
    emgpu_device* device, 
    em_allocator* allocator, 
    const emgpu_buffer_config* config, 
    emgpu_buffer* out_buffer) {
    vulkan_device* vk_device = (vulkan_device*)device->internal_context;

    out_buffer->internal_data = mem_allocate(allocator, sizeof(vulkan_buffer));
    vulkan_buffer* vk_buffer = (vulkan_buffer*)out_buffer->internal_data;

    VkBufferCreateInfo buffer_create_info = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    buffer_create_info.size  = config->buffer_size;
    buffer_create_info.usage = vulkan_buffer_usage(config->usage);
    
    // TODO: Not so sure about setting this, could simply expose it Ember.
    buffer_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    
    // A buffer is just 'some data', a completely raw, linear array of bytes used for any kind of data.
    // Unlike a image, buffers hold unstructured arrays of bytes, making them ideal for geometric data or arbitrary numbers.
    CHECK_VKRESULT(
        vkCreateBuffer(vk_device->handle, &buffer_create_info, vk_device->allocator, &vk_buffer->handle),
        "Failed to create buffer");
    
    // When you create a buffer, it does not actually assign any physical memory to the object. To make
    // the buffer usage you must query requirements like size, alignment, and compatiable memory type index.
    VkMemoryRequirements memory_requirements = {};
    vkGetBufferMemoryRequirements(vk_device->handle, vk_buffer->handle, &memory_requirements);

    VkMemoryPropertyFlags memory_properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    if (config->usage & EMBER_BUFFER_USAGE_CPU_VISIBLE)
		memory_properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    
    // The memory type index is a identifier that points to a specific configuration of hardware
    // memory, which the GPU stores its local resources.
    i32 memory_index = vulkan_memory_index(vk_device, &memory_requirements, memory_properties);
    if (memory_index == -1) {
        EM_ERROR("Vulkan", "Failed to find required memory type for buffer");
        return EMBER_RESULT_OUT_OF_MEMORY_GPU;
    }

    VkMemoryAllocateInfo memory_allocate_info = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
	memory_allocate_info.allocationSize = memory_requirements.size;
	memory_allocate_info.memoryTypeIndex = memory_index;
    
    // Create a VkDeviceMemory object by filling out a VkMemoryAllocateInfo structure with the byte size and memory type index retrieved.
    CHECK_VKRESULT(
        vkAllocateMemory(vk_device->handle, &memory_allocate_info, vk_device->allocator, &vk_buffer->memory),
        "Failed to allocate local GPU buffer memory");
    
    // Call vkBindBufferMemory to permanently link the VkDeviceMemory block to your VkBuffer handle. This binding cannot be changed for the lifetime of the buffer.
    CHECK_VKRESULT(
        vkBindBufferMemory(vk_device->handle, vk_buffer->handle, vk_buffer->memory, 0),
        "Failed to bind local GPU memory to buffer");

    return EMBER_RESULT_OK;
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
    vulkan_device* vk_device = (vulkan_device*)device->internal_context;

    out_texture->internal_data = mem_allocate(allocator, sizeof(vulkan_texture));
    vulkan_texture* vk_texture = (vulkan_texture*)out_texture->internal_data;

    out_texture->size = config->size;
    out_texture->image_format = config->image_format;
    
    vk_texture->overrided_image = EMFALSE;

    if (config->api_next != NULL) { // vulkan_texture_ext_config*
        vulkan_texture_ext_config* ext = (vulkan_texture_ext_config*)config->api_next;
        vk_texture->handle = ext->image_override;
        vk_texture->overrided_image = EMTRUE;
    }

    if (config->usage & EMBER_TEXTURE_USAGE_SAMPLED) {
        VkSamplerCreateInfo sampler_create_info = { VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
        sampler_create_info.magFilter = vulkan_filter_type(config->filter_type);
        sampler_create_info.minFilter = vulkan_filter_type(config->filter_type);
        sampler_create_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        sampler_create_info.addressModeU = vulkan_address_mode(config->address_mode);
        sampler_create_info.addressModeW = vulkan_address_mode(config->address_mode);
        sampler_create_info.addressModeV = vulkan_address_mode(config->address_mode);
        sampler_create_info.anisotropyEnable = config->max_anisotropy > 0.0f;
        sampler_create_info.maxAnisotropy = config->max_anisotropy;
        sampler_create_info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;

        CHECK_VKRESULT(
            vkCreateSampler(
                vk_device->handle, 
                &sampler_create_info, 
                vk_device->allocator, 
                &vk_texture->sampler), 
            "Failed to create texture Vulkan sampler");
    }

    if (!vk_texture->handle) {
        VkImageCreateInfo image_create_info = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
        image_create_info.imageType = VK_IMAGE_TYPE_2D;
        image_create_info.format = vulkan_format_type(config->image_format);
        image_create_info.extent.width = config->size.x;
        image_create_info.extent.height = config->size.y;
        image_create_info.extent.depth = 1;
        image_create_info.mipLevels = 1;
        image_create_info.arrayLayers = 1;
        image_create_info.samples = VK_SAMPLE_COUNT_1_BIT;
        image_create_info.tiling = VK_IMAGE_TILING_OPTIMAL;
        image_create_info.usage = vulkan_texture_usage(config->usage);
        image_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        image_create_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        CHECK_VKRESULT(
            vkCreateImage(
                vk_device->handle, 
                &image_create_info, 
                vk_device->allocator, 
                &vk_texture->handle),
            "Failed to create Vulkan image");

        // When you create a buffer, it does not actually assign any physical memory to the object. To make
        // the buffer usage you must query requirements like size, alignment, and compatiable memory type index.
        VkMemoryRequirements memory_requirements = {};
        vkGetImageMemoryRequirements(vk_device->handle, vk_texture->handle, &memory_requirements);

        VkMemoryPropertyFlags memory_properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        if (config->usage & EMBER_BUFFER_USAGE_CPU_VISIBLE)
            memory_properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        
        // The memory type index is a identifier that points to a specific configuration of hardware
        // memory, which the GPU stores its local resources.
        i32 memory_index = vulkan_memory_index(vk_device, &memory_requirements, memory_properties);
        if (memory_index == -1) {
            EM_ERROR("Vulkan", "Failed to find required memory type for buffer");
            return EMBER_RESULT_OUT_OF_MEMORY_GPU;
        }

        VkMemoryAllocateInfo memory_allocate_info = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
        memory_allocate_info.allocationSize = memory_requirements.size;
        memory_allocate_info.memoryTypeIndex = memory_index;
        
        // Create a VkDeviceMemory object by filling out a VkMemoryAllocateInfo structure with the byte size and memory type index retrieved.
        CHECK_VKRESULT(
            vkAllocateMemory(vk_device->handle, &memory_allocate_info, vk_device->allocator, &vk_texture->memory),
            "Failed to allocate local GPU buffer memory");
        
        // Call vkBindImageMemory to permanently link the VkDeviceMemory block to your VkImage handle. This binding cannot be changed for the lifetime of the texture.
        CHECK_VKRESULT(
            vkBindImageMemory(vk_device->handle, vk_texture->handle, vk_texture->memory, 0),
            "Failed to bind local GPU memory to buffer");
    }

    VkImageViewCreateInfo view_create_info = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
    view_create_info.image = vk_texture->handle;
    view_create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view_create_info.format = vulkan_format_type(config->image_format);

    view_create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    view_create_info.subresourceRange.baseMipLevel = 0;
    view_create_info.subresourceRange.levelCount = 1;
    view_create_info.subresourceRange.baseArrayLayer = 0;
    view_create_info.subresourceRange.layerCount = 1;

    CHECK_VKRESULT(
        vkCreateImageView(
            vk_device->handle, 
            &view_create_info, 
            vk_device->allocator, 
            &vk_texture->view), 
        "Failed to create image view");

    return EMBER_RESULT_OK;
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
