#include "defines.h"
#include "ember/gpu/resources.h"
#include "vk_types.h"

#include "utils/darray.h"

#include "gpu/command_decoder.h"

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
 * !NOTE Owner-wide data in resource system.
 */

// Declears a new stack frame as a owner of the following calls to `own_resource`.
// This is how the system knows where data is being transported and then to insert dependency edges.
// Resources acquired or 'owned' after this will be handed back at `release_resources` and all sync operations
// will be operated. The pair of declaring a owner and release resources acts like a code stack.
void declare_owner(vulkan_command_context* ctx /** ... **/) {

}

// This inserts a resource into the system based on the `dst_handle` and the currently
// set owner. The managed resource handle may refer to any buffer, texture, or framebuffer.
// The `curr_access` variable refers to the owner own access with the resource, this is used in pipeline barriers.
void insert_resource(vulkan_command_context* ctx, managed_resource* resource, emgpu_local_resource dst_handle, emgpu_access_flags curr_access) {

}

// This uses the given handle in the submit-wide table to give back an actual Vulkan
// handle. It will perform the nessacery actions to do so in this exact function.
managed_resource* own_resource(vulkan_command_context* ctx, emgpu_local_resource handle, emgpu_access_flags needed_access) {

}

// Pops an owner frame from the submit-stack, releases ownership of resources and
// emits all nessacery dst Vulkan sync opertions.
void release_resources(vulkan_command_context* ctx) {

}

// Adds the provided surface to the submit-wide list, this implictly means that a surface's
// framebuffer is being used this frame so the system will automattically present the surface
// after submit the command buffers.
void add_surface(const emgpu_surface* surface) {

}

