#include "defines.h"
#include "vk_types.h"

#include "window/wsi_vulkan.h"

#include <ember/gpu/ext/emwin_surface.h>

em_result vk_create_emwin_surface(
    emgpu_device* device,
    em_allocator* allocator,
    emgpu_emwin_surface_config* config,
    emgpu_surface* out_surface) {
    vulkan_device* vk_device = (vulkan_device*)device->internal_context;
    
    out_surface->internal_data = mem_allocate(allocator, sizeof(vulkan_surface));
    vulkan_surface* vk_surface = (vulkan_surface*)out_surface->internal_data;

    em_result result = emnat_vulkan_create_surface(vk_device->instance, vk_device->allocator, config->window, &vk_surface->surface);
    if (result != EMBER_RESULT_OK) return result;

    u32 format_count = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(vk_device->physical, vk_surface->surface, &format_count, NULL);

    VkSurfaceFormatKHR* formats = mem_allocate(allocator, sizeof(VkSurfaceFormatKHR) * format_count);
    vkGetPhysicalDeviceSurfaceFormatsKHR(vk_device->physical, vk_surface->surface, &format_count, formats);

    VkFormat chosen_format = VK_FORMAT_UNDEFINED;

    for (u32 i = 0; i < format_count; ++i) {
        VkSurfaceFormatKHR format = formats[i];

        if (format.format == vulkan_format_type(config->preferred_format)) {
            chosen_format = format.format;
            break;
        }
        
        // Ensures the same colour space.
        if (!config->force_format &&
            EMBER_FORMAT_FLAGS(ember_vk_format_type(format.format)) ==
            EMBER_FORMAT_FLAGS(config->preferred_format)) {
            chosen_format = format.format;
        }
    }

    // Special Vulkan edge case, means no limits on swapchain format.
    if (format_count == 1 && formats[0].format == VK_FORMAT_UNDEFINED)
        chosen_format = vulkan_format_type(config->preferred_format);

    out_surface->pixel_format = ember_vk_format_type(chosen_format);

    mem_free(allocator, formats, sizeof(VkSurfaceFormatKHR) * format_count);

    vk_surface->min_image_count = config->min_texture_count;
    vk_surface->usage = config->usage;
    return emgpu_surface_resize(device, allocator, out_surface, config->window->size);
}

em_result vulkan_extensions_setup(emgpu_device* device, em_allocator* allocator, const emgpu_device_config* config) {
    vulkan_device* vk_device = (vulkan_device*)device->internal_context;

    for (u32 i = 0; i < config->extension_count; ++i) {
        emgpu_extension_desc* desc = &config->extensions[i];

        if (strcmp(desc->name, "EMGPU_EXT_emwin_surface") == 0 && 
                EMBER_VERSIONS_COMPLIANT(desc->version, EMBER_VERSION)) {
            // emwin_surface extension.
            emgpu_emwin_surface_params* params = (emgpu_emwin_surface_params*)desc->user_data.bytes;
            vk_device->wsi.requested       = EMTRUE;
            vk_device->wsi.extensions      = emnat_vulkan_wsi_extensions(params->desktop);
            vk_device->wsi.present_support = emnat_vulkan_device_presentation_support;
            vk_device->wsi.wsi_user_data   = (void*)params->desktop;

            params->out_extension->create_surface = vk_create_emwin_surface;
            continue;
        }

        // Everthing thats fell through
        if (!desc->optional) {
            EM_ERROR("Vulkan", "Drvier doesn't support requested extension: '%s'", desc->name); 
            return EMBER_RESULT_UNAVAILABLE_API;
        }
    }

    return EMBER_RESULT_OK;
}
