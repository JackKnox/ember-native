#pragma once

#include "defines.h"

#include <ember/gpu/device.h>
#include <ember/gpu/resources.h>
#include <ember/gpu/compute.h>
#include <ember/gpu/raster.h>
#include <ember/gpu/surface.h>

typedef enum cmd_payload_type {
    COMMAND_BEGIN_COMPUTEPASS,
    COMMAND_DISPATCH,
    COMMAND_END_COMPUTEPASS,

    COMMAND_BEGIN_RENDERPASS,
    COMMAND_END_RENDERPASS,
    COMMAND_SET_VIEWPORT,
    COMMAND_SET_SCISSOR,

    COMMAND_BIND_RASTER_PIPELINE,
    COMMAND_BIND_VERTEX_BUFFERS,
    COMMAND_BIND_INDEX_BUFFER,
    COMMAND_DRAW,
    
    COMMAND_EMPTY_RESOURCE,

    COMMAND_IMPORT_TEXTURE,

    COMMAND_ACQUIRE_SURFACE,
} cmd_payload_type;

typedef struct pipeline_bind_info {
    const emgpu_pipeline* pipeline;
    const emgpu_resource_import* imports;
    const emgpu_resource_export* exports;
    u32 import_count;
    u32 export_count;
} pipeline_bind_info;

typedef struct cmd_payload {
    cmd_payload_type type;

    union {
        pipeline_bind_info begin_computepass;

        struct {
            uvec3 group_size;
        } dispatch;

        emgpu_renderpass_config begin_renderpass;

        struct {
            uvec2 origin, size;
            f32 min_depth, max_depth;
        } set_viewport;

        struct {
            uvec2 origin, size;
        } set_scissor;

        pipeline_bind_info bind_raster_pipeline;

        struct {
            emgpu_buffer* buffers;
            u32 count;
        } bind_vertex_buffers;

        emgpu_buffer* bind_index_buffer;

        struct {
            u32 vertex_count, instance_count;
        } draw;
        
        emgpu_local_resource empty_resource;

        struct {
            emgpu_texture* texture;
            emgpu_local_framebuffer dst_framebuffer;
        } import_texture;

        struct {
            emgpu_surface* surface;
            emgpu_local_framebuffer dst_framebuffer;
        } acquire_surface;
    };
} cmd_payload;

b8 emnat_poll_command_buffer(
    const emgpu_command_buffer* command_buf, 
    void** out_cursor);
