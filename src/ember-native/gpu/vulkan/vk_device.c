#include "defines.h"
#include "vk_types.h"

#include <ember/gpu/device.h>

em_result emgpu_device_init(const emgpu_device_config* config, em_allocator* allocator, emgpu_device* out_device) {
    // Allocate massive internal context.
    out_device->internal_context  = mem_allocate(allocator, sizeof(vulkan_context), MEMORY_TAG_RENDERER);
    vulkan_context* context = (vulkan_context*)out_device->internal_context;

    // Gather creation info.
	const char** required_extensions = darray_create(const char*, NULL, MEMORY_TAG_RENDERER);
	const char** required_validation_layers = darray_create(const char*, NULL, MEMORY_TAG_RENDERER);

    // TODO: Gather emgpu_device extension data.

    // ----- Vulkan instance ---------------------------------
    // Verify exsistence of extensions
    u32 supported_extension_count = 0;
    vkEnumerateInstanceExtensionProperties(NULL, &supported_extension_count, NULL);

    VkExtensionProperties* supported_extensions = darray_from_data(VkExtensionProperties, supported_extension_count, NULL, NULL, MEMORY_TAG_RENDERER);
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

	VkLayerProperties* supported_layers = darray_from_data(VkLayerProperties, supported_layer_count, NULL, NULL, MEMORY_TAG_RENDERER);
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
    
	CHECK_VKRESULT(
		vkCreateInstance(&create_info, context->allocator, &context->instance),
		"Failed to create Vulkan instance");
	
	// Clean up temp arrays
    darray_destroy(supported_extensions);
	darray_destroy(supported_layers);
	darray_destroy(required_extensions);
	darray_destroy(required_validation_layers);
}

void emgpu_device_shutdown(em_allocator* allocator, emgpu_device* device) {
    
}

em_result emgpu_device_get_capabilities(emgpu_device* device, emgpu_device_capabilities* out_capabilities) {
    
}