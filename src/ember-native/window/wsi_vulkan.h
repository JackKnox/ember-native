#pragma once

#include "defines.h"

#include "gpu/vulkan/vk_types.h"

#include <ember/window/window.h>

em_result emnat_vulkan_create_surface(VkInstance instance, VkAllocationCallbacks* allocator, emwin_window* window, VkSurfaceKHR* out_surface);

b8 emnat_vulkan_device_presentation_support(VkInstance instance, VkPhysicalDevice physical_device, u32 queue_family_index, void* user_data);

const char* emnat_vulkan_wsi_extensions(emwin_desktop* desktop);
