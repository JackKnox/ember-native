#include "defines.h"
#include "vk_types.h"

#include "utils/darray.h"

#include <ember/gpu/device.h>

em_result emgpu_device_init(em_allocator* allocator, const emgpu_device_config* config, emgpu_device* out_device) {
    // Allocate massive internal context.
    out_device->internal_context  = mem_allocate(allocator, sizeof(vulkan_context));
    vulkan_context* context = (vulkan_context*)out_device->internal_context;

    EM_INFO("GPU", "Initialising GPU device with name: %s", config->debug_name);

    // Gather creation info.
	const char** required_extensions = darray_create(const char*, allocator);
	const char** required_validation_layers = darray_create(const char*, allocator);

    // TODO: Gather emgpu_device extension data.

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
    app_info.apiVersion         = VK_API_VERSION_1_2;
    
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
		vkCreateInstance(&create_info, context->allocator, &context->instance),
		"Failed to create Vulkan instance");
    
    EM_TRACE("Vulkan", "Created Vulkan instance with %i extension(s)", darray_length(required_extensions));

    // TODO: Setup validation layers, Ember is supposed to be the ultimate solution and not another common API lib
    //       so this isn't very important except for development.

	// Clean up temp arrays
    darray_destroy(supported_extensions);
	darray_destroy(supported_layers);
	darray_destroy(required_extensions);
	darray_destroy(required_validation_layers);

    // ----- Physical device iteration -----------------------
    // Build a list of availible Vulkan devices, physical ones! 
    u32 physical_device_count = 0;

    CHECK_VKRESULT(
        vkEnumeratePhysicalDevices(context->instance, &physical_device_count, NULL),    
        "Failed to enumerate physical devices");

    if (!physical_device_count) {
        EM_ERROR("Vulkan", "No devices which support Vulkan were found.");
        return EMBER_RESULT_UNAVAILABLE_API;
    }

    EM_TRACE("Vulkan", "Enumerated %i physical device(s)", physical_device_count);
    
    VkPhysicalDevice* physical_devices = darray_from_data(VkPhysicalDevice, physical_device_count, NULL, allocator);
    vkEnumeratePhysicalDevices(context->instance, &physical_device_count, physical_devices);
    
    // We will populate this below, submit to change as we iterate all the devices.
    vulkan_phys_device chosen_device = {};

    // Now check for capabilities on those devices.
    // We first use the process of elimantion, crossing-out devices that don't match
    // the configuration then we compare remaining devices based on a heuristic.
    
    // Just generally useful for iteration.
    u32 combined_modes = (config->required_modes | config->optional_modes);
        
    i32 curr_heuristic = -1;
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

        for (u32 i = 0; i < queue_family_count; ++i) {
            VkQueueFamilyProperties* queue_family = &queue_families[i];

            for (u32 j = 0; j < __VULKAN_QUEUE_FAMILY_COUNT; ++j) {
                f64 score = score_queue_type(queue_family, (vulkan_queue_family)j);

                if (score > curr_device.queue_families[j].score) {
                    curr_device.queue_families[j].family_index = i;
                    curr_device.queue_families[j].score = score;
                    curr_device.queue_families[j].enabled = EMTRUE;
                }
            }
        }

        EM_INFO("Vulkan", "Checking physical device with name: '%s'", curr_device.capabilities.device_name);
        
        // We've now collected all the metrics into the data structures, now
        // starting elimating some sub-par devices with configuration.
        //
    
        if (physical_device_count > 0)
            curr_device.heuristic = score_phys_device(&curr_device);
        
        // The raster mode is basically graphics, enables the rasterisation pipeline
        // on the gpu which is a technique that turns points and connections into pixels
        // to 'light up' that collide with that shape, therefore rendering it. It also
        // includes running fragments shaders on those pixels to make it look pretty.
        if (combined_modes & EMBER_DEVICE_MODE_RASTER) {
            b8 raster_supported =
                curr_device.queue_families[VULKAN_QUEUE_FAMILY_RASTER].enabled;
            
            // Do some tomfool-ly because Ember requires optional modes.
            if (!raster_supported) {
                if (config->required_modes & EMBER_DEVICE_MODE_RASTER) {
                    EM_ERROR("Vulkan", "Skipping device: required raster mode is unavailable.");
                    continue;
                }

                if (config->optional_modes & EMBER_DEVICE_MODE_RASTER) {
                    EM_ERROR("Vulkan", "Raster mode requested but unavailable; continuing without raster support.");
                }
            } else {
                curr_device.capabilities.enabled_modes |= EMBER_DEVICE_MODE_RASTER;
                EM_ERROR("Vulkan", "Found raster queue family: %i", curr_device.queue_families[VULKAN_QUEUE_FAMILY_RASTER].family_index);
            }
        }

        // The compute mode is really simple, it's a way of calcualting something
        // but on the GPU across its many, many cores. This is useful for advanced lighting
        // e.g. raytracing or physics calcualtions in a game or simulation. Doing many little small
        // tasks on the GPU is sometimes better than one big one on the CPU.
        if (combined_modes & EMBER_DEVICE_MODE_COMPUTE) {
            b8 compute_supported =
                curr_device.queue_families[VULKAN_QUEUE_FAMILY_COMPUTE].enabled;

            // Do some tomfool-ly because Ember requires optional modes.
            if (!compute_supported) {
                if (config->required_modes & EMBER_DEVICE_MODE_COMPUTE) {
                    EM_ERROR("Vulkan", "Skipping device: required compute mode is unavailable.");
                    continue;
                }

                if (config->optional_modes & EMBER_DEVICE_MODE_COMPUTE) {
                    EM_ERROR("Vulkan", "Checking device: required compute mode is unavailable (optional).");
                }
            } else {
                curr_device.capabilities.enabled_modes |= EMBER_DEVICE_MODE_COMPUTE;
                EM_ERROR("Vulkan", "Found compute queue family: %i", curr_device.queue_families[VULKAN_QUEUE_FAMILY_COMPUTE].family_index);
            }
        }

        // The transfer mode is the easist to understand out of the four, its
        // how to transfer data between the CPU and GPU, this happens through those
        // huge cables coming out your GPU called PCIe cables, transferring does take time
        // and must be managed asynchronously.
        if (combined_modes & EMBER_DEVICE_MODE_TRANSFER) {
            b8 transfer_supported =
                curr_device.queue_families[VULKAN_QUEUE_FAMILY_TRANSFER].enabled;

            // Do some tomfool-ly because Ember requires optional modes.
            if (!transfer_supported) {
                if (config->required_modes & EMBER_DEVICE_MODE_TRANSFER) {
                    EM_ERROR("Vulkan", "Skipping device: required transfer mode is unavailable.");
                    continue;
                }

                if (config->optional_modes & EMBER_DEVICE_MODE_TRANSFER) {
                    EM_ERROR("Vulkan", "Checking device: required transfer mode is unavailable (optional).");
                }
            } else {
                curr_device.capabilities.enabled_modes |= EMBER_DEVICE_MODE_TRANSFER; 
                EM_ERROR("Vulkan", "Found transfer queue family: %i", curr_device.queue_families[VULKAN_QUEUE_FAMILY_TRANSFER].family_index);
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
                (curr_device.capabilities.max_anisotropy > 0);

            // Do some tomfool-ly because Ember requires optional modes.
            if (!sampler_anisotropy_supported ) {
                if (config->required_modes & EMBER_DEVICE_MODE_SAMPLER_ANISOTROPY) {
                    EM_ERROR("Vulkan", "Skipping device: required sampler anisotropy mode is unavailable.");
                    continue;
                }

                if (config->optional_modes & EMBER_DEVICE_MODE_SAMPLER_ANISOTROPY) {
                    EM_ERROR("Vulkan", "Checking device: required sampler anisotropy mode is unavailable (optional).");
                }
            } else {
                curr_device.capabilities.enabled_modes |= EMBER_DEVICE_MODE_SAMPLER_ANISOTROPY; 
            }
        }
    
        // After we've checked the device is suitable, score it against all the other suitable GPUs.
        if (curr_device.heuristic > curr_heuristic)
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
    VkDeviceQueueCreateInfo* queue_create_infos = darray_reserve(VkDeviceQueueCreateInfo, __VULKAN_QUEUE_FAMILY_COUNT, allocator);

    for (u32 i = 0; i < EM_ARRAYSIZE(chosen_device.queue_families); ++i) {
        vulkan_phys_queue* queue = &chosen_device.queue_families[i];
        if (!queue->enabled) continue;

        b8 exists = EMFALSE;
        for (u32 j = 0; j < darray_length(queue_create_infos); ++j) {
            if (queue_create_infos[j].queueFamilyIndex == queue->family_index) {
                exists = EMTRUE;
                break;
            }
        }

        if (!exists) {
            VkDeviceQueueCreateInfo* create_info = darray_push_empty(queue_create_infos);
            create_info->sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            create_info->queueFamilyIndex = queue->family_index;
            create_info->queueCount       = 1;
            create_info->pQueuePriorities = &queue_priority;
        }
    }

    EM_TRACE("Vulkan", "Requesting %i unique queue families.", darray_length(queue_create_infos));
    
    // Fill create info
    VkPhysicalDeviceFeatures device_features = {};

    VkDeviceCreateInfo device_create_info = { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
    device_create_info.queueCreateInfoCount = darray_length(queue_create_infos);
    device_create_info.pQueueCreateInfos = queue_create_infos;
    //device_create_info.enabledExtensionCount = darray_length(required_device_extensions);
    //device_create_info.ppEnabledExtensionNames = required_device_extensions;
    device_create_info.pEnabledFeatures = &device_features;

    CHECK_VKRESULT(
        vkCreateDevice(chosen_device.handle, &device_create_info, context->allocator, &context->device.handle),
        "Failed to create logical device");
    
    // Destroy temp data.
    darray_destroy(queue_create_infos);

    EM_INFO("Vulkan", "GPU device successfuly initialized.");
    return EMBER_RESULT_OK;
}

void emgpu_device_shutdown(em_allocator* allocator, emgpu_device* device) {
    
}

em_result emgpu_device_get_capabilities(emgpu_device* device, emgpu_device_capabilities* out_capabilities) {
    
}
