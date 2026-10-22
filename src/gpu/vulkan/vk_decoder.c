#include "defines.h"
#include "vk_types.h"

#include "utils/darray.h"

// Declears a new stack frame as a owner of the following calls to `own_resource`.
// This is how the system knows where data is being transported and then to insert dependency edges.
// Resources acquired or 'owned' after this will be handed back at `release_resources` and all sync operations
// will be operated. The pair of declaring a owner and release resources acts like a code stack.
void declare_owner(vulkan_command_context* ctx, owner_desc* owner) {
    owner_frame* frame = darray_push_empty(ctx->stack);
    frame->desc = *owner;
    frame->uses = darray_create(resource_use, ctx->allocator);
}

// This inserts a resource into the system based on the `dst_handle` and the currently
// set owner. The managed resource handle may refer to any buffer, texture, or framebuffer.
// The `curr_access` variable refers to the owner own access with the resource, this is used in pipeline barriers.
void insert_resource(vulkan_command_context* ctx, emgpu_local_resource dst_handle, managed_resource* resource) {
    ctx->resource_table[(u32)dst_handle] = *resource;
    ctx->resource_table[(u32)dst_handle].state.owner = darray_last(ctx->stack)->desc;
}

// Adds the provided surface to the submit-wide list, this implictly means that a surface's
// framebuffer is being used this frame so the system will automattically present the surface
// after submit the command buffers.
emgpu_surface* add_surface(vulkan_command_context* ctx, emgpu_local_resource used_resource, emgpu_surface* surface) {
    vulkan_command_surface* s = darray_push_empty(ctx->surfaces);
    s->surface = surface;
    s->resource = used_resource;
    return surface;
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
    use->state.owner = owner->desc;
    
    managed_resource* resc = &ctx->resource_table[(u32)handle];

    vulkan_texture* texture = (vulkan_texture*)resc->data.texture->internal_data;
    if (ctx->resource_table[(u32)handle].type == MANAGED_RESOURCE_TEXTURE && EMTRUE) {
        // !vulkan_layout_compliant(texture->layout, vulkan_image_layout(needed_access))) {

        VkImageMemoryBarrier2 image_barrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2 };
        image_barrier.srcStageMask = vulkan_wait_stage(resc->state.access);
        image_barrier.srcAccessMask = vulkan_access_flags(resc->state.access); 
        image_barrier.dstStageMask = vulkan_wait_stage(needed_access);
        image_barrier.dstAccessMask = vulkan_access_flags(needed_access);

        image_barrier.oldLayout = texture->layout;
        image_barrier.newLayout = vulkan_image_layout(needed_access);

        image_barrier.image = texture->handle;
        image_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        image_barrier.subresourceRange.layerCount = 1;
        image_barrier.subresourceRange.levelCount = 1;

        image_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        image_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

        VkDependencyInfo dependency_info = { VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
        dependency_info.imageMemoryBarrierCount = 1;
        dependency_info.pImageMemoryBarriers = &image_barrier;
        vkCmdPipelineBarrier2(ctx->curr_submission->handle, 
                &dependency_info);

        texture->layout = image_barrier.newLayout;
        resc->state.texture_layout = image_barrier.newLayout;
    }

    return resc;
}

