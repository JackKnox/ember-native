#pragma once

#include "defines.h"

#include <ember/gpu/device.h>
#include <ember/gpu/resources.h>
#include <ember/gpu/compute.h>
#include <ember/gpu/raster.h>
#include <ember/gpu/surface.h>

typedef enum cmd_payload_type {
    COMMAND_EMPTY, // Prevents against corrupted memory.

    COMMAND_BEGIN_COMPUTEPASS,
    COMMAND_DISPATCH,
    COMMAND_END_COMPUTEPASS,

    COMMAND_BEGIN_RENDERPASS,
    COMMAND_END_RENDERPASS,
    COMMAND_SET_VIEWPORT,
    COMMAND_SET_SCISSOR,

    COMMAND_BIND_RASTER_PIPELINE,
    COMMAND_BIND_INDEX_BUFFER,
    COMMAND_DRAW,
    
    COMMAND_EMPTY_RESOURCE,

    COMMAND_IMPORT_TEXTURE,

    COMMAND_ACQUIRE_SURFACE,
    
    // Array payloads
    // -------------------------
    COMMAND_EXPORT_RESOURCES,
    COMMAND_IMPORT_RESOURCES,
    COMMAND_COLOUR_ATTACHMENTS,
    COMMAND_BIND_VERTEX_BUFFERS
} cmd_payload_type;

typedef struct cmd_header {
    cmd_payload_type type;
    u64 size;
} cmd_header;

typedef struct {
    const emgpu_pipeline* pipeline;
    // export_resources, import_resources.
} cmd_bind_pipeline; // Equalivent of begin_computepass and bind_raster_pipeline

typedef struct {
    uvec3 group_size;
} cmd_dispatch;

typedef struct {
    uvec2 render_origin, render_size;
    // colour_attachments
} cmd_begin_renderpass;

typedef struct {
    uvec2 origin, size;
    f32 min_depth, max_depth;
} cmd_set_viewport;

typedef struct {
    uvec2 origin, size;
} cmd_set_scissor;

typedef struct {
    emgpu_buffer* index_buffer;
} cmd_bind_index_buffer;

typedef struct {
    u32 vertex_count, instance_count;
} cmd_draw;

typedef struct {
    emgpu_local_resource dst_resource;
} cmd_empty_resource;

typedef struct {
    emgpu_texture* texture;
    emgpu_local_framebuffer dst_framebuffer;
} cmd_import_texture;

typedef struct {
    emgpu_surface* surface;
    emgpu_local_framebuffer dst_framebuffer;
} cmd_acquire_surface;

typedef struct cmd_payload {
    cmd_payload_type type;
    u64 size;

    union {
        void* raw_ptr;

        cmd_bind_pipeline* bind_pipeline;
        cmd_dispatch* dispatch;
        cmd_begin_renderpass* begin_renderpass;
        cmd_set_viewport* set_viewport;
        cmd_set_scissor* set_scissor;
        cmd_bind_index_buffer* bind_index_buffer;
        cmd_draw* draw;
        cmd_empty_resource* empty_resource;
        cmd_import_texture* import_texture;
        cmd_acquire_surface* acquire_surface;
        
        emgpu_resource_export* export_resources;

        emgpu_resource_import* imports_resources;

        emgpu_colour_attachment* colour_attachments;

        emgpu_buffer** bind_vertex_buffers;
    };
} cmd_payload;

b8 emnat_poll_command_buffer(
    const emgpu_command_buffer* command_buf, 
    cmd_payload* out_cursor);
