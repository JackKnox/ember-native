#include "defines.h"
#include "vk_types.h"

#include <ember/gpu/raster.h>

em_result emgpu_device_create_renderpass(
    emgpu_device* device, 
    em_allocator* allocator, 
    const emgpu_renderpass_config* config, 
    emgpu_renderpass* out_renderpass) {
    
}

void emgpu_device_destroy_renderpass(
    emgpu_device* device, 
    em_allocator* allocator, 
    emgpu_renderpass* renderpass) {
    
}

em_result emgpu_device_create_raster_pipeline(
    emgpu_device* device, 
    em_allocator* allocator, 
    const emgpu_raster_pipeline_config* config, 
    emgpu_renderpass* bound_renderpass, 
    emgpu_pipeline* out_pipeline) {
    
}