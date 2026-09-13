#include "defines.h"
#include "vk_types.h"

#include <ember/gpu/surface.h>

em_result emgpu_surface_resize(
    emgpu_device* device, 
    emgpu_surface* surface, 
    uvec2 new_size) {
    vulkan_device* vk_device = (vulkan_device*)device->internal_context;

    vulkan_surface* vk_surface = (vulkan_surface*)surface->internal_data;

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vk_device->physical, vk_surface->surface, &vk_surface->capabilities);

    u32 queue_family_indices[] = {
        vk_device->modes[VULKAN_QUEUE_FAMILY_RASTER].family_index,
        vk_device->wsi.family_index };

    // Swapchain create info 
    VkSwapchainCreateInfoKHR swapchain_create_info = { VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
    swapchain_create_info.surface = vk_surface->surface;
    swapchain_create_info.minImageCount = vk_surface->min_image_count;
    swapchain_create_info.imageFormat = vulkan_format_type(surface->pixel_format);
    swapchain_create_info.imageColorSpace = vulkan_format_colour_space(surface->pixel_format);
    swapchain_create_info.imageExtent = (VkExtent2D) { new_size.x, new_size.y };
    swapchain_create_info.imageArrayLayers = 1;
    swapchain_create_info.imageUsage = vulkan_texture_usage(vk_surface->usage);
    swapchain_create_info.preTransform = vk_surface->capabilities.currentTransform;
    swapchain_create_info.compositeAlpha = VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;
    swapchain_create_info.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    swapchain_create_info.clipped = EMTRUE;
    swapchain_create_info.oldSwapchain = vk_surface->swapchain;
    
    swapchain_create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vk_device->modes[VULKAN_QUEUE_FAMILY_RASTER].family_index != 
        vk_device->wsi.family_index) {
        swapchain_create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        swapchain_create_info.pQueueFamilyIndices = queue_family_indices;
        swapchain_create_info.queueFamilyIndexCount = 2;
    }
}

void emgpu_surface_destroy(
    emgpu_device* device, 
    em_allocator* allocator, 
    emgpu_surface* surface) {

}
