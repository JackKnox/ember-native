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

typedef struct vulkan_context {
    VkInstance instance;
    VkAllocationCallbacks* allocator;
} vulkan_context;

// Converts Vulkan error code to engine result code.
em_result em_result_from_vulkan_result(VkResult result);

// Returns a human-readable string for a Vulkan result code.
const char* vulkan_result_string(VkResult result, b8 get_extended);

// Determines whether a Vulkan result represents a success code.
b8 vulkan_result_is_success(VkResult result);