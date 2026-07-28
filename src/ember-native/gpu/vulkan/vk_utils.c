#include "defines.h"
#include "vk_types.h"

#include "utils/darray.h"

i32 vulkan_memory_index(vulkan_context* context, VkMemoryRequirements* requirements, VkMemoryPropertyFlags flags) {
    VkPhysicalDeviceMemoryProperties device_memories = {};
    vkGetPhysicalDeviceMemoryProperties(context->device.physical, &device_memories);

    for (u32 i = 0; i < device_memories.memoryTypeCount; ++i) {
        if (requirements->memoryTypeBits & (1 << i) && (device_memories.memoryTypes[i].propertyFlags & (u32)flags) == flags)
            return i;
    }

    EM_ERROR("Vulkan", "Cannot find suitable memory type for GPU domain memory");
    return -1;
}

VkDescriptorType vulkan_descriptor_type(emgpu_descriptor_type type) {
    switch (type) {
        case EMBER_DESCRIPTOR_TYPE_STORAGE_BUFFER: return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        case EMBER_DESCRIPTOR_TYPE_STORAGE_IMAGE:  return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        case EMBER_DESCRIPTOR_TYPE_UNIFORM_BUFFER: return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        case EMBER_DESCRIPTOR_TYPE_SAMPLED_IMAGE:  return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        default:
            EM_ASSERT(EMFALSE);
            break;
    }

    return 0;
}

VkShaderStageFlags vulkan_shader_stage_type(emgpu_shader_stage_type stage_type) {
    switch (stage_type) {
        case EMBER_SHADER_STAGE_TYPE_VERTEX:   return VK_SHADER_STAGE_VERTEX_BIT;
        case EMBER_SHADER_STAGE_TYPE_FRAGMENT: return VK_SHADER_STAGE_FRAGMENT_BIT;
        case EMBER_SHADER_STAGE_TYPE_COMPUTE:  return VK_SHADER_STAGE_COMPUTE_BIT;
        default:
            EM_ASSERT(EMFALSE);
            break;
    }

    return 0;
}

em_result vulkan_create_pipeline_layout(emgpu_device* device, em_allocator* allocator, const emgpu_descriptor_desc* descriptors, u32 descriptor_count, emgpu_pipeline* out_pipeline) {
    vulkan_context* context = (vulkan_context*)device->internal_context;

    vulkan_pipeline* vk_pipeline = (vulkan_pipeline*)out_pipeline->internal_data;

    if (descriptor_count > 0) {
        VkDescriptorSetLayoutBinding* descriptor_bindings = darray_reserve(VkDescriptorSetLayoutBinding, descriptor_count, allocator, MEMORY_TAG_RENDERER);

        for (u32 i = 0; i < descriptor_count; ++i) {
            VkDescriptorSetLayoutBinding* binding = darray_push_empty(descriptor_bindings);
            binding->binding = descriptors[i].binding;
            binding->descriptorType = vulkan_descriptor_type(descriptors[i].descriptor_type);
            binding->descriptorCount = 1;
            binding->stageFlags = vulkan_shader_stage_type(descriptors[i].stage_type);
        }

        VkDescriptorSetLayoutCreateInfo layout_create_info = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
		layout_create_info.bindingCount = darray_length(descriptor_bindings);
		layout_create_info.pBindings    = descriptor_bindings;
		CHECK_VKRESULT(
            vkCreateDescriptorSetLayout(context->device.handle, &layout_create_info, context->allocator, &vk_pipeline->descriptor_layout),
            "Failed to create Vulkan descriptor set layout when creating pipeline");

        darray_destroy(descriptor_bindings);
    }

    VkPipelineLayoutCreateInfo layout_create_info = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
    layout_create_info.setLayoutCount = 1;
    layout_create_info.pSetLayouts = &vk_pipeline->descriptor_layout;
    CHECK_VKRESULT(
        vkCreatePipelineLayout(context->device.handle, &layout_create_info, context->allocator, &vk_pipeline->layout),
        "Failed to create pipeline layout");

    return EMBER_RESULT_OK;
}