// Pops an owner frame from the submit-stack, releases ownership of resources and
// emits all nessacery dst Vulkan sync opertions. If there is no stack frames just return.
void release_resources(vulkan_command_context* ctx) {
    if (darray_length(ctx->stack) == 0) return;

    owner_frame* owner = darray_last(ctx->stack);

    for (u32 i = 0; i < darray_length(owner->uses); ++i) {
        // TODO: Resolve resource transititons.
        resource_use* use = &owner->uses[i];

        managed_resource* resource = &ctx->resource_table[(u32)use->resource];
        resource_state prev = resource->state;
        resource_state next = use->state;

        if (prev.submission_index == next.submission_index) {
            if (resource->type == MANAGED_RESOURCE_TEXTURE) {
                vulkan_texture* vk_texture = (vulkan_texture*)resource->data.texture->internal_data;

                VkImageMemoryBarrier2 image_barrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2 };
                image_barrier.srcStageMask = vulkan_wait_stage(prev.access);
                image_barrier.srcAccessMask = vulkan_access_flags(prev.access); 
                image_barrier.dstStageMask = vulkan_wait_stage(next.access);
                image_barrier.dstAccessMask = vulkan_access_flags(next.access);

                image_barrier.oldLayout = vk_texture->layout;
                image_barrier.newLayout = vulkan_image_layout(next.access);

                image_barrier.image = vk_texture->handle;
                image_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                image_barrier.subresourceRange.layerCount = 1;
                image_barrier.subresourceRange.levelCount = 1;

                image_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                image_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

                VkDependencyInfo dependency_info = { VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
                dependency_info.imageMemoryBarrierCount = 1;
                dependency_info.pImageMemoryBarriers = &image_barrier;
                vkCmdPipelineBarrier2(ctx->curr_submission->handle, 
                        &dependency_info);
            }
            else if (resource->type == MANAGED_RESOURCE_BUFFER) {
                vulkan_buffer* vk_buffer= (vulkan_buffer*)resource->data.buffer;

                VkBufferMemoryBarrier2 buffer_barrier = { VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2 };
                buffer_barrier.srcStageMask = vulkan_wait_stage(prev.access);
                buffer_barrier.srcAccessMask = vulkan_access_flags(prev.access); 
                buffer_barrier.dstStageMask = vulkan_wait_stage(next.access);
                buffer_barrier.dstAccessMask = vulkan_access_flags(next.access);

                buffer_barrier.buffer = vk_buffer->handle;

                buffer_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                buffer_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

                VkDependencyInfo dependency_info = { VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
                dependency_info.bufferMemoryBarrierCount = 1;
                dependency_info.pBufferMemoryBarriers = &buffer_barrier;
                vkCmdPipelineBarrier2(ctx->curr_submission->handle, 
                        &dependency_info);
            }
        }

        if (prev.owner.binary != NULL) {
            VkSemaphoreSubmitInfo* semaphore_info = darray_push_empty(ctx->curr_submission->waits);
            semaphore_info->sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
            semaphore_info->semaphore = prev.owner.binary;
            semaphore_info->stageMask = vulkan_wait_stage(next.access);
        }


        if (next.owner.binary != NULL) {
            VkSemaphoreSubmitInfo* semaphore_info = darray_push_empty(ctx->curr_submission->signals);
            semaphore_info->sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
            semaphore_info->semaphore = next.owner.binary;
            semaphore_info->stageMask = vulkan_wait_stage(next.access);
        }


        resource->state = next;
    }

    darray_pop(ctx->stack, NULL);
}

// Turns a Vulkan command buffer into a proper submission struct. If there is no
// command buffer provided it creates a temporary one.
static em_result new_submission(vulkan_command_context* ctx, vulkan_queue_family family, VkCommandBuffer command_buffer, vulkan_command_submission** out_submission) {
    if (!command_buffer) return EMBER_RESULT_UNIMPLEMENTED;

    vulkan_command_submission* new_submission = darray_push_empty(ctx->submissions);
    new_submission->handle = command_buffer;
    new_submission->queue = family;
    new_submission->waits = darray_create(VkSemaphoreSubmitInfo, ctx->allocator);
    new_submission->signals = darray_create(VkSemaphoreSubmitInfo, ctx->allocator);

    VkCommandBufferBeginInfo begin_info = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    CHECK_VKRESULT(
        vkBeginCommandBuffer(new_submission->handle, &begin_info), 
        "Failed to begin command buffer");

    *out_submission = new_submission;
    return EMBER_RESULT_OK;
}

static void end_submission(vulkan_command_context* ctx, vulkan_command_submission* submission) {
    for (u32 i = 0; i < darray_length(ctx->surfaces); ++i) {
        vulkan_surface* vk_surface = (vulkan_surface*)ctx->surfaces[i].surface->internal_data;

        owner_desc owner = {};
        owner.type   = COMMAND_OWNER_BINARY;
        owner.binary = vk_surface->render_completes[vk_surface->image_index];
        declare_owner(ctx, &owner);

        own_resource(ctx, ctx->surfaces[i].resource, EMNAT_VULKAN_ACCESS_PRESENT_SRC);

        release_resources(ctx);
    }
    darray_clear(ctx->surfaces);

    vkEndCommandBuffer(submission->handle);
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


static void begin_renderpass(vulkan_command_context* ctx, cmd_payload* payload) {
    owner_desc renderpass_owner = {};
    renderpass_owner.type = COMMAND_OWNER_RENDERPASS;
    declare_owner(ctx, &renderpass_owner);

    VkRenderingInfo rendering_info = { VK_STRUCTURE_TYPE_RENDERING_INFO };
    rendering_info.renderArea.offset = (VkOffset2D) { payload->begin_renderpass->render_origin.x, payload->begin_renderpass->render_origin.y };
    rendering_info.renderArea.extent = (VkExtent2D) { payload->begin_renderpass->render_size.x, payload->begin_renderpass->render_size.y };
    rendering_info.layerCount = 1;

    // Recv colour attachments
    emnat_poll_command_buffer(ctx->buffer, ctx->buffer_size, payload);

    u32 colour_attachment_count = payload->size / sizeof(emgpu_colour_attachment);
    VkRenderingAttachmentInfo* colour_attachments = darray_reserve(VkRenderingAttachmentInfo, colour_attachment_count, ctx->allocator);

    for (u32 i = 0; i < colour_attachment_count; ++i) {
        const emgpu_colour_attachment* attachment = &payload->colour_attachments[i];

        VkRenderingAttachmentInfo* vk_attachment = darray_push_empty(colour_attachments);
        vk_attachment->sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;

        managed_resource* resc = own_resource(ctx, attachment->framebuffer, EMBER_ACCESS_COLOUR_ATTACHMENT_WRITE);
        vulkan_texture* vk_framebuffer = (vulkan_texture*)resc->data.texture->internal_data;
        vk_attachment->imageView = vk_framebuffer->view;
        vk_attachment->imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
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

static void bind_pipeline(vulkan_command_context* ctx, cmd_payload* payload) {
    owner_desc pipeline_owner = {};
    pipeline_owner.type = COMMAND_OWNER_PIPELINE;
    pipeline_owner.pipeline = payload->bind_pipeline->pipeline;
    declare_owner(ctx, &pipeline_owner);

    vulkan_pipeline* vk_pipeline = (vulkan_pipeline*)pipeline_owner.pipeline->internal_data;

    // Recv import resources
    emnat_poll_command_buffer(ctx->buffer, ctx->buffer_size, payload);

    const emgpu_resource_import* import = payload->imports_resources;
    for (u32 i = 0; i < payload->size / sizeof(*import); ++i, ++import)
        own_resource(ctx, import->resource, import->access_flags);

    // Recv export resources.
    emnat_poll_command_buffer(ctx->buffer, ctx->buffer_size, payload);
    
    /* WORK IN PROGRESS!
    const emgpu_resource_export* export = payload->export_resources;
    for (u32 i = 0; i < payload->size / sizeof(*export); ++i, ++export) {
        managed_resource resc = {};
        resc.state.access = export->access_flags;
        insert_resource(ctx, export->resource, &resc);
    }
    */

    ctx->bound_pipeline = EMTRUE;

    vkCmdBindPipeline(ctx->curr_submission->handle, 
            vulkan_bind_point(pipeline_owner.pipeline->type),
            vk_pipeline->handle);
}

em_result vulkan_decode_command_buffer(emgpu_device* device, vulkan_command_context* ctx) {
    vulkan_device* vk_device = (vulkan_device*)device->internal_context;

    cmd_payload payload = {};
    while (emnat_poll_command_buffer(ctx->buffer, ctx->buffer_size, &payload)) {
        vulkan_queue_family needed_queue = command_queue_family(payload.type);
        if ((!ctx->curr_submission || ctx->curr_submission->queue != needed_queue) && needed_queue != VULKAN_QUEUE_FAMILY_UNIVERSAL) {
            // Null handle allocates a new handle.
            VkCommandBuffer command_buffer = VK_NULL_HANDLE;

            // The guranteed lifetime of a command buffer is the point of last command buffer
            // that uses its resources in the submission, past that it can't be used so check for whetever it has any dependecies.
            if (!ctx->curr_submission || ctx->curr_submission->edge_count == 0)
                command_buffer = vk_device->modes[needed_queue].commandbufs[device->current_frame];

            if (ctx->curr_submission)
                end_submission(ctx, ctx->curr_submission);
            em_result result = new_submission(ctx, needed_queue, command_buffer, &ctx->curr_submission);
            if (result != EMBER_RESULT_OK) return result;
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
                empty.type         = MANAGED_RESOURCE_EMPTY;
                empty.state.access = EMBER_ACCESS_NONE;
                insert_resource(ctx, payload.empty_resource->dst_resource, &empty);
                break;

            case COMMAND_IMPORT_TEXTURE:
                ;

                managed_resource texture = {};
                texture.type         = MANAGED_RESOURCE_TEXTURE;
                texture.state.access = EMBER_ACCESS_NONE;
                texture.data.texture = payload.import_texture->texture;

                vulkan_texture* vk_texture = (vulkan_texture*)texture.data.texture->internal_data;
                texture.state.texture_layout = vk_texture->layout;
                insert_resource(ctx, payload.import_texture->dst_framebuffer, &texture);
                break;

            case COMMAND_ACQUIRE_SURFACE:
                ;
                emgpu_surface* surf = add_surface(ctx, payload.acquire_surface->dst_framebuffer, payload.acquire_surface->surface);
                vulkan_surface* vk_surface = surf->internal_data;

                VkSemaphore acquire_sem = vk_surface->image_availables[device->current_frame];

                CHECK_VKRESULT(vkAcquireNextImageKHR(vk_device->handle, vk_surface->swapchain,
                    UINT64_MAX, acquire_sem, VK_NULL_HANDLE, &vk_surface->image_index),
                    "Failed to acquire next surface texture");

                owner_desc swapchain_owner = {};
                swapchain_owner.type   = COMMAND_OWNER_BINARY;
                swapchain_owner.binary = acquire_sem;
                declare_owner(ctx, &swapchain_owner);

                managed_resource fif = {};
                fif.type = MANAGED_RESOURCE_TEXTURE;
                fif.state.access = EMBER_ACCESS_NONE;
                fif.data.texture = &vk_surface->frames_in_flight[vk_surface->image_index];

                fif.state.texture_layout = ((vulkan_texture*)fif.data.texture->internal_data)->layout;

                insert_resource(ctx, payload.acquire_surface->dst_framebuffer, &fif);
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
