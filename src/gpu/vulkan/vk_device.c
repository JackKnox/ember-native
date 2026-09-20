#include "defines.h"
#include "vk_types.h"

#include "utils/darray.h"

VKAPI_ATTR VkBool32 VKAPI_CALL vk_debug_callback(
	VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
	VkDebugUtilsMessageTypeFlagsEXT message_types,
	const VkDebugUtilsMessengerCallbackDataEXT* callback_data,
	void* user_data) {
	switch (message_severity) {
	case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
		EM_ERROR("Vulkan", callback_data->pMessage);
		break;
	case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
		EM_WARN("Vulkan", callback_data->pMessage);
		break;

	default:
	case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
		EM_TRACE("Vulkan", callback_data->pMessage);
		break;
	}

	return VK_FALSE;
}

em_result emgpu_device_init(em_allocator* allocator, const emgpu_device_config* config, emgpu_device* out_device) {
    // Allocate massive internal context.
    out_device->internal_context  = mem_allocate(allocator, sizeof(vulkan_device));
    vulkan_device* vk_device = (vulkan_device*)out_device->internal_context;

    out_device->frame_allocator = config->frame_allocator;
    vk_device->frames_in_flight = config->frames_in_flight;

    EM_INFO("GPU", "Initialising GPU device with name: %s", config->debug_name);

    // Gather creation info.
	const char** required_extensions = darray_create(const char*, allocator);
	const char** required_validation_layers = darray_create(const char*, allocator);

    em_result result = vulkan_extensions_setup(out_device, allocator, config);
    if (result != EMBER_RESULT_OK) return result;

    if (vk_device->wsi.requested) {
        darray_push(required_extensions, vk_device->wsi.extensions);
        darray_push(required_extensions, "VK_KHR_surface");
    }
    
    darray_push(required_extensions, VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    darray_push(required_validation_layers, "VK_LAYER_KHRONOS_validation");
    
    // ----- Vulkan instance ---------------------------------
    // Verify exsistence of extensions
    u32 supported_extension_count = 0;
    vkEnumerateInstanceExtensionProperties(NULL, &supported_extension_count, NULL);

    VkExtensionProperties* supported_extensions = darray_from_data(VkExtensionProperties, supported_extension_count, NULL, allocator);
    vkEnumerateInstanceExtensionProperties(NULL, &supported_extension_count, supported_extensions);

    for (u32 i = 0; i < darray_length(required_extensions); ++i) {
		b8 found = EMFALSE;
		for (u32 j = 0; j < darray_length(supported_extensions); ++j) {
			if (strcmp(required_extensions[i], supported_extensions[j].extensionName) == 0) {
				found = EMTRUE;
				break;
			}
		}

		if (!found) {
			EM_ERROR("Vulkan", "Required Vulkan extension is missing: %s.", required_extensions[i]);
			return EMBER_RESULT_UNAVAILABLE_API;
		}
	}
    
    // Verify exsistence of validation layers
	u32 supported_layer_count = 0;
	vkEnumerateInstanceLayerProperties(&supported_layer_count, NULL);

	VkLayerProperties* supported_layers = darray_from_data(VkLayerProperties, supported_layer_count, NULL, allocator);
	vkEnumerateInstanceLayerProperties(&supported_layer_count, supported_layers);

	for (u32 i = 0; i < darray_length(required_validation_layers); ++i) {
		b8 found = EMFALSE;
		for (u32 j = 0; j < darray_length(supported_layers); ++j) {
			if (strcmp(required_validation_layers[i], supported_layers[j].layerName) == 0) {
				found = EMTRUE;
				break;
			}
		}

		if (!found) {
			EM_ERROR("Vulkan", "Required Vulkan validation layer is missing: %s.", required_validation_layers[i]);
			return EMBER_RESULT_UNAVAILABLE_API;
		}
	}

	// Fill create info
	VkApplicationInfo app_info  = { VK_STRUCTURE_TYPE_APPLICATION_INFO };
    app_info.pApplicationName   = config->debug_name;
    app_info.applicationVersion = VK_MAKE_API_VERSION(0, 
        EMBER_VERSION_MAJOR(config->app_version), 
        EMBER_VERSION_MINOR(config->app_version), 
        EMBER_VERSION_PATCH(config->app_version));
    app_info.pEngineName        = "ember_gpu";
    app_info.engineVersion      = VK_MAKE_API_VERSION(0, 
        EMBER_VERSION_MAJOR(EMBER_VERSION), 
        EMBER_VERSION_MINOR(EMBER_VERSION), 
        EMBER_VERSION_PATCH(EMBER_VERSION));
    app_info.apiVersion         = VK_API_VERSION_1_3;
    
    VkInstanceCreateInfo create_info = { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
	create_info.pApplicationInfo        = &app_info;
	create_info.enabledLayerCount       = darray_length(required_validation_layers);
	create_info.ppEnabledLayerNames     = required_validation_layers;
	create_info.enabledExtensionCount   = darray_length(required_extensions);
	create_info.ppEnabledExtensionNames = required_extensions;
    
    EM_TRACE("Vulkan", "Using internal GPU library: Vulkan %i.%i.%i", 
            VK_VERSION_MAJOR(VK_HEADER_VERSION_COMPLETE), 
            VK_VERSION_MINOR(VK_HEADER_VERSION_COMPLETE), 
            VK_VERSION_PATCH(VK_HEADER_VERSION_COMPLETE));

	CHECK_VKRESULT(
		vkCreateInstance(&create_info, vk_device->allocator, &vk_device->instance),
		"Failed to create Vulkan instance");
    
    EM_TRACE("Vulkan", "Created Vulkan instance with %i extension(s)", darray_length(required_extensions));

    // TODO: Setup validation layers, Ember is supposed to be the ultimate solution and not another common API lib
    //       so this isn't very important except for development.
    VkDebugUtilsMessengerCreateInfoEXT debug_create_info = { VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT };
    debug_create_info.messageSeverity = 
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT    |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    debug_create_info.messageType =
        VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT     |
        VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
    debug_create_info.pfnUserCallback = vk_debug_callback;

    PFN_vkCreateDebugUtilsMessengerEXT create_debug_messenger = 
        (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(vk_device->instance, "vkCreateDebugUtilsMessengerEXT");

    EM_INFO("Vulkan", "Creating Vulkan validation layers.");

    CHECK_VKRESULT(
        create_debug_messenger(
            vk_device->instance, 
            &debug_create_info, 
            vk_device->allocator, 
            &vk_device->debug_messanger),
        "Failed to create internal Vulkan debug messenger");

	// Clean up temp arrays
    darray_destroy(supported_extensions);
	darray_destroy(supported_layers);
	darray_destroy(required_extensions);
	darray_destroy(required_validation_layers);

    // ----- Physical device iteration -----------------------
    // Build a list of availible Vulkan devices, physical ones! 
    u32 physical_device_count = 0;

    CHECK_VKRESULT(
        vkEnumeratePhysicalDevices(vk_device->instance, &physical_device_count, NULL),    
        "Failed to enumerate physical devices");

    if (!physical_device_count) {
        EM_ERROR("Vulkan", "No devices which support Vulkan were found.");
        return EMBER_RESULT_UNAVAILABLE_API;
    }

    EM_TRACE("Vulkan", "Enumerated %i physical device(s)", physical_device_count);
    
    VkPhysicalDevice* physical_devices = darray_from_data(VkPhysicalDevice, physical_device_count, NULL, allocator);
    vkEnumeratePhysicalDevices(vk_device->instance, &physical_device_count, physical_devices);
    
    // We will populate this below, submit to change as we iterate all the devices.
    vulkan_phys_device chosen_device = {};

    // Now check for capabilities on those devices.
    // We first use the process of elimantion, crossing-out devices that don't match
    // the configuration then we compare remaining devices based on a heuristic.
    
    // Just generally useful for iteration.
    u32 combined_modes = (config->required_modes | config->optional_modes);
        
    for (u32 i = 0; i < physical_device_count; ++i) {
        // Retrieve all useful data about the device for checking later.
        //
        vulkan_phys_device curr_device = {};
        curr_device.handle = physical_devices[i];
        
        vulkan_device_from_capabilities(&curr_device, &curr_device.capabilities);

        u32 queue_family_count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(curr_device.handle, &queue_family_count, NULL);
        VkQueueFamilyProperties* queue_families = darray_from_data(VkQueueFamilyProperties, queue_family_count, NULL, allocator);
        vkGetPhysicalDeviceQueueFamilyProperties(curr_device.handle, &queue_family_count, queue_families);

        // We rate the queue families to find which one is best to use
        // queues on. We rate each queue family for each type it supports/
        for (u32 i = 0; i < queue_family_count; ++i) {
            VkQueueFamilyProperties* queue_family = &queue_families[i];

            for (u32 j = 0; j < VULKAN_QUEUE_FAMILY_COUNT; ++j) {
                // The algorithim discourages queue types with more supported types
                // plus a priority for each type, this means the queues will be more likely
                // be to seperated across the supported families and on less-expensive GPUs
                // it will just use less queue families.
                f64 score = score_queue_type(queue_family, (vulkan_queue_family)j);

                if (score > curr_device.phys_modes[j].score) {
                    curr_device.phys_modes[j].family_index = i;
                    curr_device.phys_modes[j].score = score;
                    curr_device.phys_modes[j].enabled = EMTRUE;
                }
            }
        }

        EM_INFO("Vulkan", "Checking physical device with name: '%s'", curr_device.capabilities.device_name);
        
        // We've now collected all the metrics into the data structures, now
        // starting elimating some sub-par devices with configuration.
        //
    
        if (physical_device_count > 1)
            curr_device.heuristic = score_phys_device(&curr_device);
        
        // The raster mode is basically graphics, enables the rasterisation pipeline
        // on the gpu which is a technique that turns points and connections into pixels
        // to 'light up' that collide with that shape, therefore rendering it. It also
        // includes running fragments shaders on those pixels to make it look pretty.
        if (combined_modes & EMBER_DEVICE_MODE_RASTER) {
            vulkan_sys_info raster = vulkan_raster_setup(&curr_device);
            
            // Do some tomfool-ly because Ember requires optional modes.
            if (!raster.enabled) {
                if (config->required_modes & EMBER_DEVICE_MODE_RASTER) {
                    EM_ERROR("Vulkan", "Skipping device: required raster mode is unavailable.");
                    continue;
                }

                if (config->optional_modes & EMBER_DEVICE_MODE_RASTER) {
                    EM_WARN("Vulkan", "Checking device: optional raster mode is unavailable.");
                }
            } else {
                EM_INFO("Vulkan", "Found raster queue family: %i", curr_device.phys_modes[VULKAN_QUEUE_FAMILY_RASTER].family_index);
                curr_device.phys_modes[VULKAN_QUEUE_FAMILY_RASTER] = raster;
                curr_device.capabilities.enabled_modes |= EMBER_DEVICE_MODE_RASTER;
            }
        }

        // The compute mode is really simple, it's a way of calcualting something
        // but on the GPU across its many, many cores. This is useful for advanced lighting
        // e.g. raytracing or physics calcualtions in a game or simulation. Doing many little small
        // tasks on the GPU is sometimes better than one big one on the CPU.
        if (combined_modes & EMBER_DEVICE_MODE_COMPUTE) {
            vulkan_sys_info compute = vulkan_compute_setup(&curr_device);

            // Do some tomfool-ly because Ember requires optional modes.
            if (!compute.enabled) {
                if (config->required_modes & EMBER_DEVICE_MODE_COMPUTE) {
                    EM_ERROR("Vulkan", "Skipping device: required compute mode is unavailable.");
                    continue;
                }

                if (config->optional_modes & EMBER_DEVICE_MODE_COMPUTE) {
                    EM_WARN("Vulkan", "Checking device: optional compute mode is unavailable.");
                }
            } else {
                EM_INFO("Vulkan", "Found compute queue family: %i", curr_device.phys_modes[VULKAN_QUEUE_FAMILY_COMPUTE].family_index);
                curr_device.phys_modes[VULKAN_QUEUE_FAMILY_COMPUTE] = compute;
                curr_device.capabilities.enabled_modes |= EMBER_DEVICE_MODE_COMPUTE;
            }
        }

        // The transfer mode is the easist to understand out of the four, its
        // how to transfer data between the CPU and GPU, this happens through those
        // huge cables coming out your GPU called PCIe cables, transferring does take time
        // and must be managed asynchronously.
        if (combined_modes & EMBER_DEVICE_MODE_TRANSFER) {
            vulkan_sys_info transfer = vulkan_transfer_setup(&curr_device);

            // Do some tomfool-ly because Ember requires optional modes.
            if (!transfer.enabled) {
                if (config->required_modes & EMBER_DEVICE_MODE_TRANSFER) {
                    EM_ERROR("Vulkan", "Skipping device: required transfer mode is unavailable.");
                    continue;
                }

                if (config->optional_modes & EMBER_DEVICE_MODE_TRANSFER) {
                    EM_WARN("Vulkan", "Checking device: required transfer mode is unavailable.");
                }
            } else {
                EM_INFO("Vulkan", "Found transfer queue family: %i", curr_device.phys_modes[VULKAN_QUEUE_FAMILY_TRANSFER].family_index);
                curr_device.phys_modes[VULKAN_QUEUE_FAMILY_TRANSFER] = transfer;
                curr_device.capabilities.enabled_modes |= EMBER_DEVICE_MODE_TRANSFER; 
            }
        }
        
        // Sampler anisotropy makes textures stay sharp when viewed at steep
        // angles instead of becoming blurry. For example, imagine looking down
        // a long road or floor tiles stretching into the distance. Without
        // anisotropic filtering the texture quickly loses detail, whereas with
        // it enabled the GPU samples the texture more intelligently to preserve
        // detail. It is slightly more expensive than normal texture filtering,
        // but on modern GPUs the quality improvement is usually well worth it.
        if (combined_modes & EMBER_DEVICE_MODE_SAMPLER_ANISOTROPY) {
            b8 sampler_anisotropy_supported =
                (curr_device.capabilities.max_anisotropy > 1.0f);

            // Do some tomfool-ly because Ember requires optional modes.
            if (!sampler_anisotropy_supported ) {
                if (config->required_modes & EMBER_DEVICE_MODE_SAMPLER_ANISOTROPY) {
                    EM_ERROR("Vulkan", "Skipping device: required sampler anisotropy mode is unavailable.");
                    continue;
                }

                if (config->optional_modes & EMBER_DEVICE_MODE_SAMPLER_ANISOTROPY) {
                    EM_WARN("Vulkan", "Checking device: required sampler anisotropy mode is unavailable.");
                }
            } else {
                curr_device.capabilities.enabled_modes |= EMBER_DEVICE_MODE_SAMPLER_ANISOTROPY; 
            }
        }
    
        // After we've checked the device is suitable, score it against all the other suitable GPUs.
        if (!chosen_device.handle ||
            curr_device.heuristic > chosen_device.heuristic)
            chosen_device = curr_device;
    }
    
    if (!chosen_device.handle) {
        EM_ERROR("Vulkan", "No suitable devices were found.");
        return EMBER_RESULT_UNAVAILABLE_API;
    }
    
    EM_INFO("Vulkan", "Selected physical device: '%s'", chosen_device.capabilities.device_name);
    
    // ----- Logical device creation -------------------------

    EM_INFO("Vulkan", "Creating logical device.");
    
    // We could of picked the same family index for R/C/T and we can't send duplicates
    // into logical device creation so we need to this complicated double for loop.
    f32 queue_priority = 1.0f;
    VkDeviceQueueCreateInfo* queue_create_infos = darray_reserve(VkDeviceQueueCreateInfo, VULKAN_QUEUE_FAMILY_COUNT, allocator);

    for (u32 i = 0; i < EM_ARRAYSIZE(chosen_device.phys_modes); ++i) {
        vulkan_sys_info* sys = &chosen_device.phys_modes[i];
        if (!sys->enabled) continue;

        b8 exists = EMFALSE;
        for (u32 j = 0; j < darray_length(queue_create_infos); ++j) {
            if (queue_create_infos[j].queueFamilyIndex == sys->family_index) {
                exists = EMTRUE;
                break;
            }
        }

        if (!exists) {
            VkDeviceQueueCreateInfo* create_info = darray_push_empty(queue_create_infos);
            create_info->sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            create_info->queueFamilyIndex = sys->family_index;
            create_info->queueCount       = 1;
            create_info->pQueuePriorities = &queue_priority;
        }
    }

    EM_TRACE("Vulkan", "Requesting %i unique queue families.", darray_length(queue_create_infos));
    
    // Fill create info
    VkPhysicalDeviceVulkan13Features dynamic_rendering = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES };
    dynamic_rendering.dynamicRendering = EMTRUE;
    dynamic_rendering.synchronization2 = EMTRUE;

    VkPhysicalDeviceVulkan12Features timeline_semaphores = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES };
    timeline_semaphores.pNext = &dynamic_rendering;
    timeline_semaphores.timelineSemaphore = EMTRUE;

    VkPhysicalDeviceFeatures device_features = {};

    const char* device_extensions[] = { "VK_KHR_dynamic_rendering", "VK_KHR_swapchain" };

    VkDeviceCreateInfo device_create_info = { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
    device_create_info.pNext = &timeline_semaphores;
    device_create_info.queueCreateInfoCount = darray_length(queue_create_infos);
    device_create_info.pQueueCreateInfos = queue_create_infos;
    device_create_info.enabledExtensionCount = EM_ARRAYSIZE(device_extensions);
    device_create_info.ppEnabledExtensionNames = device_extensions;
    device_create_info.pEnabledFeatures = &device_features;

    CHECK_VKRESULT(
        vkCreateDevice(chosen_device.handle, &device_create_info, vk_device->allocator, &vk_device->handle),
        "Failed to create logical device");

    vk_device->physical = chosen_device.handle;
    
    // Destroy temp data.
    darray_destroy(queue_create_infos);

    for (u32 i = 0; i < EM_ARRAYSIZE(chosen_device.phys_modes); ++i) {
        vulkan_sys_info* mode_info = &chosen_device.phys_modes[i];

        vulkan_sys_state* new_state = &vk_device->modes[i];
        new_state->family_index = chosen_device.phys_modes[i].family_index;
        new_state->commandbufs = darray_from_data(VkCommandBuffer, config->frames_in_flight, NULL, allocator);
        vkGetDeviceQueue(vk_device->handle, new_state->family_index, 0, &new_state->queue);

        VkCommandPoolCreateInfo pool_create_info = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
        pool_create_info.queueFamilyIndex = new_state->family_index;

        CHECK_VKRESULT(
            vkCreateCommandPool(vk_device->handle, &pool_create_info, vk_device->allocator, &new_state->pool), 
            "Failed to create mode command pool");

        VkCommandBufferAllocateInfo allocate_info = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
        allocate_info.commandPool = new_state->pool;
        allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocate_info.commandBufferCount = config->frames_in_flight;

        CHECK_VKRESULT(
            vkAllocateCommandBuffers(vk_device->handle, &allocate_info, new_state->commandbufs),
            "Failed to allocate mode command buffers");

        VkSemaphoreTypeCreateInfo timeline_info = { VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO };
        timeline_info.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;

        VkSemaphoreCreateInfo semaphore_info = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
        semaphore_info.pNext = &timeline_info;

        CHECK_VKRESULT(
            vkCreateSemaphore(vk_device->handle, &semaphore_info, vk_device->allocator, &new_state->semaphore), 
            "Failed to create mode timeline semaphore");
    }

    if (vk_device->wsi.requested) 
        vkGetDeviceQueue(vk_device->handle, vk_device->wsi.family_index, 0, &vk_device->wsi.queue);

    EM_INFO("Vulkan", "GPU device successfuly initialized.");
    return EMBER_RESULT_OK;
}

em_result emgpu_device_open_queue(emgpu_device* device, emgpu_queue* out_queue) {
    return EMBER_RESULT_OK;
}

em_result emgpu_device_submit(emgpu_device* device, emgpu_queue queue, const emgpu_command_buffer* command_buf) {
    vulkan_device* vk_device = (vulkan_device*)device->internal_context;

    vulkan_command_context ctx = {};
    ctx.allocator = &device->frame_allocator;
    ctx.command_buf = command_buf;
    ctx.submissions = darray_create(vulkan_command_context, ctx.allocator);
    ctx.surfaces    = darray_create(emgpu_surface*, ctx.allocator);
    ctx.stack       = darray_create(owner_frame, ctx.allocator);
    ctx.resource_table = mem_allocate(ctx.allocator, sizeof(managed_resource) * command_buf->current_resource_idx);

    // This is the big boy function; it decodes the entire command buffer and fills
    // the command context with submissions and surface calls to hand directly to the queue.
    // It also manages all the resource depenedecies and inserts dependency break all for
    // use, see vk_decoder.c.
    em_result result = vulkan_decode_command_buffer(device, &ctx);
    if (result != EMBER_RESULT_OK) return result;

    for (u32 i = 0; i < darray_length(ctx.submissions); ++i) {
        const vulkan_command_submission* submission = &ctx.submissions[i];

        VkCommandBufferSubmitInfo command_buf_info = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO };
        command_buf_info.commandBuffer = submission->handle;

        VkSubmitInfo2 submit_info = { VK_STRUCTURE_TYPE_SUBMIT_INFO_2 };
        submit_info.waitSemaphoreInfoCount   = darray_length(submission->waits);
        submit_info.pWaitSemaphoreInfos      = submission->waits;
        submit_info.commandBufferInfoCount   = 1;
        submit_info.pCommandBufferInfos      = &command_buf_info;
        submit_info.signalSemaphoreInfoCount = darray_length(submission->signals);
        submit_info.pSignalSemaphoreInfos    = submission->signals;

        CHECK_VKRESULT(
            vkQueueSubmit2(vk_device->modes[submission->queue].queue, 1, &submit_info, VK_NULL_HANDLE), 
            "Failed to submit to device queue");
    }

    for (u32 i = 0; i < darray_length(ctx.surfaces); ++i) {
        emgpu_surface* surface = ctx.surfaces[i];
        vulkan_surface* vk_surface = (vulkan_surface*)surface->internal_data;

        VkPresentInfoKHR present_info = { VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
        present_info.waitSemaphoreCount = 1;
        present_info.pWaitSemaphores    = &vk_surface->render_completes[vk_surface->image_index];
        present_info.swapchainCount     = 1;
        present_info.pSwapchains        = &vk_surface->swapchain;
        present_info.pImageIndices      = &vk_surface->image_index;

        CHECK_VKRESULT(
            vkQueuePresentKHR(
                vk_device->wsi.queue, &present_info), 
            "Failed to present swapchains to queue");
    }

    return EMBER_RESULT_OK;
}

void emgpu_device_shutdown(em_allocator* allocator, emgpu_device* device) {
    
}

em_result emgpu_device_get_capabilities(emgpu_device* device, emgpu_device_capabilities* out_capabilities) {
    return EMBER_RESULT_OK;
}

// ----- Raster mode entry point --------------------------
vulkan_sys_info vulkan_raster_setup(vulkan_phys_device* device) {
    vulkan_sys_info info = {};
    info.enabled    = device->phys_modes[VULKAN_QUEUE_FAMILY_RASTER].enabled;
    info.extensions = VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME;
    return info;
}

// ----- Compute mode entry point --------------------------
vulkan_sys_info vulkan_compute_setup(vulkan_phys_device* device) {
    vulkan_sys_info info = {};
    info.enabled    = device->phys_modes[VULKAN_QUEUE_FAMILY_COMPUTE].enabled;
    return info;
}

// ----- Transfer mode entry point --------------------------
vulkan_sys_info vulkan_transfer_setup(vulkan_phys_device* device) {
    vulkan_sys_info info = {};
    info.enabled    = device->phys_modes[VULKAN_QUEUE_FAMILY_TRANSFER].enabled;
    return info;
}
