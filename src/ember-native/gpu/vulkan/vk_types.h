#pragma once

#include "defines.h"

#include "ember/gpu/command_buffer.h"
#include "ember/gpu/surface.h"
#include "ember/gpu/types.h"
#include "gpu/command_decoder.h"

#include <ember/gpu/device.h>
#include <ember/gpu/resources.h>
#include <ember/gpu/raster.h>

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

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
    VULKAN_QUEUE_FAMILY_UNIVERSAL,
} vulkan_queue_family;

// Universal type doubles as the count so the universal
// type is omitted when creating arrays.
#define VULKAN_QUEUE_FAMILY_COUNT VULKAN_QUEUE_FAMILY_UNIVERSAL

// Physical device.
// --------------------------------------------------------

/*

#define DATA_ELEMENTS \
    ELEMENT(f64, score, scores) \
    ELEMENT(u32, family_index, family_indexes) \
    ELEMENT(const char*, extensions, extensions) \
    ELEMENT(b8, enabled, enables)
#include "sossoa.h"

DATA_ELEMENT_CARRAY(VULKAN_QUEUE_FAMILY_COUNT)
*/

typedef struct vulkan_sys_info {
    f64 score;
    u32 family_index;
    const char* extensions;
    b8 enabled;
} vulkan_sys_info;

typedef struct vulkan_phys_device {
    VkPhysicalDevice handle;
    emgpu_device_capabilities capabilities;
    vulkan_sys_info phys_modes[VULKAN_QUEUE_FAMILY_COUNT];
    i32 heuristic;
} vulkan_phys_device;


// Resources.
// --------------------------------------------------------

typedef struct vulkan_buffer {
    VkBuffer handle;
    VkDeviceMemory memory;
} vulkan_buffer;

typedef struct vulkan_texture {
    VkImage image;
    VkImageView view;
    VkImageLayout layout;
} vulkan_texture;

typedef struct vulkan_pipeline {
    VkPipeline handle;
    VkPipelineLayout layout;
    VkDescriptorSetLayout descriptor_layout;
} vulkan_pipeline;

typedef struct vulkan_renderpass {
    VkRenderPass handle;
} vulkan_renderpass;

// WSI - Window system interface.
// --------------------------------------------------------

typedef b8 (*vulkan_presentation_support)(
        VkInstance instance, 
        VkPhysicalDevice physical_device, 
        u32 queue_family_index, 
        void* user_data);

typedef struct vulkan_wsi {
    b8 requested;

    vulkan_presentation_support present_support;
    const char* extensions;
    void* wsi_user_data;
    u32 family_index;
} vulkan_wsi;

typedef struct vulkan_surface {
    VkSurfaceCapabilitiesKHR capabilities;
    VkSurfaceKHR surface;
    VkSwapchainKHR swapchain;

    emgpu_texture* frames_in_flight;
    u32 image_index;

    // Config.
    u32 min_image_count;
    emgpu_texture_usage usage;

    VkSemaphore* image_availables;
    VkSemaphore* render_completes;
} vulkan_surface;

// Decode logic.
// --------------------------------------------------------

typedef enum managed_resc_type {
    MANAGED_RESOURCE_EMPTY,
    MANAGED_RESOURCE_BUFFER,
    MANAGED_RESOURCE_TEXTURE, // Same as a framebuffer in the system.
} managed_resc_type;

typedef enum managed_resc_owner {
    COMMAND_OWNER_PIPELINE,
    COMMAND_OWNER_RENDERPASS,
    COMMAND_OWNER_BINARY,
} managed_resc_owner;

typedef struct resource_state {
    emgpu_access_flags access;
    i32 submission_index; // -1 means it's irrevent.
    VkImageLayout texture_layout; // Valid only when resource is a texture
    VkSemaphore binary_owner;
} resource_state;

typedef struct managed_resource {
    managed_resc_type type;
    resource_state state;
    union {
        emgpu_buffer* buffer;
        emgpu_texture* texture;
    } data;
} managed_resource;

typedef struct resource_use {
    emgpu_local_resource resource;
    resource_state state;
} resource_use;

typedef struct owner_desc {
    managed_resc_owner type;

    union {
        const emgpu_pipeline* pipeline;
        VkSemaphore binary;
    };
} owner_desc;

typedef struct owner_frame {
    owner_desc desc;
    resource_use* uses;
} owner_frame;

// Submission logic.
// --------------------------------------------------------

