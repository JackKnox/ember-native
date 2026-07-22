#pragma once

#include "defines.h"

#include <ember/gpu/device.h>

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

typedef enum vulkan_queue_family {
    VULKAN_QUEUE_FAMILY_RASTER,
    VULKAN_QUEUE_FAMILY_COMPUTE,
    VULKAN_QUEUE_FAMILY_TRANSFER,
    __VULKAN_QUEUE_FAMILY_COUNT,
} vulkan_queue_family;

typedef struct vulkan_phys_queue {
    u32 family_index;
    b8 enabled;
} vulkan_phys_queue; 

typedef struct vulkan_phys_device {
    VkPhysicalDevice handle;
    VkPhysicalDeviceProperties properties;
    VkPhysicalDeviceFeatures features;
    vulkan_phys_queue queue_families[__VULKAN_QUEUE_FAMILY_COUNT];
    u32 heuristic;
    emgpu_device_mode enabled_modes;
} vulkan_phys_device;

typedef struct vulkan_log_queue {
    VkQueue handle;
    u32 family_index;
    b8 enabled;
} vulkan_log_queue;

typedef struct vulkan_log_device {
    VkDevice handle;
    VkCommandPool command_pool;
    vulkan_log_queue log_queues[__VULKAN_QUEUE_FAMILY_COUNT];
} vulkan_log_device;

typedef struct vulkan_context {
    VkInstance instance;
    VkAllocationCallbacks* allocator;
    vulkan_log_device device;
} vulkan_context;

// Converts Vulkan error code to engine result code.
em_result em_result_from_vulkan_result(VkResult result);

// Returns a human-readable string for a Vulkan result code.
const char* vulkan_result_string(VkResult result, b8 get_extended);

// Determines whether a Vulkan result represents a success code.
b8 vulkan_result_is_success(VkResult result);
