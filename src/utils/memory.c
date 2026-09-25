#include "defines.h"

u64 alignment_ptr(u64 v, u64 alignment) {
    return (v + (alignment - 1)) & ~(alignment - 1);
}

void* mem_allocate(em_allocator* allocator, u64 size) {
	return allocator->alloc(allocator, size, 0);
}

void mem_free(em_allocator* allocator, void* block, u64 size) {
	allocator->free(allocator, block, size, 0);
}

void* mem_reallocate(em_allocator* allocator, void* block, u64 old_size, u64 new_size) {
	if (!allocator->realloc) {
		mem_free(allocator, block, old_size);
		return mem_allocate(allocator, new_size);
	}

    if (!block) {
        return mem_allocate(allocator, new_size);
    }

    if (new_size == 0) {
        mem_free(allocator, block, old_size);
        return NULL;
    }

	return allocator->realloc(allocator, block, old_size, new_size, 0);
}
