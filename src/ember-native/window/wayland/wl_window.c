#include "defines.h"
#include "wl_types.h"

#include <ember/window/window.h>

em_result emwin_window_open(em_allocator* allocator, const emwin_window_config* config, emwin_monitor_id id, emwin_window* out_window, emwin_desktop** out_desktop) {
    if (*out_desktop == NULL) {
        em_result result = emnat_wayland_desktop_create(allocator, out_desktop);
        if (result != EMBER_RESULT_OK)
            return result;
    } 


	out_window->internal_context = mem_allocate(allocator, sizeof(wayland_window));
	wayland_window* internal_window = (wayland_window*)out_window->internal_context;

	out_window->size = config->size;
	out_window->desktop = *out_desktop;

    wayland_desktop* internal_desktop = (wayland_desktop*)(*out_desktop)->internal_context;

	// Copy window title into managed buffer this is so we aren't accessing stale memory later.
	u32 string_length = (strlen(config->title) + 1) * sizeof(char);
	out_window->title = mem_allocate(allocator, string_length);
	memcpy(out_window->title, config->title, string_length);

	// Create a wayland surface. A surface is just a managed buffer with an assigned role (Cursor, Window, etc.)
	internal_window->surface = wl_compositor_create_surface(internal_desktop->compositor);

	// This uses the xdg-shell extensions to create an acutal window from it.
	internal_window->xdg_surface = xdg_wm_base_get_xdg_surface(internal_desktop->xdg_wm_base, internal_window->surface);
	xdg_surface_add_listener(internal_window->xdg_surface, &internal_desktop->surface_listener, (void*)out_window);

	// Top level is the actual object that is the 'window' in Wayland. 
	// I dont know why xdg_surface and xdg_toplevel are different but who cares.
	internal_window->xdg_toplevel = xdg_surface_get_toplevel(internal_window->xdg_surface);
	
	xdg_toplevel_add_listener(internal_window->xdg_toplevel, &internal_desktop->xdg_toplevel_listener, (void*)out_window);

    // Sets the title, could set App ID here but idk.
	xdg_toplevel_set_title(internal_window->xdg_toplevel, config->title);
	
	// Sets limits for window if set, kinda cool this is in the standard.
	if (config->min_size.x != 0 || config->min_size.y != 0)
		xdg_toplevel_set_min_size(internal_window->xdg_toplevel, config->min_size.x, config->min_size.y);

	if (config->max_size.x != 0 || config->max_size.y != 0)
		xdg_toplevel_set_max_size(internal_window->xdg_toplevel, config->max_size.x, config->max_size.y);

	// Commit all the changes we've just made to the surface.
	wl_surface_commit(internal_window->surface);

	// Wait for all the values to fill up with useful stuff with our callbacks.
	wl_display_roundtrip(internal_desktop->display);
    return EMBER_RESULT_OK;
}

void emwin_window_close(em_allocator* allocator, emwin_window* window) {
    
}

void emwin_window_request_close(emwin_window* window) {
	wayland_window* internal_window = (wayland_window*)window->internal_context;
	internal_window->should_close = EMTRUE;
}

b8 emwin_window_should_close(const emwin_window* window) {
	wayland_window* internal_window = (wayland_window*)window->internal_context;
	return internal_window->should_close;
}
