#include "defines.h"
#include "vk_types.h"

#include <ember/gpu/surface.h>

em_result emgpu_surface_resize(
    emgpu_device* device, 
    em_allocator* allocator,
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
    swapchain_create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
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
    
    CHECK_VKRESULT(
        vkCreateSwapchainKHR(
            vk_device->handle, 
            &swapchain_create_info, 
            vk_device->allocator,
            &vk_surface->swapchain), 
        "Failed to create internal Vulkan swapchain");

    CHECK_VKRESULT(
        vkGetSwapchainImagesKHR(
            vk_device->handle, 
            vk_surface->swapchain,
            &surface->image_count,
            NULL), 
        "Failed to retrieve Vulkan swapchain images");

    VkImage* images = (VkImage*)mem_allocate(allocator, sizeof(VkImage) * surface->image_count);
    vkGetSwapchainImagesKHR(vk_device->handle, vk_surface->swapchain, &surface->image_count, images);

    if (!vk_surface->frames_in_flight)
        vk_surface->frames_in_flight = (emgpu_texture*)mem_allocate(allocator, sizeof(emgpu_texture) * surface->image_count);
        
    for (u32 i = 0; i < surface->image_count; ++i) {
        if (vk_surface->frames_in_flight[i].internal_data)
            emgpu_texture_destroy(device, allocator, &vk_surface->frames_in_flight[i]);

        vulkan_texture_ext_config ext_config = {};
        ext_config.image_override = images[i];

        emgpu_texture_config swapchain_texture_config = {}; 
        swapchain_texture_config.size = new_size;
        swapchain_texture_config.image_format = surface->pixel_format;
        swapchain_texture_config.usage = EMBER_TEXTURE_USAGE_ATTACHMENT_DST;
        swapchain_texture_config.api_next = &ext_config;

        em_result result = emgpu_texture_create(
            device, allocator, &swapchain_texture_config, &vk_surface->frames_in_flight[i]);
        if (result != EMBER_RESULT_OK) return result;
    }
    
    if (!vk_surface->image_availables) {
        vk_surface->image_availables 
            = mem_allocate(allocator, sizeof(VkSemaphore) * vk_device->frames_in_flight);

        for (u32 i = 0; i < vk_device->frames_in_flight; ++i) {
            VkSemaphoreCreateInfo create_info = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
            
            CHECK_VKRESULT(
                vkCreateSemaphore(
                    vk_device->handle, 
                    &create_info, 
                    vk_device->allocator,
                    &vk_surface->image_availables[i]),
                "Failed to image available semaphore in Vulkan surface");
        }
    }

    if (!vk_surface->render_completes) {
        vk_surface->render_completes 
            = mem_allocate(allocator, sizeof(VkSemaphore) * vk_device->frames_in_flight);
        
        for (u32 i = 0; i < vk_device->frames_in_flight; ++i) {
            VkSemaphoreCreateInfo create_info = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
            
            CHECK_VKRESULT(
                vkCreateSemaphore(
                    vk_device->handle, 
                    &create_info, 
                    vk_device->allocator,
                    &vk_surface->render_completes[i]),
                "Failed to render complete semaphore in Vulkan surface");
        }
    }

    mem_free(allocator, images, sizeof(VkImage) * surface->image_count);
    return EMBER_RESULT_OK;
}

void emgpu_surface_destroy(
    emgpu_device* device, 
    em_allocator* allocator, 
    emgpu_surface* surface) {

}