typedef struct vulkan_command_submission {
    vulkan_queue_family queue;
    u32 edge_count;
    VkCommandBuffer handle;
    VkSemaphoreSubmitInfo* waits;
    VkSemaphoreSubmitInfo* signals;
} vulkan_command_submission;

typedef struct vulkan_command_context {
    em_allocator* allocator;
    const emgpu_command_buffer* command_buf;

    owner_frame* stack;

    managed_resource* resource_table;

    emgpu_surface** surfaces;

    vulkan_command_submission* curr_submission;
    vulkan_command_submission* submissions;

    b8 bound_pipeline;
} vulkan_command_context;

// Logical device.
// --------------------------------------------------------

typedef struct vulkan_sys_state {
    VkQueue queue;
    u32 family_index;
    VkTimelineSemaphore semaphore;
    VkCommandPool pool;
    VkCommandBuffer* commandbufs;
} vulkan_sys_state;

typedef struct vulkan_device {
    VkInstance instance;
    VkDebugUtilsMessengerEXT debug_messanger;
    VkAllocationCallbacks* allocator;

    VkDevice handle;
    VkPhysicalDevice physical;
    vulkan_sys_state modes[VULKAN_QUEUE_FAMILY_COUNT];
    vulkan_wsi wsi;
} vulkan_device;

// Main entry points for device modes.
// --------------------------------------------------------

vulkan_sys_info vulkan_raster_setup(vulkan_phys_device* device);

vulkan_sys_info vulkan_compute_setup(vulkan_phys_device* device);

vulkan_sys_info vulkan_transfer_setup(vulkan_phys_device* device);

em_result vulkan_extensions_setup(emgpu_device* device, em_allocator* allocator, const emgpu_device_config* config);

// Utilites.
// --------------------------------------------------------

em_result vulkan_decode_command_buffer(emgpu_device* device, vulkan_command_context* ctx);

// Finds a suitable memory index based on memory requirements, -1 means one could not be found.
i32 vulkan_memory_index(vulkan_device* vk_device, VkMemoryRequirements* requirements, VkMemoryPropertyFlags flags);

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

// Converts Ember descriptor type to Vulkan type.
VkDescriptorType vulkan_descriptor_type(emgpu_descriptor_type type);

// Converts Ember shader stage type to Vulkan type.
VkShaderStageFlags vulkan_shader_stage_type(emgpu_shader_stage_type stage_type);

// Finds the needed Vulkan queue family for a commmand.
vulkan_queue_family command_queue_family(cmd_payload_type type); 

// Converts load op format to a Vulkan format.
VkAttachmentLoadOp vulkan_load_op_type(emgpu_load_op load_op);

// Converts store op format to a Vulkan format.
VkAttachmentStoreOp vulkan_store_op_type(emgpu_store_op store_op);

// Converts the ops type assaigned to a pipeline into a needed bind point.
VkPipelineBindPoint vulkan_bind_point(emgpu_ops_type type);

// Converts blend factor to a Vulkan format.
VkBlendFactor vulkan_blend_factor_type(emgpu_blend_factor blend_factor);

// Converts blend op to a Vulkan format.
VkBlendOp vulkan_blend_op_type(emgpu_blend_op blend_op);

VkBufferUsageFlags vulkan_buffer_usage(emgpu_buffer_usage usage);

// Converts Ember texture usage to Vulkan texture usage.
VkImageUsageFlags vulkan_texture_usage(emgpu_texture_usage usage);

// Gives surface colour space for a given Ember format.
VkColorSpaceKHR vulkan_format_colour_space(emgpu_format format);

// Finds ths stage wait flags for the needed access to resource.
VkPipelineStageFlags2 vulkan_wait_stage(emgpu_access_flags access);

// Finds the nessacery image layout for a access flag, mearly a suggestion
// if the texture already has a supported image layout it will fall through.
VkImageLayout vulkan_image_layout(emgpu_access_flags access);

// Converts format type to a Vulkan format.
VkFormat vulkan_format_type(emgpu_format format);

// TODO: THIS FUNCTION ANNOYES ME SO MUCH.
VkFormat ember_vk_format_type(emgpu_format format);

// Converts Vulkan error code to engine result code.
em_result em_result_from_vulkan_result(VkResult result);

// Returns a human-readable string for a Vulkan result code.
const char* vulkan_result_string(VkResult result, b8 get_extended);

// Determines whether a Vulkan result represents a success code.
b8 vulkan_result_is_success(VkResult result);
