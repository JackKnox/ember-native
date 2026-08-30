#include "defines.h"
#include "wl_types.h"

#include <ember/window/desktop.h>
#include <ember/window/events.h>

void configure_xdg_surface(void* data,
			  struct xdg_surface* xdg_surface,
			  uint32_t serial) {
	emwin_window* window = (emwin_window*)data;
	wayland_window* internal_window = (wayland_window*)window->internal_context;

	// This and ping are kind of the same thing. It prevents the 'not responding' message.
	xdg_surface_ack_configure(xdg_surface, serial);
}

void ping_xdg_wm_base(void* data,
		      struct xdg_wm_base* xdg_wm_base,
		      uint32_t serial) {
	emwin_desktop* desktop = (emwin_desktop*)data;
	wayland_desktop* internal_desktop = (wayland_desktop*)desktop->internal_context;

	// This and configure are kind of the same thing. It prevents the 'not responding' message.
	xdg_wm_base_pong(xdg_wm_base, serial);
}

// Basically means the state of the window has changed, please react to it somehow.
void configure_xdg_toplevel(void *data,
			  struct xdg_toplevel *xdg_toplevel,
			  int32_t width,
			  int32_t height,
			  struct wl_array *states) {
	emwin_window* window = (emwin_window*)data;
	wayland_window* internal_window = (wayland_window*)window->internal_context;
	
	window->size.x = width;
	window->size.y = height;
	
	b8 maximised = EMFALSE;

    uint32_t *state;
	wl_array_for_each(state, states) {
		switch (*state) {
			case XDG_TOPLEVEL_STATE_MAXIMIZED:
				maximised = EMTRUE;
				break;
		}
	}
    
    // TODO: Push window resize event.
}

// Its a little suggestion from Wayland to safely close the window. It can force us tho...
void close_xdg_toplevel(void *data,
		      struct xdg_toplevel *xdg_toplevel) {
	emwin_window* window = (emwin_window*)data;
	wayland_window* internal_window = (wayland_window*)window->internal_context;

    // TODO: Push window close event.
	internal_window->should_close = EMTRUE;
}

// Use this to get all the super cool managing objects from Wayland.
// Its setup like this to support lots of extensions, you want to use an extension retrieve from here.
void registry_global_add(void* data,
			  struct wl_registry* registry,
			  uint32_t name,
			  const char* interface, uint32_t version) {
	emwin_desktop* desktop = (emwin_desktop*)data;
	wayland_desktop* internal_desktop = (wayland_desktop*)desktop->internal_context;

	if (strcmp(interface, "wl_compositor") == 0) {
		internal_desktop->compositor 
			= wl_registry_bind(registry, name, &wl_compositor_interface, 3);
	}
	else if (strcmp(interface, "wl_shm") == 0) {
		internal_desktop->shm 
			= wl_registry_bind(registry, name, &wl_shm_interface, 1);
	}
	else if (strcmp(interface, "xdg_wm_base") == 0) {
		internal_desktop->xdg_wm_base 
			= wl_registry_bind(registry, name, &xdg_wm_base_interface, 1);
		xdg_wm_base_add_listener(internal_desktop->xdg_wm_base, &internal_desktop->xdg_wm_base_listener, (void*)desktop);
	}
}

// idk what this does...
void registry_global_remove(
    void* data,
    struct wl_registry* registry,
    uint32_t name) {
}

em_result emnat_wayland_desktop_create(em_allocator* allocator, struct emwin_desktop** out_desktop) {
    emwin_desktop* desktop = mem_allocate(allocator, sizeof(emwin_desktop));

	desktop->internal_context = mem_allocate(allocator, sizeof(wayland_desktop));
	wayland_desktop* internal_desktop = (wayland_desktop*)desktop->internal_context;

    internal_desktop->registry_listener.global        = registry_global_add;
    internal_desktop->registry_listener.global_remove = registry_global_remove;
    internal_desktop->surface_listener.configure      = configure_xdg_surface;
    internal_desktop->xdg_wm_base_listener.ping       = ping_xdg_wm_base;
    internal_desktop->xdg_toplevel_listener.configure = configure_xdg_toplevel;
    internal_desktop->xdg_toplevel_listener.close     = close_xdg_toplevel;

    // Connect to the main Wayland object, underneath this is a UNIX socket handshake.
    internal_desktop->display = wl_display_connect(NULL);
    
    // The registry is core global object discovery mechanism. It’s how a client learns what the compositor is offering.
    internal_desktop->registry = wl_display_get_registry(internal_desktop->display);

    wl_registry_add_listener(internal_desktop->registry, &internal_desktop->registry_listener, (void*)desktop);
    wl_registry_set_user_data(internal_desktop->registry, (void*)desktop);
    
	// Wait for all the values to fill up with useful stuff with our callbacks.
    wl_display_roundtrip(internal_desktop->display);

    *out_desktop = desktop;
    return EMBER_RESULT_OK;
}

em_result emwin_poll_events(emwin_desktop* desktop, emwin_desktop_event* out_event) {

}

em_result emwin_set_clipboard_text(emwin_desktop* desktop, const char* text) {
    
}

char* emwin_get_clipboard_text(emwin_desktop* desktop) {
    
}

b8 emwin_has_clipboard_text(emwin_desktop* desktop) {
    
}
