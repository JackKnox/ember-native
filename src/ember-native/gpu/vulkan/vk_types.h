#pragma once

#include "defines.h"

#include <ember/gpu/device.h>
#include <ember/gpu/resources.h>

#include <ember/gpu/raster.h>

#include <vulkan/vulkan.h>

// Checks a Vulkan call and logs an error on failure.
// Intended for recoverable errors during initialization or runtime.
#define CHECK_VKRESULT(func, message)                       \
    {                                                       \
        VkResult result = func;                             \
        if (!vulkan_result_is_success(result)) {            \
            EM_ERROR("Vulkan", message ": %s",              \
                     vulkan_result_string(result, EMTRUE)); \
            return em_result_from_vulkan_result(result);    \
        }                                                   \
    }

typedef VkSemaphore VkTimelineSemaphore;

typedef enum vulkan_queue_family {
    VULKAN_QUEUE_FAMILY_RASTER,
    VULKAN_QUEUE_FAMILY_COMPUTE,
    VULKAN_QUEUE_FAMILY_TRANSFER,
    __VULKAN_QUEUE_FAMILY_COUNT,
} vulkan_queue_family;

typedef struct vulkan_phys_queue {
    f64 score;
    u32 family_index;
    b8 enabled;
} vulkan_phys_queue; 

typedef struct vulkan_phys_device {
    VkPhysicalDevice handle;
    
    emgpu_device_capabilities capabilities;
    vulkan_phys_queue queue_families[__VULKAN_QUEUE_FAMILY_COUNT];
    
    i32 heuristic;
} vulkan_phys_device;

typedef struct vulkan_log_queue {
    VkQueue handle;
    u32 family_index;
    b8 enabled;
    VkTimelineSemaphore semaphore;
} vulkan_log_queue;

typedef struct vulkan_log_device {
    VkDevice handle;
    VkPhysicalDevice physical;
} vulkan_log_device;

typedef struct vulkan_context {
    VkInstance instance;
    VkAllocationCallbacks* allocator;
    vulkan_log_device device;
} vulkan_context;

typedef struct vulkan_pipeline {
    VkPipeline handle;
    VkPipelineLayout layout;
    VkDescriptorSetLayout descriptor_layout;
} vulkan_pipeline;

typedef struct vulkan_buffer {
    VkBuffer handle;
    VkDeviceMemory memory;
} vulkan_buffer;

typedef struct vulkan_renderpass {
    VkRenderPass handle;
} vulkan_renderpass;

typedef struct vulkan_surface {
    VkSurfaceKHR surface;
    VkSwapchainKHR swapchain;

    VkSurfaceCapabilitiesKHR capabilities;
    VkColorSpaceKHR colour_space;
    emgpu_texture* frames_in_flight;
    u32 image_index;

    VkSemaphore* image_availables;
    VkSemaphore* render_completes;
} vulkan_surface;

/*
 * Types of resource dependency:
 *     * Timeline break
 *     * Cross-queue pipeline barrier
 *     * Pipeline barrier
 *     * Binary semaphore
 *     * Renderpass / subpass
 */

typedef struct managed_resource {
    void* raw;
} managed_resource;

typedef struct vulkan_command_submission {
    emgpu_ops_type ops_type;
    VkCommandBuffer handle;
} vulkan_command_submission;

typedef struct vulkan_command_context {
    vulkan_command_submission curr_submission;
    const emgpu_pipeline* curr_pipeline;
} vulkan_command_context;

// Converts Vulkan error code to engine result code.
em_result em_result_from_vulkan_result(VkResult result);

// Returns a human-readable string for a Vulkan result code.
const char* vulkan_result_string(VkResult result, b8 get_extended);

// Determines whether a Vulkan result represents a success code.
b8 vulkan_result_is_success(VkResult result);

// Converts format type to a Vulkan format.
VkFormat vulkan_format_type(emgpu_format format);

// Finds a suitable memory index based on memory requirements, -1 means one could not be found.
i32 vulkan_memory_index(vulkan_context* context, VkMemoryRequirements* requirements, VkMemoryPropertyFlags flags);

// Creates the pipeline and descriptor layouts on top of the pipeline.
em_result vulkan_create_pipeline_layout(emgpu_device* device, em_allocator* allocator, const emgpu_descriptor_desc* descriptors, u32 descriptor_count, emgpu_pipeline* out_pipeline);

// Create a shader stage based on a type and source.
em_result vulkan_create_shader_stage(emgpu_device* device, em_allocator* allocator, const emgpu_shader_src* shader, VkShaderStageFlags shader_type, VkPipelineShaderStageCreateInfo* out_shader_stage);

// Fill capabilities structure from physical device.
// TODO: Maybe get rid of this?
void vulkan_device_from_capabilities(vulkan_phys_device* curr_device, emgpu_device_capabilities* out_capabilities);
// Scores a physical GPU based on its overall usefulness.
u32 score_phys_device(vulkan_phys_device* device);

// Scores a Vulkan queue family for a specific Ember queue purpose.
f64 score_queue_type(VkQueueFamilyProperties* queue_family, vulkan_queue_family queue_type);

// Converts load op format to a Vulkan format.
VkAttachmentLoadOp vulkan_load_op_type(emgpu_load_op load_op);

// Converts store op format to a Vulkan format.
VkAttachmentStoreOp vulkan_store_op_type(emgpu_store_op store_op);

// Converts blend factor to a Vulkan format.
VkBlendFactor vulkan_blend_factor_type(emgpu_blend_factor blend_factor);

// Converts blend op to a Vulkan format.
VkBlendOp vulkan_blend_op_type(emgpu_blend_op blend_op);

void vulkan_colour_attachment_type(const emgpu_colour_attachment* attachment, VkRenderingAttachmentInfo* out_attachment);