em_result emgpu_device_submit(emgpu_device* device, emgpu_queue queue, const emgpu_command_buffer* commmand_buf) {
    vulkan_command_context ctx = {};

    cmd_payload* payload = NULL;
    while (vulkan_poll_command_buffer(commmand_buf, (u8**) &payload) == EMBER_RESULT_OK) {
        switch (payload->type) {
            case COMMAND_BEGIN_COMPUTEPASS: {
                emgpu_ops_type new_ops = EMBER_OPER_TYPE_COMPUTE;
                if (ctx.curr_submission.ops_type != new_ops) {
                    if (ctx.curr_mode_commandbuff[new_ops].submitted)
                        new_command_buffer(&device->frame_allocator, new_ops, &ctx.curr_submission);
                    else
                        ctx.curr_submission = ctx.curr_mode_commandbuf[new_ops];
                }

                // Resolve dependencys here.
                for (u32 i = 0; i < payload->begin_computepass.import_resource_count; ++i) {
                    const emgpu_resource_import* import = &payload->begin_computepass.import_resources[i];
                    own_resource(&ctx, import->resource, import->access_flags);
                }
                
                // bind compute pipeline.
                const vulkan_pipeline* comp_pipeline = (const vulkan_pipeline*)payload->begin_computepass.pipeline->internal_data;
                for (u32 i = 0; i < payload->begin_computepass.export_resource_count; ++i) {
                    const emgpu_resource_export* export = &payload->begin_computepass.export_resources[i];

                    managed_resource resource = {};
                    resource.raw = (void*) comp_pipeline.descriptors[export->src_binding];
                    insert_resource(&ctx, &resource, export->resource, export->access_flags);
                }

                vkCmdBindPipeline(ctx.curr_submission.handle, VK_PIPELINE_BIND_POINT_COMPUTE, comp_pipeline->handle);

                // TODO: Descriptors.
                break;
            }

            case COMMAND_DISPATCH: {
                vkCmdDispatch(ctx.curr_submission.handle, payload->dispatch.group_size.x, payload->dispatch.group_size.y, payload->dispatch.group_size.z);
                break;
            }

            case COMMAND_END_COMPUTEPASS: {
                release_resources(&ctx);
                break;
            }

            case COMMAND_BEGIN_RENDERPASS: {
                emgpu_ops_type new_ops = EMBER_OPER_TYPE_RASTER;
                if (ctx.curr_submission.ops_type != new_ops) {
                    if (ctx.curr_mode_commandbuff[new_ops].submitted)
                        new_command_buffer(&device->frame_allocator, new_ops, &ctx.curr_submission);
                    else
                        ctx.curr_submission = ctx.curr_mode_commandbuf[new_ops];
                }

                VkRenderingInfo rendering_info = { VK_STRUCTURE_TYPE_RENDERING_INFO };
                rendering_info.renderArea.offset = (VkOffset2D) { payload->begin_renderpass.render_origin.x, payload->begin_renderpass.render_origin.y };
                rendering_info.renderArea.extent = (VkExtent2D) { payload->begin_renderpass.render_size.x, payload->begin_renderpass.render_size.y };
                rendering_info.layerCount = 1;
                
                VkRenderingAttachmentInfo* colour_attachments = darray_reserve(VkRenderingAttachmentInfo, payload->begin_renderpass.colour_attachment_count, &device->frame_allocator);
                
                for (u32 i = 0; i < payload->begin_renderpass.colour_attachment_count; ++i) {
                    const emgpu_colour_attachment* attachment = &payload->begin_renderpass.colour_attachments[i];
                    
                    VkRenderingAttachmentInfo* vk_attachment = darray_push_empty(colour_attachments);
                    vk_attachment->sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
                    
                    managed_resource* framebuffer = own_resource(&ctx, attachment->framebuffer, EMBER_ACCESS_COLOUR_ATTACHMENT_WRITE);
                    vk_attachment->imageView = framebuffer->view;
                    //vk_attachment->imageLayout = framebuffer->layout;
                    vk_attachment->loadOp = vulkan_load_op_type(attachment->load_op);
                    vk_attachment->storeOp = vulkan_store_op_type(attachment->store_op);
                    vk_attachment->clearValue.color.float32[0] = ((attachment->clear_colour >> 24) & 0xFF) / 255.0f;
                    vk_attachment->clearValue.color.float32[1] = ((attachment->clear_colour >> 16) & 0xFF) / 255.0f;
                    vk_attachment->clearValue.color.float32[2] = ((attachment->clear_colour >> 8)  & 0xFF) / 255.0f;
                    vk_attachment->clearValue.color.float32[3] = ((attachment->clear_colour)       & 0xFF) / 255.0f;
                }

                rendering_info.colorAttachmentCount = darray_length(colour_attachments);
                rendering_info.pColorAttachments    = colour_attachments;

                vkCmdBeginRendering(ctx.curr_submission.handle, &rendering_info);
                
                darray_destroy(colour_attachments);
                break;
            }

            case COMMAND_END_RENDERPASS: {
                vkCmdEndRendering(ctx.curr_submission.handle);
                release_resources(&ctx);
                break;
            }

            case COMMAND_SET_VIEWPORT: {
                VkViewport viewport = {};
                viewport.x = (f32)payload->set_viewport.origin.x;
                viewport.y = (f32)(payload->set_viewport.origin.y + payload->set_viewport.size.y);
                viewport.width  =  (f32)payload->set_viewport.size.x;
                viewport.height = -(f32)payload->set_viewport.size.y;
                viewport.minDepth = payload->set_viewport.min_depth;
                viewport.maxDepth = payload->set_viewport.max_depth;
                vkCmdSetViewport(ctx.curr_submission.handle, 0, 1, &viewport);
                break;
            }

            case COMMAND_SET_SCISSOR: {
                VkRect2D scissor = {};
                scissor.offset.x = payload->set_scissor.origin.x;
                scissor.offset.y = payload->set_scissor.origin.y;
                scissor.extent.width  = payload->set_scissor.size.x;
                scissor.extent.height = payload->set_scissor.size.y;
                vkCmdSetScissor(ctx.curr_submission.handle, 0, 1, &scissor);
                break;
            }

            case COMMAND_BIND_RASTER_PIPELINE: {
                const vulkan_pipeline* raster_pipeline = (const vulkan_pipeline*)payload->bind_raster_pipeline.pipeline->internal_data;
                vkCmdBindPipeline(ctx.curr_submission.handle, VK_PIPELINE_BIND_POINT_GRAPHICS, raster_pipeline->handle);
                ctx.curr_pipeline = payload->bind_raster_pipeline.pipeline;

                // TODO: Descriptors.

                break;
            }

            case COMMAND_BIND_VERTEX_BUFFERS: {
                if (payload->bind_vertex_buffers.count == 1) {
                    vulkan_buffer* buffer = (vulkan_buffer*)payload->bind_vertex_buffers.buffers->internal_data;
                    VkDeviceSize offset = 0;
                    vkCmdBindVertexBuffers(ctx.curr_submission.handle, 0, 1, &buffer->handle, &offset);
                }
                else {

                }
                break;
            }

            case COMMAND_BIND_INDEX_BUFFER: {
                vulkan_buffer* buffer = (vulkan_buffer*)payload->bind_index_buffer->internal_data;
                vkCmdBindIndexBuffer(ctx.curr_submission.handle, buffer->handle, 0, VK_INDEX_TYPE_UINT16);
                break;
            }

            case COMMAND_DRAW: {
                vkCmdDraw(ctx.curr_submission.handle, payload->draw.vertex_count, payload->draw.instance_count, 0, 0);
                break;
            }
            
            case COMMAND_EMPTY_RESOURCE: {
                managed_resource empty = {};
                insert_resource(&ctx, &empty, payload->empty_resource, EMBER_ACCESS_NONE);
                break;
            }

            case COMMAND_IMPORT_TEXTURE: {
                managed_resource framebuffer = {};
                framebuffer.raw = (void*)payload->import_texture.texture;
                insert_resource(&ctx, &framebuffer, payload->import_texture.dst_framebuffer, EMBER_ACCESS_NONE);
                break;
            }

            case COMMAND_ACQUIRE_SURFACE: {
                const vulkan_surface* vk_surface = (const vulkan_surface*)payload->acquire_surface.surface;
                add_surface(payload->acquire_surface.surface);

                declare_owner(&ctx, BINARY_SEMAPHORE, vk_surface->image_availables[vk_surface->image_index]);

                managed_resource framebuffer = {};
                framebuffer.raw = (void*) &vk_surface->frames_in_flight[vk_surface->image_index];
                insert_resource(&ctx, &framebuffer, payload->acquire_surface.dst_framebuffer, EMBER_ACCESS_NONE);

                release_resources(&ctx);
                break;
            }
        }
    }
}
