#include "defines.h"
#include "psx_types.h"

#include <ember/platform/filesystem.h>

#include <stdio.h>
#include <sys/stat.h>

void emplat_file_get_info(const char* filepath, emplat_file_info* out_info) {
    struct stat st;

    if (stat(filepath, &st) != 0) {
        out_info->exists = EMFALSE; 
        return; // We dont really care about errno just say it doesn't exist.
    }

    out_info->exists = EMTRUE;
    out_info->size   = (u64)st.st_size;
    out_info->is_directory = S_ISDIR(st.st_mode);

    out_info->created_time  = 0;
    out_info->modified_time = (u64)st.st_mtime;
    out_info->accessed_time = (u64)st.st_atime;
}

em_result emplat_file_open(const char* filepath, emplat_file_flags flags, emplat_file* out_file) {
    const char* mode_str = NULL;
    if ((flags & EMBER_FILE_FLAGS_READ) != 0 && (flags & EMBER_FILE_FLAGS_WRITE) != 0)
		mode_str = "w+b";
	else if ((flags & EMBER_FILE_FLAGS_READ) != 0 && (flags & EMBER_FILE_FLAGS_WRITE) == 0)
		mode_str = "rb";
	else if ((flags & EMBER_FILE_FLAGS_READ) == 0 && (flags & EMBER_FILE_FLAGS_WRITE) != 0)
		mode_str = "wb";

    *out_file = fopen(filepath, mode_str);
    return EMBER_RESULT_OK;
}

void emplat_file_close(emplat_file* file) {
    if (*file != 0) {
        fclose((FILE*)*file);
        *file = 0;
    }
}

u64 emplat_file_size(emplat_file* file) {
    FILE* handle = *file;
    if (!handle) return 0;
    
    u64 size = 0;
    fseek(handle, 0, SEEK_END);
    size = ftell(handle);
    rewind(handle);

    return size;
}

em_result emplat_file_read(emplat_file* file, u64 size, void* out_data, u64* out_data_size) {
    FILE* handle = *file;
    if (!handle) return EMBER_RESULT_INVALID_VALUE;

    *out_data_size = fread(out_data, 1, size, handle);
    if (ferror(handle))
        return EMBER_RESULT_UNKNOWN; // TODO: Convert errno to result enum.
    return EMBER_RESULT_OK;
}

em_result emplat_file_write(emplat_file* file, u64 size, const void* data, u64* out_data_size) {
    FILE* handle = *file;
    if (!handle) return EMBER_RESULT_INVALID_VALUE;

    *out_data_size = fwrite(data, 1, size, handle);
    if (ferror(handle))
        return EMBER_RESULT_UNKNOWN; // TODO: Convert errno to result enum.
    if (*out_data_size != size)
        return EMBER_RESULT_UNKNOWN; // Should of been caught be ferror() but go on then.
    return EMBER_RESULT_OK;
}

em_result emplat_file_lock(emplat_file* file) {
    FILE* handle = *file;
    if (!handle) return EMBER_RESULT_INVALID_VALUE;

    flockfile(handle);
    return EMBER_RESULT_OK; // TODO: Convert errno to result enum.
}

em_result emplat_file_unlock(emplat_file* file) {
    FILE* handle = *file;
    if (!handle) return EMBER_RESULT_INVALID_VALUE;

    funlockfile(handle);
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