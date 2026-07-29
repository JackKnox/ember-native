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

VkAttachmentLoadOp vulkan_load_op_type(emgpu_load_op load_op) {
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

VkAttachmentStoreOp vulkan_store_op_type(emgpu_store_op store_op) {
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

VkFormat vulkan_format_type(emgpu_format format) {
    switch (format) {
        // --- 8-bit UINT ---
        case EMGPU_FORMAT_R8_UINT:    return VK_FORMAT_R8_UINT;
        case EMGPU_FORMAT_RG8_UINT:   return VK_FORMAT_R8G8_UINT;
        case EMGPU_FORMAT_RGB8_UINT:  return VK_FORMAT_R8G8B8_UINT;
        case EMGPU_FORMAT_RGBA8_UINT: return VK_FORMAT_R8G8B8A8_UINT;

        // --- 8-bit SINT ---
        case EMGPU_FORMAT_R8_SINT:    return VK_FORMAT_R8_SINT;
        case EMGPU_FORMAT_RG8_SINT:   return VK_FORMAT_R8G8_SINT;
        case EMGPU_FORMAT_RGB8_SINT:  return VK_FORMAT_R8G8B8_SINT;
        case EMGPU_FORMAT_RGBA8_SINT: return VK_FORMAT_R8G8B8A8_SINT;

        // --- 8-bit UNORM ---
        case EMGPU_FORMAT_R8_UNORM:    return VK_FORMAT_R8_UNORM;
        case EMGPU_FORMAT_RG8_UNORM:   return VK_FORMAT_R8G8_UNORM;
        case EMGPU_FORMAT_RGB8_UNORM:  return VK_FORMAT_R8G8B8_UNORM;
        case EMGPU_FORMAT_RGBA8_UNORM: return VK_FORMAT_R8G8B8A8_UNORM;

        // --- 8-bit SNORM ---
        case EMGPU_FORMAT_R8_SNORM:    return VK_FORMAT_R8_SNORM;
        case EMGPU_FORMAT_RG8_SNORM:   return VK_FORMAT_R8G8_SNORM;
        case EMGPU_FORMAT_RGB8_SNORM:  return VK_FORMAT_R8G8B8_SNORM;
        case EMGPU_FORMAT_RGBA8_SNORM: return VK_FORMAT_R8G8B8A8_SNORM;

        // --- 16-bit UINT ---
        case EMGPU_FORMAT_R16_UINT:    return VK_FORMAT_R16_UINT;
        case EMGPU_FORMAT_RG16_UINT:   return VK_FORMAT_R16G16_UINT;
        case EMGPU_FORMAT_RGB16_UINT:  return VK_FORMAT_R16G16B16_UINT;
        case EMGPU_FORMAT_RGBA16_UINT: return VK_FORMAT_R16G16B16A16_UINT;

        // --- 16-bit SINT ---
        case EMGPU_FORMAT_R16_SINT:    return VK_FORMAT_R16_SINT;
        case EMGPU_FORMAT_RG16_SINT:   return VK_FORMAT_R16G16_SINT;
        case EMGPU_FORMAT_RGB16_SINT:  return VK_FORMAT_R16G16B16_SINT;
        case EMGPU_FORMAT_RGBA16_SINT: return VK_FORMAT_R16G16B16A16_SINT;

        // --- 16-bit FLOAT ---
        case EMGPU_FORMAT_R16_FLOAT:    return VK_FORMAT_R16_SFLOAT;
        case EMGPU_FORMAT_RG16_FLOAT:   return VK_FORMAT_R16G16_SFLOAT;
        case EMGPU_FORMAT_RGB16_FLOAT:  return VK_FORMAT_R16G16B16_SFLOAT;
        case EMGPU_FORMAT_RGBA16_FLOAT: return VK_FORMAT_R16G16B16A16_SFLOAT;

        // --- 32-bit UINT ---
        case EMGPU_FORMAT_R32_UINT:    return VK_FORMAT_R32_UINT;
        case EMGPU_FORMAT_RG32_UINT:   return VK_FORMAT_R32G32_UINT;
        case EMGPU_FORMAT_RGB32_UINT:  return VK_FORMAT_R32G32B32_UINT;
        case EMGPU_FORMAT_RGBA32_UINT: return VK_FORMAT_R32G32B32A32_UINT;

        // --- 32-bit SINT ---
        case EMGPU_FORMAT_R32_SINT:    return VK_FORMAT_R32_SINT;
        case EMGPU_FORMAT_RG32_SINT:   return VK_FORMAT_R32G32_SINT;
        case EMGPU_FORMAT_RGB32_SINT:  return VK_FORMAT_R32G32B32_SINT;
        case EMGPU_FORMAT_RGBA32_SINT: return VK_FORMAT_R32G32B32A32_SINT;

        // --- 32-bit FLOAT ---
        case EMGPU_FORMAT_R32_FLOAT:    return VK_FORMAT_R32_SFLOAT;
        case EMGPU_FORMAT_RG32_FLOAT:   return VK_FORMAT_R32G32_SFLOAT;
        case EMGPU_FORMAT_RGB32_FLOAT:  return VK_FORMAT_R32G32B32_SFLOAT;
        case EMGPU_FORMAT_RGBA32_FLOAT: return VK_FORMAT_R32G32B32A32_SFLOAT;

        // --- SRGB ---
        case EMGPU_FORMAT_R8_SRGB:    return VK_FORMAT_R8_SRGB;
        case EMGPU_FORMAT_RG8_SRGB:   return VK_FORMAT_R8G8_SRGB;
        case EMGPU_FORMAT_RGB8_SRGB:  return VK_FORMAT_R8G8B8_SRGB;
        case EMGPU_FORMAT_RGBA8_SRGB: return VK_FORMAT_R8G8B8A8_SRGB;

        // --- BGRA / BGR ---
        case EMGPU_FORMAT_BGR8_UNORM:  return VK_FORMAT_B8G8R8_UNORM;
        case EMGPU_FORMAT_BGR8_SNORM:  return VK_FORMAT_B8G8R8_SNORM;
        case EMGPU_FORMAT_BGR8_UINT:   return VK_FORMAT_B8G8R8_UINT;
        case EMGPU_FORMAT_BGR8_SINT:   return VK_FORMAT_B8G8R8_SINT;
        case EMGPU_FORMAT_BGR8_SRGB:   return VK_FORMAT_B8G8R8_SRGB;

        case EMGPU_FORMAT_BGRA8_UNORM: return VK_FORMAT_B8G8R8A8_UNORM;
        case EMGPU_FORMAT_BGRA8_SNORM: return VK_FORMAT_B8G8R8A8_SNORM;
        case EMGPU_FORMAT_BGRA8_UINT:  return VK_FORMAT_B8G8R8A8_UINT;
        case EMGPU_FORMAT_BGRA8_SINT:  return VK_FORMAT_B8G8R8A8_SINT;
        case EMGPU_FORMAT_BGRA8_SRGB:  return VK_FORMAT_B8G8R8A8_SRGB;

        // --- Depth / Stencil ---
        case EMGPU_FORMAT_D16_UNORM:         return VK_FORMAT_D16_UNORM;
        case EMGPU_FORMAT_D24_UNORM:         return VK_FORMAT_X8_D24_UNORM_PACK32;
        case EMGPU_FORMAT_D32_FLOAT:         return VK_FORMAT_D32_SFLOAT;
        case EMGPU_FORMAT_D24_UNORM_S8_UINT: return VK_FORMAT_D24_UNORM_S8_UINT;
        case EMGPU_FORMAT_D32_FLOAT_S8_UINT: return VK_FORMAT_D32_SFLOAT_S8_UINT;

        default:
            return VK_FORMAT_UNDEFINED;
    }
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

em_result em_result_from_vulkan_result(VkResult result) {
    switch (result) {
    case VK_SUCCESS:
        return EMBER_RESULT_OK;
    case VK_TIMEOUT:
        return EMBER_RESULT_TIMEOUT;
    case VK_INCOMPLETE:
    case VK_NOT_READY:
        return EMBER_RESULT_INVALID_VALUE;
    case VK_ERROR_OUT_OF_HOST_MEMORY:
    case VK_ERROR_MEMORY_MAP_FAILED:
    case VK_ERROR_TOO_MANY_OBJECTS:
    case VK_ERROR_FRAGMENTED_POOL:
    case VK_ERROR_FRAGMENTATION:
        return EMBER_RESULT_OUT_OF_MEMORY_CPU;
    case VK_ERROR_OUT_OF_DEVICE_MEMORY:
    case VK_ERROR_OUT_OF_POOL_MEMORY:
        return EMBER_RESULT_OUT_OF_MEMORY_GPU;
    case VK_ERROR_DEVICE_LOST:
    case VK_ERROR_SURFACE_LOST_KHR:
        return EMBER_RESULT_UNINITIALIZED;
    case VK_ERROR_LAYER_NOT_PRESENT:
    case VK_ERROR_EXTENSION_NOT_PRESENT:
    case VK_ERROR_FEATURE_NOT_PRESENT:
    case VK_ERROR_INCOMPATIBLE_DRIVER:
        return EMBER_RESULT_UNAVAILABLE_API;
    case VK_ERROR_FORMAT_NOT_SUPPORTED:
    case VK_ERROR_INCOMPATIBLE_DISPLAY_KHR:
        return EMBER_RESULT_UNSUPPORTED_FORMAT;
    case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR:
    case VK_ERROR_OUT_OF_DATE_KHR:
        return EMBER_RESULT_IN_USE;

    default:
    case VK_ERROR_UNKNOWN:
        return EMBER_RESULT_UNKNOWN;
    }
}

const char* vulkan_result_string(VkResult result, b8 get_extended) {
    // From: https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/VkResult.html
    // Success Codes
    switch (result) {
    default:
    case VK_SUCCESS:
        return !get_extended ? "VK_SUCCESS" : "VK_SUCCESS Command successfully completed";
    case VK_NOT_READY:
        return !get_extended ? "VK_NOT_READY" : "VK_NOT_READY A fence or query has not yet completed";
    case VK_TIMEOUT:
        return !get_extended ? "VK_TIMEOUT" : "VK_TIMEOUT A wait operation has not completed in the specified time";
    case VK_EVENT_SET:
        return !get_extended ? "VK_EVENT_SET" : "VK_EVENT_SET An event is signaled";
    case VK_EVENT_RESET:
        return !get_extended ? "VK_EVENT_RESET" : "VK_EVENT_RESET An event is unsignaled";
    case VK_INCOMPLETE:
        return !get_extended ? "VK_INCOMPLETE" : "VK_INCOMPLETE A return array was too small for the result";
    case VK_SUBOPTIMAL_KHR:
        return !get_extended ? "VK_SUBOPTIMAL_KHR" : "VK_SUBOPTIMAL_KHR A swapchain no longer matches the surface properties exactly, but can still be used to present to the surface successfully.";
    case VK_THREAD_IDLE_KHR:
        return !get_extended ? "VK_THREAD_IDLE_KHR" : "VK_THREAD_IDLE_KHR A deferred operation is not complete but there is currently no work for this thread to do at the time of this call.";
    case VK_THREAD_DONE_KHR:
        return !get_extended ? "VK_THREAD_DONE_KHR" : "VK_THREAD_DONE_KHR A deferred operation is not complete but there is no work remaining to assign to additional threads.";
    case VK_OPERATION_DEFERRED_KHR:
        return !get_extended ? "VK_OPERATION_DEFERRED_KHR" : "VK_OPERATION_DEFERRED_KHR A deferred operation was requested and at least some of the work was deferred.";
    case VK_OPERATION_NOT_DEFERRED_KHR:
        return !get_extended ? "VK_OPERATION_NOT_DEFERRED_KHR" : "VK_OPERATION_NOT_DEFERRED_KHR A deferred operation was requested and no operations were deferred.";
    case VK_PIPELINE_COMPILE_REQUIRED_EXT:
        return !get_extended ? "VK_PIPELINE_COMPILE_REQUIRED_EXT" : "VK_PIPELINE_COMPILE_REQUIRED_EXT A requested pipeline creation would have required compilation, but the application requested compilation to not be performed.";

        // Error codes
    case VK_ERROR_OUT_OF_HOST_MEMORY:
        return !get_extended ? "VK_ERROR_OUT_OF_HOST_MEMORY" : "VK_ERROR_OUT_OF_HOST_MEMORY A host memory allocation has failed.";
    case VK_ERROR_OUT_OF_DEVICE_MEMORY:
        return !get_extended ? "VK_ERROR_OUT_OF_DEVICE_MEMORY" : "VK_ERROR_OUT_OF_DEVICE_MEMORY A device memory allocation has failed.";
    case VK_ERROR_INITIALIZATION_FAILED:
        return !get_extended ? "VK_ERROR_INITIALIZATION_FAILED" : "VK_ERROR_INITIALIZATION_FAILED Initialization of an object could not be completed for implementation-specific reasons.";
    case VK_ERROR_DEVICE_LOST:
        return !get_extended ? "VK_ERROR_DEVICE_LOST" : "VK_ERROR_DEVICE_LOST The logical or physical device has been lost. See Lost Device";
    case VK_ERROR_MEMORY_MAP_FAILED:
        return !get_extended ? "VK_ERROR_MEMORY_MAP_FAILED" : "VK_ERROR_MEMORY_MAP_FAILED Mapping of a memory object has failed.";
    case VK_ERROR_LAYER_NOT_PRESENT:
        return !get_extended ? "VK_ERROR_LAYER_NOT_PRESENT" : "VK_ERROR_LAYER_NOT_PRESENT A requested layer is not present or could not be loaded.";
    case VK_ERROR_EXTENSION_NOT_PRESENT:
        return !get_extended ? "VK_ERROR_EXTENSION_NOT_PRESENT" : "VK_ERROR_EXTENSION_NOT_PRESENT A requested extension is not supported.";
    case VK_ERROR_FEATURE_NOT_PRESENT:
        return !get_extended ? "VK_ERROR_FEATURE_NOT_PRESENT" : "VK_ERROR_FEATURE_NOT_PRESENT A requested feature is not supported.";
    case VK_ERROR_INCOMPATIBLE_DRIVER:
        return !get_extended ? "VK_ERROR_INCOMPATIBLE_DRIVER" : "VK_ERROR_INCOMPATIBLE_DRIVER The requested version of Vulkan is not supported by the driver or is otherwise incompatible for implementation-specific reasons.";
    case VK_ERROR_TOO_MANY_OBJECTS:
        return !get_extended ? "VK_ERROR_TOO_MANY_OBJECTS" : "VK_ERROR_TOO_MANY_OBJECTS Too many objects of the type have already been created.";
    case VK_ERROR_FORMAT_NOT_SUPPORTED:
        return !get_extended ? "VK_ERROR_FORMAT_NOT_SUPPORTED" : "VK_ERROR_FORMAT_NOT_SUPPORTED A requested format is not supported on this device.";
    case VK_ERROR_FRAGMENTED_POOL:
        return !get_extended ? "VK_ERROR_FRAGMENTED_POOL" : "VK_ERROR_FRAGMENTED_POOL A pool allocation has failed due to fragmentation of the pool's memory. This must only be returned if no attempt to allocate host or device memory was made to accommodate the new allocation. This should be returned in preference to VK_ERROR_OUT_OF_POOL_MEMORY, but only if the implementation is certain that the pool allocation failure was due to fragmentation.";
    case VK_ERROR_SURFACE_LOST_KHR:
        return !get_extended ? "VK_ERROR_SURFACE_LOST_KHR" : "VK_ERROR_SURFACE_LOST_KHR A surface is no longer available.";
    case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR:
        return !get_extended ? "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR" : "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR The requested window is already in use by Vulkan or another API in a manner which prevents it from being used again.";
    case VK_ERROR_OUT_OF_DATE_KHR:
        return !get_extended ? "VK_ERROR_OUT_OF_DATE_KHR" : "VK_ERROR_OUT_OF_DATE_KHR A surface has changed in such a way that it is no longer compatible with the swapchain, and further presentation requests using the swapchain will fail. Applications must query the new surface properties and recreate their swapchain if they wish to continue presenting to the surface.";
    case VK_ERROR_INCOMPATIBLE_DISPLAY_KHR:
        return !get_extended ? "VK_ERROR_INCOMPATIBLE_DISPLAY_KHR" : "VK_ERROR_INCOMPATIBLE_DISPLAY_KHR The display used by a swapchain does not use the same presentable image layout, or is incompatible in a way that prevents sharing an image.";
    case VK_ERROR_INVALID_SHADER_NV:
        return !get_extended ? "VK_ERROR_INVALID_SHADER_NV" : "VK_ERROR_INVALID_SHADER_NV One or more shaders failed to compile or link. More details are reported back to the application via VK_EXT_debug_report if enabled.";
    case VK_ERROR_OUT_OF_POOL_MEMORY:
        return !get_extended ? "VK_ERROR_OUT_OF_POOL_MEMORY" : "VK_ERROR_OUT_OF_POOL_MEMORY A pool memory allocation has failed. This must only be returned if no attempt to allocate host or device memory was made to accommodate the new allocation. If the failure was definitely due to fragmentation of the pool, VK_ERROR_FRAGMENTED_POOL should be returned instead.";
    case VK_ERROR_INVALID_EXTERNAL_HANDLE:
        return !get_extended ? "VK_ERROR_INVALID_EXTERNAL_HANDLE" : "VK_ERROR_INVALID_EXTERNAL_HANDLE An external handle is not a valid handle of the specified type.";
    case VK_ERROR_FRAGMENTATION:
        return !get_extended ? "VK_ERROR_FRAGMENTATION" : "VK_ERROR_FRAGMENTATION A descriptor pool creation has failed due to fragmentation.";
    case VK_ERROR_INVALID_DEVICE_ADDRESS_EXT:
        return !get_extended ? "VK_ERROR_INVALID_DEVICE_ADDRESS_EXT" : "VK_ERROR_INVALID_DEVICE_ADDRESS_EXT A buffer creation failed because the requested address is not available.";
        // * NOTE: Same as above
        //case VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS:
        //    return !get_extended ? "VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS" :"VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS A buffer creation or memory allocation failed because the requested address is not available. A shader group handle assignment failed because the requested shader group handle information is no longer valid.";
    case VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT:
        return !get_extended ? "VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT" : "VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT An operation on a swapchain created with VK_FULL_SCREEN_EXCLUSIVE_APPLICATION_CONTROLLED_EXT failed as it did not have exlusive full-screen access. This may occur due to implementation-dependent reasons, outside of the application's control.";
    case VK_ERROR_UNKNOWN:
        return !get_extended ? "VK_ERROR_UNKNOWN" : "VK_ERROR_UNKNOWN An unknown error has occurred; either the application has provided invalid input, or an implementation failure has occurred.";
    }
}

b8 vulkan_result_is_success(VkResult result) {
    // From: https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/VkResult.html
    switch (result) {
        // Success Codes
    default:
    case VK_SUCCESS:
    case VK_NOT_READY:
    case VK_TIMEOUT:
    case VK_EVENT_SET:
    case VK_EVENT_RESET:
    case VK_INCOMPLETE:
    case VK_SUBOPTIMAL_KHR:
    case VK_THREAD_IDLE_KHR:
    case VK_THREAD_DONE_KHR:
    case VK_OPERATION_DEFERRED_KHR:
    case VK_OPERATION_NOT_DEFERRED_KHR:
    case VK_PIPELINE_COMPILE_REQUIRED_EXT:
        return EMTRUE;

        // Error codes
    case VK_ERROR_OUT_OF_HOST_MEMORY:
    case VK_ERROR_OUT_OF_DEVICE_MEMORY:
    case VK_ERROR_INITIALIZATION_FAILED:
    case VK_ERROR_DEVICE_LOST:
    case VK_ERROR_MEMORY_MAP_FAILED:
    case VK_ERROR_LAYER_NOT_PRESENT:
    case VK_ERROR_EXTENSION_NOT_PRESENT:
    case VK_ERROR_FEATURE_NOT_PRESENT:
    case VK_ERROR_INCOMPATIBLE_DRIVER:
    case VK_ERROR_TOO_MANY_OBJECTS:
    case VK_ERROR_FORMAT_NOT_SUPPORTED:
    case VK_ERROR_FRAGMENTED_POOL:
    case VK_ERROR_SURFACE_LOST_KHR:
    case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR:
    case VK_ERROR_OUT_OF_DATE_KHR:
    case VK_ERROR_INCOMPATIBLE_DISPLAY_KHR:
    case VK_ERROR_INVALID_SHADER_NV:
    case VK_ERROR_OUT_OF_POOL_MEMORY:
    case VK_ERROR_INVALID_EXTERNAL_HANDLE:
    case VK_ERROR_FRAGMENTATION:
    case VK_ERROR_INVALID_DEVICE_ADDRESS_EXT:
        // * NOTE: Same as above
        //case VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS:
    case VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT:
    case VK_ERROR_UNKNOWN:
        return EMFALSE;
    }
}
