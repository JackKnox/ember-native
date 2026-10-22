
// Example when we open a window then clear a colour to it.
//
#define EMBER_DEFINE_HELPERS

#include <ember/platform/system.h>
#include <ember/platform/logger.h>

#include <ember/window/window.h>
#include <ember/window/events.h>
#include <ember/gpu/ext/emwin_surface.h>

#include <ember/gpu/command_buffer.h>
#include <ember/gpu/raster.h>

#include <stddef.h>
#include <stdbool.h>

// A small helper macro to cut down on the repetitive error-check boilerplate.
// Calls a function, checks the result, logs a message and jumps to cleanup if it failed.
// em_result_string() converts the result code into something human readable.
#define CHECK_FUNC(func, message)                          \
    {                                                      \
        em_result result = func;                           \
        if (result != EMBER_RESULT_OK) {                   \
            emplat_printf(EMBER_LOG_LEVEL_ERROR, message ": %s", \
				em_result_string(result, true));           \
            goto cleanup;                                  \
        }                                                  \
    }

int main(int argc, char** argv) {
	// The Allocator API. A simple structure wrapping malloc / free / realloc and
	// some metadata. Every Ember subsystem takes one of these so you can plug in
	// a custom allocator (arena, pool, tracking, etc.) wherever you need to.
	// em_allocator_default() just hands back the OS allocator — malloc and free.
	em_allocator system_alloc = emplat_system_allocator();

	// Before we can open a window we need to fill out a config.
	// emwin_window_default() pre-fills sensible values so we only need to touch the fields we actually care about.
	emwin_window_config window_config = emwin_window_default();
	window_config.title   = "Example - Clear Colour"; // Shows up in the title bar; uses UTF-8 strings.
	window_config.size.x  = 640;                      // Initial dimensions of the window in pixels.
	window_config.size.y  = 640;             
	//window_config.desktop = NULL;

	// emwin_desktop represents our connection to the system's window manager.
	// emwin_window_open will create one for us and write the pointer here.
    emwin_desktop* desktop = NULL;

	// Open the window. The desktop pointer is also populated here as a side effect —
	// we need it for the GPU surface extension below.
	emwin_window window = {};
	CHECK_FUNC(
		emwin_window_open(&system_alloc, &window_config, 0, &window, &desktop), 
		"Failed to open window");
	
	// The GPU device supports extensions for platform-specific surface creation.
	// Using ember_window we use the emwin_surface extension and pass it our desktop handle
	// so the backend knows which desktop to talk to.
    emgpu_emwin_surface_ext wsi_extension = {};

    emgpu_extension_desc extensions[] = { emgpu_register_emwin_surface(desktop, &wsi_extension) };

	// Now configure the GPU device. Like the window, there's a _default() helper
	// that pre-fills everything sensible so we only need to set what matters.
	emgpu_device_config device_config = emgpu_device_default();
	device_config.debug_name       = window_config.title;    // Shows up in GPU debug tooling.
	device_config.frame_allocator  = emplat_system_allocator(); // Allocator used to allocate frame-local resources.
	device_config.app_version      = EMBER_VERSION;
	device_config.required_modes   = EMBER_DEVICE_MODE_RASTER; // We need it — no point continuing without it.
	device_config.optional_modes   = EMBER_DEVICE_MODE_VALIDATION; // Nice to have for debugging but we won't bail if it's unavailable.
	device_config.frames_in_flight = 3;                            // Triple buffering.
    device_config.extension_count  = EM_ARRAYSIZE(extensions);
    device_config.extensions       = extensions;    // Pass in the extensions we want...
	
	emgpu_device device = {};
	CHECK_FUNC(
		emgpu_device_init(&system_alloc, &device_config, &device), 
		"Failed to create rendering device");

	// Let's see what the chosen device actually supports and print it to the trace logger — handy during development.
	emgpu_device_capabilities capabilities = {};
	CHECK_FUNC(
		emgpu_device_get_capabilities(&device, &capabilities),
		"Failed to retrieve device capabilities");

    // Now we can create a ember_window-backed surface using the function pointer the device
	// extension filled in for us. This connects the Vulkan swapchain to our window.
	emgpu_emwin_surface_config surface_config = emgpu_emwin_surface_default();
	surface_config.preferred_format  = EMGPU_FORMAT_BGRA8_UNORM; // Common format; force_format = FALSE means we fall back gracefully if unavailable.
	surface_config.force_format      = false;                    // force_format = TRUE means it will only accept the exact format. Still preserves colour / depth / stencil type.
    surface_config.min_texture_count = 3; // Usually three.
    surface_config.usage             = EMBER_TEXTURE_USAGE_ATTACHMENT_DST; // How we are using the outputed texture on exec
    surface_config.window            = &window;

	// Call the function from the 'out extension' which creates the window.
	emgpu_surface surface = {};
	CHECK_FUNC(
		wsi_extension.create_surface(&device, &system_alloc, &surface_config, &surface),
		"Failed to create window surface");

    emgpu_queue main_queue = 0ULL;
    CHECK_FUNC(
        emgpu_device_open_queue(&device, &main_queue),
        "Failed to open main GPU device queue");

    /*
    b8 running = EMTRUE;
	while (running) {
        emwin_desktop_event desk_event = {};

        while (emwin_poll_events(desktop, &desk_event) == EMBER_RESULT_OK) {
            switch (desk_event.type) {
                case EMWIN_EVENT_WINDOW_CLOSE:
                    running = EMFALSE;
                    break;
                default: break;
            }
        }


        // Each frame is driven by emgpu_frame. It manages per-frame allocations
        // using the device's frame_allocator — everything allocated here is
        // automatically freed when the frame is submitted.
        emgpu_command_buffer frame = {};
        CHECK_FUNC(
            emgpu_command_buffer_create(&device, &frame),
            "Failed to init frame command buffer");

        // Grab the next texture from the swapchain to render into.
        emgpu_local_framebuffer window_tex = emgpu_cmd_acquire_surface(&frame, &surface);

        emgpu_colour_attachment attachments[] = {
            {
                .framebuffer = window_tex,
                .load_op = EMBER_LOAD_OP_CLEAR,
                .store_op = EMBER_STORE_OP_STORE,
                .presentable = EMTRUE,
                .clear_colour = 0xFFAAAAFF
            }
        };

        emgpu_renderpass_config render_begin_info = emgpu_renderpass_default();
        render_begin_info.render_origin = (uvec2) { 0, 0 };
        render_begin_info.render_size   = window.size;

        render_begin_info.colour_attachments = attachments;
        render_begin_info.colour_attachment_count = EM_ARRAYSIZE(attachments);
        emgpu_cmd_begin_renderpass(&frame, &render_begin_info);
        
        emgpu_cmd_set_viewport(&frame, (uvec2) { 0, 0 }, window.size, 0.0f, 1.0f);
        emgpu_cmd_set_scissor(&frame, (uvec2) { 0, 0 }, window.size);

        emgpu_cmd_end_renderpass(&frame);

        // Submit the frame to the GPU. This also handles presentation — CHECK_FUNC
        // will jump to cleanup if something goes wrong here.
        emgpu_device_submit(&device, main_queue, &frame);

    }
    */

	// Cleanup. Order matters here — destroy GPU resources before shutting down
	// the device, and shut down the device before closing the window.
	// The allocator passed to each destroy call MUST match the one used to create it
	// or you'll corrupt the allocator's internal state.
cleanup:
	emgpu_surface_destroy(&device, &system_alloc, &surface);
	emgpu_device_shutdown(&system_alloc, &device);
	emwin_window_close(&system_alloc, &window); // Also tears down the desktop connection.
	return 0;
}
