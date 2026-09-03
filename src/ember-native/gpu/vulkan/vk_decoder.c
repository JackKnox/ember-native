#include "defines.h"
#include "ember/gpu/types.h"
#include "vk_types.h"

#include "utils/darray.h"

/*
 * Track resources across pipeline, queues, image layouts and semaphore dependencys.
 *
 * Pipeline barries and semaphore barriers.
 *
 * Multi-surface support.
 *
 * Pipeline stage wait flags.
 *
 * Allocate more command buffer if nessacary.
 *
 * Semaphores and fences are used per-frame.
 *
 * Timeline semaphores are used per-queue.
 *
 * Types of resource dependency:
 *     * Timeline break
 *     * Cross-queue pipeline barrier
 *     * Pipeline barrier
 *     * Binary semaphore
 *     * Renderpass / subpass
 */

// Turns a Vulkan command buffer into a proper submission struct. If there is no
// command buffer provided it creates a temporary one.
em_result new_submission(vulkan_command_context* ctx, vulkan_queue_family family, VkCommandBuffer command_buffer, vulkan_command_submission** out_submission) {
    if (!command_buffer) return EMBER_RESULT_UNIMPLEMENTED;

    vulkan_command_submission* new_submission = darray_push_empty(ctx->submissions);
    new_submission->handle = command_buffer;
    new_submission->queue = family;

    VkCommandBufferBeginInfo begin_info = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    CHECK_VKRESULT(
        vkBeginCommandBuffer(new_submission->handle, &begin_info), 
        "Failed to begin command buffer");

    *out_submission = new_submission;
    return EMBER_RESULT_OK;
}

void end_submission(vulkan_command_context* ctx, vulkan_command_submission* submission) {
    vkEndCommandBuffer(submission->handle);
}

// Declears a new stack frame as a owner of the following calls to `own_resource`.
// This is how the system knows where data is being transported and then to insert dependency edges.
// Resources acquired or 'owned' after this will be handed back at `release_resources` and all sync operations
// will be operated. The pair of declaring a owner and release resources acts like a code stack.
void declare_owner(vulkan_command_context* ctx, owner_desc* owner) {
    owner_frame* frame = darray_push_empty(ctx->stack);
    frame->desc = *owner;
}

// This inserts a resource into the system based on the `dst_handle` and the currently
// set owner. The managed resource handle may refer to any buffer, texture, or framebuffer.
// The `curr_access` variable refers to the owner own access with the resource, this is used in pipeline barriers.
void insert_resource(vulkan_command_context* ctx, emgpu_local_resource dst_handle, managed_resource* resource) {
    ctx->resource_table[(u32)dst_handle] = *resource;
}

// This uses the given handle in the submit-wide table to give back an actual Vulkan
// handle. It will perform the nessacery actions to do so in this exact function.
managed_resource* own_resource(vulkan_command_context* ctx, emgpu_local_resource handle, emgpu_access_flags needed_access) {
    owner_frame* owner = darray_last(ctx->stack);

    resource_use* use = darray_push_empty(owner->uses);
    use->resource = handle;
    use->access = needed_access;
    return &ctx->resource_table[(u32)handle]; 
}

// Pops an owner frame from the submit-stack, releases ownership of resources and
// emits all nessacery dst Vulkan sync opertions. If there is no stack frames just return.
void release_resources(vulkan_command_context* ctx) {

}

// Adds the provided surface to the submit-wide list, this implictly means that a surface's
// framebuffer is being used this frame so the system will automattically present the surface
// after submit the command buffers.
emgpu_surface* add_surface(vulkan_command_context* ctx, emgpu_surface* surface) {
    darray_push(ctx->surfaces, surface);
    return *darray_last(ctx->surfaces);
}

static void cmd_bind_pipeline(vulkan_command_context* ctx, pipeline_bind_info* info) {
    vulkan_pipeline* vk_pipeline = (vulkan_pipeline*)info->pipeline->internal_data;

    owner_desc pipeline_owner = {};
    pipeline_owner.type = COMMAND_OWNER_PIPELINE;
    pipeline_owner.pipeline = info->pipeline;
    declare_owner(ctx, &pipeline_owner);

    const emgpu_resource_import* import = info->imports;
    for (u32 i = 0; i < info->import_count; ++i, ++import)
        own_resource(ctx, import->resource, import->access_flags);

    //const emgpu_resource_export* export = info->exports;
    //for (u32 i = 0; i < info->export_count; ++i, ++export)
    //    insert_resource(ctx, export->resource, export->access_flags, (void*)vk_pipeline->descriptors[export->src_binding]);

    ctx->bound_pipeline = EMTRUE;

    vkCmdBindPipeline(ctx->curr_submission->handle, 
            vulkan_bind_point(info->pipeline->type),
            vk_pipeline->handle);
}

