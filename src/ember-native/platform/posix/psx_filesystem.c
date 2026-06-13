#include "defines.h"
#include "psx_types.h"

#include <ember/platform/filesystem.h>

void emplat_file_get_info(const char* filepath, emplat_file_info* out_info) {

}

em_result emplat_file_open(const char* filepath, emplat_file_flags flags, emplat_file* out_file) {
    
}

void emplat_file_close(emplat_file* file) {
    
}

u64 emplat_file_size(emplat_file* file) {
    
}

em_result emplat_file_read(emplat_file* file, u64 size, void* out_data, u64* out_data_size) {
    
}

em_result emplat_file_write(emplat_file* file, u64 size, const void* data, u64* out_data_size) {
    
}

em_result emplat_file_lock(emplat_file* file, b8 block) {
    
}

em_result emplat_file_unlock(emplat_file* file) {
    
}

em_result emplat_file_write_safe(const char* filepath, const void* data, u64 size) {
    
}

em_result emplat_filewatcher_create(PFN_on_filewatch callback, emplat_filewatcher* out_filewatcher) {
    
}

void emplat_filewatcher_destroy(emplat_filewatcher* filewatcher) {
    
}

em_result emplat_filewatcher_add(emplat_filewatcher* filewatcher, const char* filepath) {
    
}

em_result emplat_filewatcher_add_pattern(emplat_filewatcher* filewatcher, const char* pattern) {
    
}

em_result emplat_filewatcher_poll(emplat_filewatcher* filewatcher, u32 event_count, emplat_filewatch_event* out_events) {
    
}