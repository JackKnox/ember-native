#include "defines.h"
#include "vk_types.h"

#include <ember/gpu/resources.h>

em_result emgpu_device_create_buffer(
    emgpu_device* device, 
    em_allocator* allocator, 
    const emgpu_buffer_config* config, 
    emgpu_buffer* out_buffer) {
    
}

em_result emgpu_device_copy_buffer(
    emgpu_device* device, 
    emgpu_buffer* src_buffer,
    emgpu_buffer* dst_buffer, 
    u64 src_offset, u64 dst_offset, 
    u64 region) {
    
}

em_result emgpu_device_upload_to_buffer(
    emgpu_device* device, 
    emgpu_buffer* buffer, 
    const void* data, 
    u64 offset, u64 region) {
    
}

void emgpu_device_destroy_buffer(
    emgpu_device* device, 
    em_allocator* allocator, 
    emgpu_buffer* buffer) {
    
}

em_result emgpu_device_create_texture(
    emgpu_device* device, 
    em_allocator* allocator, 
    const emgpu_texture_config* config, 
    emgpu_texture* out_texture) {
    
}

em_result emgpu_device_upload_to_texture(
    emgpu_device* device, 
    emgpu_texture* texture, 
    const void* data, 
    uvec2 start_offset, 
    uvec2 region) {
    
}

void emgpu_device_destroy_texture(
    emgpu_device* device, 
    em_allocator* allocator, 
    emgpu_texture* texture) {
    
}

em_result emgpu_device_update_pipeline_descriptors(
    emgpu_device* device, 
    emgpu_pipeline* pipeline, 
    emgpu_update_descriptors* descriptors, 
    u32 descriptor_count) {
    
}
    
void emgpu_device_destroy_pipeline(
    emgpu_device* device, 
    em_allocator* allocator, 
    emgpu_pipeline* pipeline) {
    
}