static void cmd_begin_renderpass(vulkan_command_context* ctx, emgpu_renderpass_config* config) {
    owner_desc renderpass_owner = {};
    renderpass_owner.type = COMMAND_OWNER_RENDERPASS;
    //renderpass_owner.renderpass = NULL;
    declare_owner(ctx, &renderpass_owner);

    VkRenderingAttachmentInfo* colour_attachments = darray_reserve(VkRenderingAttachmentInfo, config->colour_attachment_count, ctx->allocator);
    
    VkRenderingInfo rendering_info = { VK_STRUCTURE_TYPE_RENDERING_INFO };
    rendering_info.renderArea.offset = (VkOffset2D) { config->render_origin.x, config->render_origin.y };
    rendering_info.renderArea.extent = (VkExtent2D) { config->render_size.x, config->render_size.y };
    rendering_info.layerCount = 1;

    for (u32 i = 0; i < config->colour_attachment_count; ++i) {
        const emgpu_colour_attachment* attachment = &config->colour_attachments[i];

        VkRenderingAttachmentInfo* vk_attachment = darray_push_empty(colour_attachments);
        vk_attachment->sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;

        managed_resource* resc = own_resource(ctx, attachment->framebuffer, EMBER_ACCESS_COLOUR_ATTACHMENT_WRITE);
        vulkan_texture* vk_framebuffer = (vulkan_texture*)resc->data.texture->internal_data;
        vk_attachment->imageView = vk_framebuffer->view;
        vk_attachment->imageLayout = vk_framebuffer->layout;
        vk_attachment->loadOp = vulkan_load_op_type(attachment->load_op);
        vk_attachment->storeOp = vulkan_store_op_type(attachment->store_op);
        vk_attachment->clearValue.color.float32[0] = ((attachment->clear_colour >> 24) & 0xFF) / 255.0f;
        vk_attachment->clearValue.color.float32[1] = ((attachment->clear_colour >> 16) & 0xFF) / 255.0f;
        vk_attachment->clearValue.color.float32[2] = ((attachment->clear_colour >> 8)  & 0xFF) / 255.0f;
        vk_attachment->clearValue.color.float32[3] = ((attachment->clear_colour)       & 0xFF) / 255.0f;
    }

    rendering_info.colorAttachmentCount = darray_length(colour_attachments);
    rendering_info.pColorAttachments    = colour_attachments;

    vkCmdBeginRendering(ctx->curr_submission->handle, 
            &rendering_info);

    darray_destroy(colour_attachments);
}

static void cmd_bind_vertex_buffers(vulkan_command_context* ctx, u32 count, emgpu_buffer* buffers) {
    if (count == 1) {
        VkDeviceSize offset = 0;

        vulkan_buffer* buffer = (vulkan_buffer*)buffers[0].internal_data;
        vkCmdBindVertexBuffers(ctx->curr_submission->handle, 0, 1, &buffer->handle, &offset);
        return;
    }
}

