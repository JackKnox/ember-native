#include "defines.h"
#include "psx_types.h"

#include <ember/platform/system.h>

#include <unistd.h>
#include <time.h>
#include <dlfcn.h>

void* system_malloc(em_allocator* allocator, u64 size, u64 alignment, memory_tag tag) {
	mem_report(size, tag);

	if (!alignment) {
		return malloc(size);
	}

    if (alignment < sizeof(void*)) alignment = sizeof(void*);

    void* ptr = NULL;
    if (posix_memalign(&ptr, alignment, size) != 0) return NULL;
    return ptr;
}

void system_free(em_allocator* allocator, void* block, u64 size, u64 alignment, memory_tag tag) {
	mem_report_free(size, tag);

	if (!alignment) {
		free(block);
		return;
	}

    free(block);
}

void* system_realloc(em_allocator* allocator, void* block, u64 old_size, u64 new_size, u64 alignment, memory_tag tag) {
	mem_report_free(old_size, tag);
	mem_report(new_size, tag);

    if (!alignment) {
        return realloc(block, new_size);
    }

    if (alignment < sizeof(void*)) alignment = sizeof(void*);

    void* new_ptr = NULL;
    if (posix_memalign(&new_ptr, alignment, new_size) != 0) {
        return NULL;
    }

    // Copy old data (up to the smaller size)
    u64 copy_size = old_size < new_size ? old_size : new_size;
    memcpy(new_ptr, block, copy_size);

    free(block);
    return new_ptr;
}

em_allocator emplat_allocator_default() {
	em_allocator allocator = {};
	allocator.alloc = system_malloc;
	allocator.free = system_free;
	allocator.realloc = system_realloc;
	return allocator;
}

void emplat_sleep_ms(f64 ms) {
    struct timespec req;
    req.tv_sec  = (time_t)(ms / 1000.0);
    req.tv_nsec = (long)((ms - (req.tv_sec * 1000.0))
                       * 1000000.0);

    while (nanosleep(&req, NULL) != -1)
        continue;
}

void emplat_sleep_us(u64 us) {
    struct timespec req;
    req.tv_sec  = (time_t)(us / 1000000ULL);
    req.tv_nsec = (long)((us % 1000000ULL) * 1000ULL);

    while (nanosleep(&req, NULL) != -1)
        continue;
}

const char* emplat_system_get_env(const char* name) {
    return getenv(name);
}

em_result emplat_system_set_env(const char* name, const char* value) {
    setenv(name, value, EMTRUE);
    return EMBER_RESULT_OK; // TODO: Return errno value
}

u32 emplat_system_get_pid() {
    return getpid();
}

em_result emplat_system_execute(const char* command) {
    
}

emplat_library emplat_system_library_load(const char* filepath) {
    return dlopen(filepath, RTLD_LAZY | RTLD_LOCAL);
}

void* emplat_system_library_symbol(emplat_library lib, const char* name) {
    return dlsym(lib, name);
}

void emplat_system_library_unload(emplat_library lib) {
	dlclose(lib);
}

u32 emplat_get_cpu_cores() {
    
}

u32 emplat_system_cache_line_size() {
    
}

u32 emplat_system_ram() {
    
}

const char* emplat_get_systemfolder(emplat_system_folder folder) {
    
}

emplat_time emplat_system_now() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);

    return (emplat_time)ts.tv_sec +
           (emplat_time)ts.tv_nsec * 1e-9;
}

u64 emplat_system_now_ns() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);

    return ((u64)ts.tv_sec * 1000000000ULL) +
           (u64)ts.tv_nsec;
}

emplat_timer_info emplat_system_now_info() {
    struct timespec res;
    clock_getres(CLOCK_MONOTONIC, &res);

    emplat_timer_info info = {};
    info.resolution_ns =
        ((u64)res.tv_sec * 1000000000ULL) +
        (u64)res.tv_nsec;

    info.monotonic = EMTRUE;
    info.high_precision = (info.resolution_ns <= 1000);
    return info;
}

emplat_powerstate emplat_system_powerinfo(i32* seconds, i32* percent) {
    
}