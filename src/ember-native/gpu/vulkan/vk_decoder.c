#include "defines.h"
#include "gpu/command_decoder.h"
#include "vk_types.h"

#include "utils/darray.h"

/*
 * Allocate more command buffer if nessacary.
 *
 * Types of resource dependency:
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
    use->state.access = needed_access;
    use->state.submission_index = darray_length(ctx->submissions) - 1;
    use->state.texture_layout = vulkan_image_layout(needed_access);
    if (owner->desc.type == COMMAND_OWNER_BINARY) 
        use->state.binary_owner = owner->desc.binary;

    return &ctx->resource_table[(u32)handle]; 
}

// Pops an owner frame from the submit-stack, releases ownership of resources and
// emits all nessacery dst Vulkan sync opertions. If there is no stack frames just return.
void release_resources(vulkan_command_context* ctx) {
    if (darray_length(ctx->stack) == 0) return;

    owner_frame* owner = darray_last(ctx->stack);
    
    for (u32 i = 0; i < darray_length(owner->uses); ++i) {
        resource_use* use = &owner->uses[i];

        managed_resource* resource = &ctx->resource_table[(u32)use->resource];
        resource_state prev = resource->state;
        resource_state next = use->state;

        if (prev.submission_index == next.submission_index) {
            // Pipeline barrier.
        }
        else {
            ctx->curr_submission->edge_count++;
            
            if (prev.binary_owner != VK_NULL_HANDLE) {
                // Semaphores.
                VkSemaphoreSubmitInfo* semaphore_info = darray_push_empty(ctx->curr_submission->waits);
                semaphore_info->semaphore = prev.binary_owner;
                semaphore_info->stageMask = vulkan_wait_stage(next.access);
            }
            else {
                // Timeline break
                // TODO: Or ordering? (linked list submissions)
            }
        }
    }
}

// Adds the provided surface to the submit-wide list, this implictly means that a surface's
// framebuffer is being used this frame so the system will automattically present the surface
// after submit the command buffers.
emgpu_surface* add_surface(vulkan_command_context* ctx, emgpu_surface* surface) {
    darray_push(ctx->surfaces, surface);
    return *darray_last(ctx->surfaces);
}

static void bind_pipeline(vulkan_command_context* ctx, cmd_payload* payload) {
    owner_desc pipeline_owner = {};
    pipeline_owner.type = COMMAND_OWNER_PIPELINE;
    pipeline_owner.pipeline = payload->bind_pipeline->pipeline;
    declare_owner(ctx, &pipeline_owner);

    vulkan_pipeline* vk_pipeline = (vulkan_pipeline*)pipeline_owner.pipeline->internal_data;

    // Recv import resources
    emnat_poll_command_buffer(ctx->command_buf, payload);

    const emgpu_resource_import* import = payload->imports_resources;
    for (u32 i = 0; i < payload->size / sizeof(*import); ++i, ++import)
        own_resource(ctx, import->resource, import->access_flags);

    // Recv export resources.
    emnat_poll_command_buffer(ctx->command_buf, payload);

    //const emgpu_resource_export* export = info->exports;
    //for (u32 i = 0; i < info->export_count; ++i, ++export)
    //    insert_resource(ctx, export->resource, export->access_flags, (void*)vk_pipeline->descriptors[export->src_binding]);

    ctx->bound_pipeline = EMTRUE;

    vkCmdBindPipeline(ctx->curr_submission->handle, 
            vulkan_bind_point(pipeline_owner.pipeline->type),
            vk_pipeline->handle);
}

static void begin_renderpass(vulkan_command_context* ctx, cmd_payload* payload) {
    owner_desc renderpass_owner = {};
    renderpass_owner.type = COMMAND_OWNER_RENDERPASS;
    //renderpass_owner.renderpass = NULL;
    declare_owner(ctx, &renderpass_owner);

    VkRenderingInfo rendering_info = { VK_STRUCTURE_TYPE_RENDERING_INFO };
    rendering_info.renderArea.offset = (VkOffset2D) { payload->begin_renderpass->render_origin.x, payload->begin_renderpass->render_origin.y };
    rendering_info.renderArea.extent = (VkExtent2D) { payload->begin_renderpass->render_size.x, payload->begin_renderpass->render_size.y };
    rendering_info.layerCount = 1;

    // Recv colour attachments
    emnat_poll_command_buffer(ctx->command_buf, payload);

    u32 colour_attachment_count = payload->size / sizeof(emgpu_colour_attachment);
    VkRenderingAttachmentInfo* colour_attachments = darray_reserve(VkRenderingAttachmentInfo, colour_attachment_count, ctx->allocator);

    for (u32 i = 0; i < colour_attachment_count; ++i) {
        const emgpu_colour_attachment* attachment = &payload->colour_attachments[i];

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

static void bind_vertex_buffers(vulkan_command_context* ctx, cmd_payload* payload) {
    u32 vertex_buffer_count = payload->size / sizeof(*payload->bind_vertex_buffers);

    if (vertex_buffer_count == 1) {
        VkDeviceSize offset = 0;

        vulkan_buffer* buffer = (vulkan_buffer*)payload->bind_vertex_buffers[0]->internal_data;
        vkCmdBindVertexBuffers(ctx->curr_submission->handle, 0, 1, &buffer->handle, &offset);
        return;
    }
}

em_result vulkan_decode_command_buffer(emgpu_device* device, vulkan_command_context* ctx) {
    vulkan_device* vk_device = (vulkan_device*)device->internal_context;

    cmd_payload payload = {};
    while (emnat_poll_command_buffer(ctx->command_buf, &payload)) {
        EM_INFO("Vulkan", "Decodeded command: %i", (u32)payload.type);
        vulkan_queue_family needed_queue = command_queue_family(payload.type);
        if (!ctx->curr_submission || ctx->curr_submission->queue != needed_queue) {
            // Null handle allocates a new handle.
            VkCommandBuffer command_buffer = VK_NULL_HANDLE;

            // The guranteed lifetime of a command buffer is the point of last command buffer
            // that uses its resources in the submission, past that it can't be used so check for whetever it has any dependecies.
            if (ctx->curr_submission && ctx->curr_submission->edge_count == 0)
                command_buffer = vk_device->modes[needed_queue].commandbufs[device->current_frame];

            if (ctx->curr_submission)
                end_submission(ctx, ctx->curr_submission);
            new_submission(ctx, needed_queue, command_buffer, &ctx->curr_submission);
        }

        switch (payload.type) {
            case COMMAND_BEGIN_COMPUTEPASS:
                bind_pipeline(ctx, &payload);
                break;

            case COMMAND_DISPATCH:
                vkCmdDispatch(ctx->curr_submission->handle, 
                        payload.dispatch->group_size.x, 
                        payload.dispatch->group_size.y,
                        payload.dispatch->group_size.z);
                break;

            case COMMAND_END_COMPUTEPASS:
                release_resources(ctx);
                break;

            case COMMAND_BEGIN_RENDERPASS:
                begin_renderpass(ctx, &payload);
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
                viewport.x      = (f32)payload.set_viewport->origin.x;
                viewport.y      = (f32)(payload.set_viewport->origin.y + payload.set_viewport->size.y);
                viewport.width  = (f32)payload.set_viewport->size.x;
                viewport.height = -(f32)payload.set_viewport->size.y;
                viewport.minDepth = payload.set_viewport->min_depth;
                viewport.maxDepth = payload.set_viewport->max_depth;
                vkCmdSetViewport(ctx->curr_submission->handle, 0, 1, &viewport);
                break;

            case COMMAND_SET_SCISSOR:
                ;
                VkRect2D scissor = {};
                scissor.offset.x = payload.set_scissor->origin.x;
                scissor.offset.y = payload.set_scissor->origin.y;
                scissor.extent.width  = payload.set_scissor->size.x;
                scissor.extent.height = payload.set_scissor->size.y;
                vkCmdSetScissor(ctx->curr_submission->handle, 0, 1, &scissor);
                break;

            case COMMAND_BIND_RASTER_PIPELINE:
                if (ctx->bound_pipeline)
                    release_resources(ctx);

                bind_pipeline(ctx, &payload);
                break;

            case COMMAND_BIND_VERTEX_BUFFERS:
                bind_vertex_buffers(ctx, &payload);
                break;

            case COMMAND_BIND_INDEX_BUFFER:
                ;
                vulkan_buffer* buffer = 
                    (vulkan_buffer*)payload.bind_index_buffer->index_buffer->internal_data;

                vkCmdBindIndexBuffer(ctx->curr_submission->handle, buffer->handle, 0, VK_INDEX_TYPE_UINT16);
                break;

            case COMMAND_DRAW:
                vkCmdDraw(ctx->curr_submission->handle, 
                        payload.draw->vertex_count, 
                        payload.draw->instance_count, 
                        0, 0);
                break;

            case COMMAND_EMPTY_RESOURCE:
                ;
                managed_resource empty = {};
                empty.type   = MANAGED_RESOURCE_EMPTY;
                empty.state.access = EMBER_ACCESS_NONE;
                empty.state.submission_index = -1;

                insert_resource(ctx, payload.empty_resource->dst_resource, &empty);
                break;

            case COMMAND_IMPORT_TEXTURE:
                ;

                managed_resource texture = {};
                texture.type = MANAGED_RESOURCE_TEXTURE;
                texture.state.access           = EMBER_ACCESS_NONE;
                texture.state.submission_index = -1;
                texture.data.texture = payload.import_texture->texture;

                vulkan_texture* vk_texture = (vulkan_texture*)texture.data.texture->internal_data;
                texture.state.texture_layout = vk_texture->layout;
                insert_resource(ctx, payload.import_texture->dst_framebuffer, &texture);
                break;

            case COMMAND_ACQUIRE_SURFACE:
                ;
                vulkan_surface* vk_surface = 
                    add_surface(ctx, payload.acquire_surface->surface)->internal_data;

                owner_desc binary_owner = {};
                binary_owner.type = COMMAND_OWNER_BINARY;
                binary_owner.binary = vk_surface->image_availables[vk_surface->image_index];
                declare_owner(ctx, &binary_owner);

                managed_resource frame_in_flight = {};
                frame_in_flight.type = MANAGED_RESOURCE_BUFFER;
                frame_in_flight.state.access           = EMBER_ACCESS_NONE;
                frame_in_flight.state.submission_index = -1;
                frame_in_flight.data.texture = &vk_surface->frames_in_flight[vk_surface->image_index];

                vulkan_texture* vk_frame_in_flight = (vulkan_texture*)frame_in_flight.data.texture->internal_data;
                texture.state.texture_layout = vk_frame_in_flight->layout;
                insert_resource(ctx, payload.acquire_surface->dst_framebuffer, &frame_in_flight);

                release_resources(ctx);
                break;

            default:
                EM_ASSERT(EMFALSE && "Unreachable code in command decoder");
                break;
        }

    }

    if (ctx->curr_submission)
        end_submission(ctx, ctx->curr_submission);
    return EMBER_RESULT_OK;
}