em_result vulkan_decode_command_buffer(emgpu_device* device, vulkan_command_context* ctx, const emgpu_command_buffer* commmand_buf) {
    vulkan_context* context = (vulkan_context*)device->internal_context;

    cmd_payload* payload = NULL;
    while (emnat_poll_command_buffer(commmand_buf, (void**)&payload)) {
        vulkan_queue_family needed_queue = command_queue_family(payload->type);
        if (!ctx->curr_submission || ctx->curr_submission->queue != needed_queue) {
            // Null handle allocates a new handle.
            VkCommandBuffer command_buffer = VK_NULL_HANDLE;

            // The guranteed lifetime of a command buffer is the point of last command buffer
            // that uses its resources in the submission, past that it can't be used so check for whetever it has any dependecies.
            if (darray_length(ctx->curr_submission->edges) == 0)
                command_buffer = context->modes[needed_queue].commandbufs[device->current_frame];

            if (ctx->curr_submission)
                end_submission(ctx, ctx->curr_submission);
            new_submission(ctx, needed_queue, command_buffer, &ctx->curr_submission);
        }

        switch (payload->type) {
            case COMMAND_BEGIN_COMPUTEPASS:
                cmd_bind_pipeline(ctx, &payload->begin_computepass);
                break;

            case COMMAND_DISPATCH:
                vkCmdDispatch(ctx->curr_submission->handle, 
                        payload->dispatch.group_size.x, 
                        payload->dispatch.group_size.y,
                        payload->dispatch.group_size.z);
                break;

            case COMMAND_END_COMPUTEPASS:
                release_resources(ctx);
                break;

            case COMMAND_BEGIN_RENDERPASS:
                cmd_begin_renderpass(ctx, &payload->begin_renderpass);
                break;
                
            case COMMAND_END_RENDERPASS:
                vkCmdEndRendering(ctx->curr_submission->handle);
                
                if (ctx->bound_pipeline)
                    release_resources(ctx);
                ctx->bound_pipeline = EMFALSE;

                release_resources(ctx);
                break;

            case COMMAND_SET_VIEWPORT:
                ;
                VkViewport viewport = {};
                viewport.x      = (f32)payload->set_viewport.origin.x;
                viewport.y      = (f32)(payload->set_viewport.origin.y + payload->set_viewport.size.y);
                viewport.width  = (f32)payload->set_viewport.size.x;
                viewport.height = -(f32)payload->set_viewport.size.y;
                viewport.minDepth = payload->set_viewport.min_depth;
                viewport.maxDepth = payload->set_viewport.max_depth;
                vkCmdSetViewport(ctx->curr_submission->handle, 0, 1, &viewport);
                break;

            case COMMAND_SET_SCISSOR:
                ;
                VkRect2D scissor = {};
                scissor.offset.x = payload->set_scissor.origin.x;
                scissor.offset.y = payload->set_scissor.origin.y;
                scissor.extent.width  = payload->set_scissor.size.x;
                scissor.extent.height = payload->set_scissor.size.y;
                vkCmdSetScissor(ctx->curr_submission->handle, 0, 1, &scissor);
                break;

            case COMMAND_BIND_RASTER_PIPELINE:
                if (ctx->bound_pipeline)
                    release_resources(ctx);

                cmd_bind_pipeline(ctx, &payload->bind_raster_pipeline);
                break;

            case COMMAND_BIND_VERTEX_BUFFERS:
                cmd_bind_vertex_buffers(ctx, 
                        payload->bind_vertex_buffers.count, 
                        payload->bind_vertex_buffers.buffers);
                break;

            case COMMAND_BIND_INDEX_BUFFER:
                ;
                vulkan_buffer* buffer = 
                    (vulkan_buffer*)payload->bind_index_buffer->internal_data;

                vkCmdBindIndexBuffer(ctx->curr_submission->handle, buffer->handle, 0, VK_INDEX_TYPE_UINT16);
                break;

            case COMMAND_DRAW:
                vkCmdDraw(ctx->curr_submission->handle, 
                        payload->draw.vertex_count, 
                        payload->draw.instance_count, 
                        0, 0);
                break;

            case COMMAND_EMPTY_RESOURCE:
                ;
                managed_resource empty = {};
                empty.type   = MANAGED_RESOURCE_EMPTY;
                empty.access = EMBER_ACCESS_NONE;
                empty.queue  = VULKAN_QUEUE_FAMILY_UNIVERSAL;

                insert_resource(ctx, payload->empty_resource, &empty);
                break;

            case COMMAND_IMPORT_TEXTURE:
                ;
                managed_resource texture = {};
                texture.type   = MANAGED_RESOURCE_TEXTURE;
                texture.access = EMBER_ACCESS_NONE;
                texture.queue  = VULKAN_QUEUE_FAMILY_UNIVERSAL;

                texture.data.texture = payload->import_texture.texture;
                insert_resource(ctx, payload->import_texture.dst_framebuffer, &texture);
                break;

            case COMMAND_ACQUIRE_SURFACE:
                ;
                vulkan_surface* vk_surface = 
                    add_surface(ctx, payload->acquire_surface.surface)->internal_data;

                owner_desc binary_owner = {};
                binary_owner.type = COMMAND_OWNER_BINARY;
                binary_owner.binary = vk_surface->image_availables[vk_surface->image_index];
                declare_owner(ctx, &binary_owner);

                managed_resource frame_in_flight = {};
                frame_in_flight.type   = MANAGED_RESOURCE_BUFFER;
                frame_in_flight.access = EMBER_ACCESS_NONE;
                frame_in_flight.queue  = VULKAN_QUEUE_FAMILY_UNIVERSAL;

                frame_in_flight.data.texture = &vk_surface->frames_in_flight[vk_surface->image_index];
                insert_resource(ctx, payload->acquire_surface.dst_framebuffer, &frame_in_flight);

                release_resources(ctx);
                break;
        }

    }

    if (ctx->curr_submission)
        end_submission(ctx, ctx->curr_submission);
    return EMBER_RESULT_OK;
}
