#include "defines.h"
#include "command_decoder.h"

u64 align_command(u64 value, u64 alignment) {
    // alignment must be a power of two
    return (value + alignment - 1) & ~(alignment - 1);
}

b8 emnat_poll_command_buffer(
    const emgpu_command_buffer* command_buf,
    cmd_payload* out_cursor)
{
    u8* base = (u8*)command_buf->commands_buf;
    u64 size = command_buf->buffer_size;
    u64 offset = 0;

    if (out_cursor->raw_ptr != NULL) {
        /* raw_ptr points at the payload; header is immediately before it. */
        u64 current =
            (u64)((u8*)out_cursor->raw_ptr - base)
            - sizeof(cmd_header);

        offset = align_command(current + out_cursor->size, 16);
    }

    if (offset > size || size - offset < sizeof(cmd_header))
        return EMFALSE;

    cmd_header* hdr = (cmd_header*)(base + offset);

    if (hdr->size < sizeof(cmd_header) ||
        hdr->size > size - offset)
        return EMFALSE;

    out_cursor->raw_ptr =
        base + offset + sizeof(cmd_header);

    out_cursor->type = hdr->type;
    out_cursor->size = hdr->size;

    return EMTRUE;
}
