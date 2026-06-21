#include "defines.h"
#include "utils/darray.h"

void* _darray_create(u64 stride, u32 capacity, em_allocator* allocator, memory_tag memtag) {
    u64 size = stride * capacity;
    darray_header* new_array = mem_allocate(allocator, sizeof(darray_header) + size, memtag);
    new_array->capacity = capacity;
    new_array->length = 0;
    new_array->memtag = memtag;
    new_array->stride = stride;
    new_array->allocator = allocator;
    return memset(new_array + 1, 0, size);
}

void* _darry_from_data(u64 stride, u32 length, const void* from_data, em_allocator* allocator, memory_tag memtag) {
    void* new_array = _darray_create(stride, length, allocator, memtag);
    _darray_header(new_array)->length = length;

    if (from_data)
        memcpy(new_array, from_data, stride * length);
    return new_array;
}

void darray_destroy(void* array) {
    darray_header* hdr = _darray_header(array);
    mem_free(hdr->allocator, hdr, sizeof(darray_header) + hdr->stride * hdr->capacity, hdr->memtag);
}

void* _darray_push(void** out_array, const void* value_ptr) {
    void* arr = *out_array;

    // Resize if needed
    u64 old_capacity = darray_capacity(arr);
    if (darray_length(arr) + 1 > old_capacity) {
        u64 new_capacity = (old_capacity ? old_capacity * DARRAY_RESIZE_FACTOR : DARRAY_DEFAULT_CAPACITY);
        *out_array = darray_resize(arr, new_capacity);
    }

    darray_header* hdr = _darray_header(*out_array);

    // Write new element
    void* addr = (u8*)(*out_array) + (hdr->length * hdr->stride);
    if (value_ptr)
        memcpy(addr, value_ptr, hdr->stride);

    hdr->length += 1;
    return addr;
}

void _darray_concat(void** out_array, u32 src_size, void* src_array) {
    void* arr = *out_array;

    // Resize if needed
    u64 old_capacity = darray_capacity(arr);
    if (darray_length(arr) + src_size > old_capacity) {
        *out_array = darray_resize(arr, darray_length(arr) + src_size);
    }

    darray_header* hdr = _darray_header(*out_array);

    void* addr = (u8*)(*out_array) + (hdr->length * hdr->stride);
    memcpy(addr, src_array, hdr->stride * src_size);
    hdr->length += src_size;
}

void darray_pop_at(void* array, u32 index, void* dest) {
    darray_header* hdr = _darray_header(array);

    u8* addr = (u8*)array + index * hdr->stride;
    if (dest) memcpy(dest, (void*)addr, hdr->stride);

    if (index != hdr->length - 1) {
        memcpy(
            addr,
            addr + hdr->stride,
            hdr->stride * (hdr->length - index));
    }

    hdr->length -= 1;
}

darray_header* _darray_header(void* array) {
    return ((darray_header*)array) - 1;
}

void* darray_resize(void* array, u32 new_size) {
    darray_header* hdr = _darray_header(array);
    void* new_array = _darray_create(hdr->stride, new_size, hdr->allocator, hdr->memtag);
    darray_length(new_array) = new_size < hdr->length ? new_size : hdr->length; // = min()

    memcpy(new_array, array, darray_length(new_array) * hdr->stride);
    darray_destroy(array);
    return new_array;
}