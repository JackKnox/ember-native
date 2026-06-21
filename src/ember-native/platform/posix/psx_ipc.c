#include "defines.h"
#include "psx_types.h"

#include <ember/platform/ipc.h>

#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>

em_result emplat_open_shm(const char* name, u64 size, emplat_shm_state* out_state) {
    
}

void* emplat_shm_pointer(emplat_shm_state* state) {
    
}

void emplat_close_shm(emplat_shm_state* state) {
    
}