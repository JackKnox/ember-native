#include "defines.h"
#include "vk_types.h"

#include "utils/darray.h"

#include <ember/gpu/raster.h>

em_result emgpu_raster_pipeline_create(
    emgpu_device* device, 
    em_allocator* allocator, 
    const emgpu_raster_pipeline_config* config, 
    emgpu_pipeline* out_pipeline) {
    vulkan_context* context = (vulkan_context*)device->internal_context;

    out_pipeline->internal_data = mem_allocate(allocator, sizeof(vulkan_pipeline));
    vulkan_pipeline* vk_pipeline = (vulkan_pipeline*)out_pipeline->internal_data;
    
    // Marks this as a raster pipeline.
    out_pipeline->type = EMBER_OPER_TYPE_RASTER;
    
    const u32 shader_stage_count = 2; // Includes vertex and fragment shaders.
    VkPipelineShaderStageCreateInfo shader_stages[shader_stage_count] = {};
    
    emgpu_raster_blend_config em_blend_config = (config->blend_state != NULL ? *config->blend_state : emgpu_raster_blend_default());
    
    // This is for one attachment on the bound renderpass.
    // TODO: Allow for multiple attachments or wait for Vulkan to promote dynamic rendering to core.
    VkPipelineColorBlendAttachmentState colour_attachment = {};
    colour_attachment.blendEnable = (config->blend_state != NULL);
    colour_attachment.srcColorBlendFactor = vulkan_blend_factor_type(em_blend_config.src_colour);
    colour_attachment.dstColorBlendFactor = vulkan_blend_factor_type(em_blend_config.dst_colour);
    colour_attachment.colorBlendOp = vulkan_blend_op_type(em_blend_config.colour_op);
    colour_attachment.srcAlphaBlendFactor = vulkan_blend_factor_type(em_blend_config.src_alpha);
    colour_attachment.dstAlphaBlendFactor = vulkan_blend_factor_type(em_blend_config.dst_alpha);
    colour_attachment.alphaBlendOp = vulkan_blend_op_type(em_blend_config.alpha_op);
    colour_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    
    // This structure describes blending state, which is seperate to all the other renderpass 
    // transitions and image layouts at the top of this file. It describes what should happen when 
    // two primitive with this same pipeline intersect, usually it blendes the colour together for transpanracy. 
    VkPipelineColorBlendStateCreateInfo blend_state = { VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
    blend_state.attachmentCount = 1;
    blend_state.pAttachments    = &colour_attachment;

    emgpu_raster_vertex_config em_vertex_config = (config->vertex_input != NULL ? *config->vertex_input : emgpu_raster_vertex_default());
    
    // Vertex attributes are used to describe the layout of any assigned vertex buffer to the pipeline.
    // It describes vector, matrices, interger, floats etc. It takes the vertex buffer, gets the current vertex
    // and then interpretes it as the attributes given here.
    VkVertexInputAttributeDescription* attributes = darray_reserve(VkVertexInputAttributeDescription, em_vertex_config.attribute_count, NULL);
    u64 attribute_stride = 0;

    for (u32 i = 0; i < em_vertex_config.attribute_count; ++i) {
        emgpu_format attribute = em_vertex_config.attributes[i];

        VkVertexInputAttributeDescription* descriptor = darray_push_empty(attributes);
        descriptor->location = i; // Describes which ith attribute it is.
        // TODO: Allow for multiple vertex buffers in Ember.
        descriptor->binding  = 0; // Descibes which vertex buffer as pipeline can more than one vertex buffer.
        descriptor->format   = vulkan_format_type(attribute);
        descriptor->offset   = attribute_stride;

        if (descriptor->format == VK_FORMAT_UNDEFINED) {
            EM_ERROR("Vulkan", "Unsupported format in vertex attributes");
            return EMBER_RESULT_UNSUPPORTED_FORMAT;
        }

        attribute_stride += EMBER_FORMAT_SIZE(attribute);
    }
    
    // This describes how the GPU should read a vertex buffer, this is different to vertex
    // attributes as it describes a region of a vertex buffer (as said in `binding`) that
    // has attributes in it and gives it a "input rate". This is how it advnaced:
    //     VK_VETREX_INPUT_RATE_VERTEX - This means the data gets moved to the next set of data
    //     per vertex. This is default so each vertex gets its own set of data which makes sense.
    //
    //     VK_VETREX_INPUT_RATE_INSTANCE - In this case data gets advanced per instance, this 
    //     is used when you do instanced rendering when its the same vertex for all instances 
    //     so the other input rate wouldnt work here. This is used for instance-specific data.
    VkVertexInputBindingDescription binding_desc = {};
    binding_desc.binding = 0; 
    binding_desc.stride  = attribute_stride;
    binding_desc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkPipelineVertexInputStateCreateInfo vertex_input_state = { VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
    vertex_input_state.vertexAttributeDescriptionCount = darray_length(attributes);
    vertex_input_state.pVertexAttributeDescriptions    = attributes;
    vertex_input_state.vertexBindingDescriptionCount   = 1;
    vertex_input_state.pVertexBindingDescriptions      = &binding_desc;

    // Input assembly state is how the GPU interpretes the vertices and how it defines connections between
    // the raw vertices to draw actual shapes. It tells Vulkan how to group vertices into primitives (triangles, lines, points, etc.).
    VkPipelineInputAssemblyStateCreateInfo input_assembly = { VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
    switch (em_vertex_config.topology) {
        case EMBER_PRIMITIVE_TYPE_POINT_LIST:     input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
        case EMBER_PRIMITIVE_TYPE_LINE_LIST:      input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
        case EMBER_PRIMITIVE_TYPE_LINE_STRIP:     input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
        case EMBER_PRIMITIVE_TYPE_TRIANGLE_LIST:  input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        case EMBER_PRIMITIVE_TYPE_TRIANGLE_STRIP: input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
    }

    VkFormat* colour_formats = darray_reserve(VkFormat, config->colour_attachment_count, allocator);
    
    for (u32 i = 0; i < config->colour_attachment_count; ++i) {
        VkFormat format = vulkan_format_type(config->colour_attachments[i]);
        if (format == VK_FORMAT_UNDEFINED) {
            EM_ERROR("Vulkan", "Unsupported format in colour attachments");
            return EMBER_RESULT_UNSUPPORTED_FORMAT;
        }

        darray_push(colour_formats, format);
    }

    VkPipelineRenderingCreateInfo rendering_info = { VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO };
    rendering_info.colorAttachmentCount = darray_length(colour_formats);
    rendering_info.pColorAttachmentFormats = colour_formats;

    // Dynamic state enables certain values to Vulkan at command buffer record-time. This
    // is so the pipeline isn't coupled to a certain window size or etc.
    VkDynamicState dynamic_states[] = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR,
        VK_DYNAMIC_STATE_LINE_WIDTH
    };

    VkPipelineDynamicStateCreateInfo dynamic_state = { VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
    dynamic_state.dynamicStateCount = EM_ARRAYSIZE(dynamic_states);
    dynamic_state.pDynamicStates    = dynamic_states;
    
    // Fill create info.
    VkGraphicsPipelineCreateInfo pipeline_create_info = { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
    pipeline_create_info.stageCount = shader_stage_count;
    pipeline_create_info.pStages    = shader_stages;

    pipeline_create_info.pVertexInputState = &vertex_input_state;
    pipeline_create_info.pInputAssemblyState = &input_assembly;
    pipeline_create_info.pColorBlendState = &blend_state;
    pipeline_create_info.pDynamicState = &dynamic_state;
    
    pipeline_create_info.pNext = &rendering_info;

    // A shader module is just a container for the SPIR-V shader, it has to be created so it can
    // be compiled and possibly cached by the user.
    em_result result = vulkan_create_shader_stage(device, allocator, &config->vertex_shader, VK_SHADER_STAGE_VERTEX_BIT, &shader_stages[0]);
    if (result != EMBER_RESULT_OK) return result;
    
    // A shader module is just a container for the SPIR-V shader, it has to be created so it can
    // be compiled and possibly cached by the user.
    result = vulkan_create_shader_stage(device, allocator, &config->fragment_shader, VK_SHADER_STAGE_FRAGMENT_BIT, &shader_stages[1]);
    if (result != EMBER_RESULT_OK) return result;
    
    // The pipeline layout is the interface between the shader stages and the resources that act as input/output. 
    // Your shaders often need access to large data blocks like uniform buffers, storage buffers, and image samplers. Instead of 
    // binding these individually, Vulkan groups them into Descriptor Sets. The VkPipelineLayout doesn’t store the actual data; 
    // instead, it defines an array of Descriptor Set Layouts, telling the pipeline exactly what kind of resources to expect and in what order.
    result = vulkan_create_pipeline_layout(device, allocator, config->descriptors, config->descriptor_count, out_pipeline);
   
    pipeline_create_info.layout = vk_pipeline->layout;

    // Bundles everything together into a pipeline object. It supports creating many pipelines at one as its a expensive
    // operation. Its also uses a pipeline cache which allows the GPU to store and reuse compiled shader code.
    CHECK_VKRESULT(
        vkCreateGraphicsPipelines(context->device.handle, NULL, 1, &pipeline_create_info, context->allocator, &vk_pipeline->handle),
        "Failed to create raster pipeline");

    for (u32 i = 0; EM_ARRAYSIZE(shader_stages); ++i)
        vkDestroyShaderModule(context->device.handle, shader_stages[i].module, context->allocator);
    darray_destroy(colour_formats);
    darray_destroy(attributes);
    return EMBER_RESULT_OK;
}
