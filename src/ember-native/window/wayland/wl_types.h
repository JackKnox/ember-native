#pragma once

#include "ember/core.h"

#include "protocols/wayland-client.h"
#include "protocols/xdg-shell-client.h"

#include <ember/window/window.h>

typedef struct wayland_desktop {
    struct wl_display*    display;
    struct wl_registry*   registry;
    struct wl_compositor* compositor;
    struct wl_shm*        shm;
    struct xdg_wm_base*   xdg_wm_base;
    struct wl_registry_listener registry_listener;
    struct xdg_surface_listener surface_listener;
    struct xdg_wm_base_listener xdg_wm_base_listener;
    struct xdg_toplevel_listener xdg_toplevel_listener;
} wayland_desktop;

em_result emwl_desktop_create(em_allocator* allocator, struct emwin_desktop** out_desktop);

typedef struct wayland_window { 
    struct wl_surface* surface;
    struct xdg_surface* xdg_surface;
    struct xdg_toplevel* xdg_toplevel;
    b8 should_close;
} wayland_window;
