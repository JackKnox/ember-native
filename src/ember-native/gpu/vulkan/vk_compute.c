#include "defines.h"
#include "vk_types.h"

#include <ember/gpu/compute.h>

em_result emgpu_compute_pipeline_create(
    emgpu_device* device, 
    em_allocator* allocator, 
    const emgpu_compute_pipeline_config* config, 
    emgpu_pipeline* out_compute_pipeline) {
    vulkan_context* context = (vulkan_context*)device->internal_context;

    out_compute_pipeline->internal_data = mem_allocate(allocator, sizeof(vulkan_pipeline));
    vulkan_pipeline* vk_pipeline = (vulkan_pipeline*)out_compute_pipeline->internal_data;
    
    // Marks this as a compute pipeline.
    out_compute_pipeline->type = EMBER_OPER_TYPE_COMPUTE;

    VkComputePipelineCreateInfo pipeline_create_info = { VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO };
    
    em_result result = vulkan_create_shader_stage(device, allocator, &config->shader, VK_SHADER_STAGE_COMPUTE_BIT, &pipeline_create_info.stage);
    if (result != EMBER_RESULT_OK) return result;

    // The pipeline layout is the interface between the shader stages and the resources that act as input/output. 
    // Your shaders often need access to large data blocks like uniform buffers, storage buffers, and image samplers. Instead of 
    // binding these individually, Vulkan groups them into Descriptor Sets. The VkPipelineLayout doesn’t store the actual data; 
    // instead, it defines an array of Descriptor Set Layouts, telling the pipeline exactly what kind of resources to expect and in what order.
    result = vulkan_create_pipeline_layout(device, allocator, config->descriptors, config->descriptor_count, out_compute_pipeline);
    if (result != EMBER_RESULT_OK) return result;
    
    pipeline_create_info.layout = vk_pipeline->layout;

    // Bundles everything together into a pipeline object. It supports creating many pipelines at one as its a expensive
    // operation. Its also uses a pipeline cache which allows the GPU to store and reuse compiled shader code.
    CHECK_VKRESULT(
        vkCreateComputePipelines(context->device.handle, NULL, 1, &pipeline_create_info, context->allocator, &vk_pipeline->handle),
        "Failed to create compute pipeline");

    vkDestroyShaderModule(context->device.handle, pipeline_create_info.stage.module, context->allocator);
    return EMBER_RESULT_OK;
}
