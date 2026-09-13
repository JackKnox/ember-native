#include "defines.h"
#include "wl_types.h"

#include "window/wsi_vulkan.h"

typedef VkFlags VkWaylandSurfaceCreateFlagsKHR;

typedef struct VkWaylandSurfaceCreateInfoKHR {
    VkStructureType                 sType;
    const void*                     pNext;
    VkWaylandSurfaceCreateFlagsKHR  flags;
    struct wl_display*              display;
    struct wl_surface*              surface;
} VkWaylandSurfaceCreateInfoKHR;

typedef VkResult (*PFN_vkCreateWaylandSurfaceKHR)(VkInstance,const VkWaylandSurfaceCreateInfoKHR*,const VkAllocationCallbacks*,VkSurfaceKHR*);
typedef VkBool32 (*PFN_vkGetPhysicalDeviceWaylandPresentationSupportKHR)(VkPhysicalDevice,uint32_t,struct wl_display*);

em_result emnat_vulkan_create_surface(VkInstance instance, VkAllocationCallbacks* allocator, emwin_window* window, VkSurfaceKHR* out_surface) {
    wayland_window* wl_window = (wayland_window*)window->internal_context;

    wayland_desktop* wl_desktop = (wayland_desktop*)window->desktop->internal_context;
        
    VkWaylandSurfaceCreateInfoKHR create_info = { VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR };
    create_info.display = wl_desktop->display;
    create_info.surface = wl_window->surface;

    PFN_vkCreateWaylandSurfaceKHR vkCreateWaylandSurfaceKHR =
        (PFN_vkCreateWaylandSurfaceKHR)vkGetInstanceProcAddr(instance, "vkCreateWaylandSurfaceKHR");
    CHECK_VKRESULT(
        vkCreateWaylandSurfaceKHR(instance, &create_info, allocator, out_surface), 
        "Failed to create native Vulkan wayland surface");

    return EMBER_RESULT_OK;
}

b8 emnat_vulkan_device_presentation_support(VkInstance instance, VkPhysicalDevice physical_device, u32 queue_family_index, void* user_data) {
    PFN_vkGetPhysicalDeviceWaylandPresentationSupportKHR vkGetPhysicalDeviceWaylandPresentationSupportKHR =
        (PFN_vkGetPhysicalDeviceWaylandPresentationSupportKHR)vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceWaylandPresentationSupportKHR");
    
    wayland_desktop* wl_desktop = (wayland_desktop*)((emwin_desktop*)user_data)->internal_context;
    return vkGetPhysicalDeviceWaylandPresentationSupportKHR(physical_device, queue_family_index, wl_desktop->display);
}

const char* emnat_vulkan_wsi_extensions(emwin_desktop* desktop) {
    return "VK_KHR_wayland_surface";
}
