#pragma once

#include "defines.h"

typedef struct darray_header {
    memory_tag memtag;
    em_allocator* allocator;
    u32 capacity, length;
    u64 stride;
} darray_header;

#define DARRAY_DEFAULT_CAPACITY 1
#define DARRAY_RESIZE_FACTOR 2

void*          _darray_create(u64 stride, u32 capacity, em_allocator* allocator, memory_tag memtag);
void*          _darry_from_data(u64 stride, u32 length, const void* from_data, em_allocator* allocator, memory_tag memtag);
void*          _darray_push(void** out_array, const void* value_ptr);
void           _darray_concat(void** array, u32 src_size, void* src_array);

darray_header* _darray_header(void* array);

void*          darray_resize(void* array, u32 new_size);
void           darray_destroy(void* array);
void           darray_pop_at(void* array, u32 index, void* dest);

#define darray_create(type, allocator, memtag) \
    _darray_create(sizeof(type), DARRAY_DEFAULT_CAPACITY, allocator, memtag)

#define darray_from_data(type, length, from_data, allocator, memtag) \
    _darry_from_data(sizeof(type), length, from_data, allocator, memtag)

#define darray_reserve(type, capacity, allocator, memtag) \
     _darray_create(sizeof(type), capacity, allocator, memtag)

#define darray_push(array, value)          \
    do {                                   \
        __typeof__(*array) _tmp = (value); \
        _darray_push((void**) &(array), &_tmp); \
    } while (0)

#define darray_push_empty(array) \
    _darray_push((void**) &(array), NULL)

#define darray_concat(array, src_size, src_array) \
    _darray_concat((void**) &(array), src_size, src_array)

#define darray_clear(array) \
    _darray_header(array)->length = 0

#define darray_length(array) (_darray_header(array)->length)

#define darray_capacity(array) (_darray_header(array)->capacity)

#define darray_stride(array) (_darray_header(array)->stride)

#define darray_last(array) (&array[darray_length(array) - 1])