em_result vulkan_create_shader_stage(emgpu_device* device, em_allocator* allocator, const emgpu_shader_src* shader, VkShaderStageFlags shader_type, VkPipelineShaderStageCreateInfo* out_shader_stage) {
    vulkan_context* context = (vulkan_context*)device->internal_context;

    if (!shader->data || !shader->size) {
        EM_ERROR("Vulkan", "Invalid shader source in pipeline.");
        return EMBER_RESULT_INVALID_VALUE;
    }

    if (!shader->entry_point) {
        EM_ERROR("Vulkan", "Invalid entry point name in pipeline.");
        return EMBER_RESULT_INVALID_VALUE;
    }
    
    // A shader module is just a container for the SPIR-V shader, it has to be created so it can
    // be compiled and possibly cached by the user (us). The `shader_stage_info` is how it connects
    // to the pipeline plus stage type and specialization info.
    VkPipelineShaderStageCreateInfo shader_stage_info = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
    shader_stage_info.stage = shader_type;
    shader_stage_info.pName = shader->entry_point;
    
    VkShaderModuleCreateInfo module_create_info = { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
    module_create_info.codeSize = shader->size;
    module_create_info.pCode = shader->data;

    CHECK_VKRESULT(
        vkCreateShaderModule(context->device.handle, &module_create_info, context->allocator, &shader_stage_info.module),
        "Failed to create compute shader module");

    return EMBER_RESULT_OK;
}

void vulkan_device_from_capabilities(vulkan_phys_device* curr_device, emgpu_device_capabilities* out_capabilities) {
    VkPhysicalDeviceProperties properties = {};
    vkGetPhysicalDeviceProperties(curr_device->handle, &properties);

    VkPhysicalDeviceFeatures features = {};    
    vkGetPhysicalDeviceFeatures(curr_device->handle, &features);

    VkPhysicalDeviceMemoryProperties memory_properties = {};
    vkGetPhysicalDeviceMemoryProperties(curr_device->handle, &memory_properties);
    
    memcpy(out_capabilities->device_name, properties.deviceName, sizeof(out_capabilities->device_name));

    switch (properties.deviceType) {
        case VK_PHYSICAL_DEVICE_TYPE_OTHER:
            out_capabilities->device_type = EMBER_DEVICE_TYPE_OTHER;
            break;

        case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
            out_capabilities->device_type = EMBER_DEVICE_TYPE_INTEGRATED_GPU;
            break;

        case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
            out_capabilities->device_type = EMBER_DEVICE_TYPE_DISCRETE_GPU;
            break;

        case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
            out_capabilities->device_type = EMBER_DEVICE_TYPE_VIRTUAL_GPU;
            break;

        case VK_PHYSICAL_DEVICE_TYPE_CPU:
            out_capabilities->device_type = EMBER_DEVICE_TYPE_CPU;
            break;
    }

    out_capabilities->driver_version = EMBER_MAKE_VERSION(
            VK_VERSION_MAJOR(properties.driverVersion), 
            VK_VERSION_MINOR(properties.driverVersion), 
            VK_VERSION_PATCH(properties.driverVersion));

    out_capabilities->max_anisotropy = properties.limits.maxSamplerAnisotropy;
    out_capabilities->vendor_signiture = properties.vendorID;
}

u32 score_phys_device(vulkan_phys_device* device) {
    // TODO
    return 0;
}

f64 score_queue_type(VkQueueFamilyProperties* queue_family, vulkan_queue_family queue_type) {
    f64 queue_types = 0;
    if (queue_family->queueFlags & VK_QUEUE_GRAPHICS_BIT) ++queue_types;    
    if (queue_family->queueFlags & VK_QUEUE_COMPUTE_BIT)  ++queue_types;
    if (queue_family->queueFlags & VK_QUEUE_TRANSFER_BIT) ++queue_types;
    
    f64 queue_priority = 0;
    switch (queue_type) {
        case VULKAN_QUEUE_FAMILY_RASTER:   queue_priority = 8; break;
        case VULKAN_QUEUE_FAMILY_COMPUTE:  queue_priority = 4; break;
        case VULKAN_QUEUE_FAMILY_TRANSFER: queue_priority = 2; break;
    }

    // TODO: Currently not considering queue count, the driver is designed around
    // multiple queues but not multiple queues per type. Properly need input from
    // Ember on whetever multiple queues is a priority and then we'll need to expose
    // hardware queues to Ember and I don't really want to do that.
    return queue_priority / queue_types; //+ log2(queue_family->queueCount);
}

VkAttachmentLoadOp load_op_to_vulkan_type(emgpu_load_op load_op) {
    switch (load_op) {
    case EMBER_LOAD_OP_LOAD: return VK_ATTACHMENT_LOAD_OP_LOAD;
    case EMBER_LOAD_OP_CLEAR: return VK_ATTACHMENT_LOAD_OP_CLEAR;
    case EMBER_LOAD_OP_DONT_CARE: return VK_ATTACHMENT_LOAD_OP_DONT_CARE;

    default:
        EM_ASSERT(EMFALSE && "Unsupported load op!");
        break;
    }

    return 0;
}

VkAttachmentStoreOp store_op_to_vulkan_type(emgpu_store_op store_op) {
    switch (store_op) {
    case EMBER_STORE_OP_STORE: return VK_ATTACHMENT_STORE_OP_STORE;
    case EMBER_STORE_OP_DONT_CARE: return VK_ATTACHMENT_STORE_OP_DONT_CARE;
    
    default:
        EM_ASSERT(EMFALSE && "Unsupported store op!");
        break;
    }

    return 0;
}

VkBlendFactor vulkan_blend_factor_type(emgpu_blend_factor blend_factor) {
    switch (blend_factor) {
        case EMBER_BLEND_FACTOR_ZERO:                         return VK_BLEND_FACTOR_ZERO;
        case EMBER_BLEND_FACTOR_ONE:                          return VK_BLEND_FACTOR_ONE;
        case EMBER_BLEND_FACTOR_SRC_COLOUR:                   return VK_BLEND_FACTOR_SRC_COLOR;
        case EMBER_BLEND_FACTOR_ONE_MINUS_SRC_COLOUR:         return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
        case EMBER_BLEND_FACTOR_DST_COLOUR:                   return VK_BLEND_FACTOR_DST_COLOR;
        case EMBER_BLEND_FACTOR_ONE_MINUS_DST_COLOUR:         return VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
        case EMBER_BLEND_FACTOR_SRC_ALPHA:                    return VK_BLEND_FACTOR_SRC_ALPHA;
        case EMBER_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA:          return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        case EMBER_BLEND_FACTOR_DST_ALPHA:                    return VK_BLEND_FACTOR_DST_ALPHA;
        case EMBER_BLEND_FACTOR_ONE_MINUS_DST_ALPHA:          return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
        case EMBER_BLEND_FACTOR_CONSTANT_COLOUR:              return VK_BLEND_FACTOR_CONSTANT_COLOR;
        case EMBER_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOUR:    return VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR;
        case EMBER_BLEND_FACTOR_CONSTANT_ALPHA:               return VK_BLEND_FACTOR_CONSTANT_ALPHA;
        case EMBER_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA:     return VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA;
        
        default:
            EM_ASSERT(EMFALSE && "Unsupported blend factor!");
            break;
    }

    return 0;
}

VkBlendOp vulkan_blend_op_type(emgpu_blend_op blend_op) {
    switch (blend_op) {
        case EMBER_BLEND_OP_ADD:                return VK_BLEND_OP_ADD;
        case EMBER_BLEND_OP_SUBTRACT:           return VK_BLEND_OP_SUBTRACT;
        case EMBER_BLEND_OP_REVERSE_SUBTRACT:   return VK_BLEND_OP_REVERSE_SUBTRACT;
        case EMBER_BLEND_OP_MIN:                return VK_BLEND_OP_MIN;
        case EMBER_BLEND_OP_MAX:                return VK_BLEND_OP_MAX;

        default:
            EM_ASSERT(EMFALSE && "Unsupported blend op!");
            break;
    }

    return 0;
}
