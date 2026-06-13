#include "defines.h"
#include "psx_types.h"

#include <ember/platform/threading.h>

em_result emplat_thread_create(emplat_thread* thr, PFN_thread_start func, void* arg) {
    
}

emplat_thread emplat_thread_current() {
    
}

b8 emplat_thread_equal(emplat_thread thr0, emplat_thread thr1) {
    
}

em_result emplat_thread_join(emplat_thread thr, u32* res) {
    
}

em_result emplat_mutex_init(emplat_mutex_type type, emplat_mutex* mtx) {
    
}

void emplat_mutex_destroy(emplat_mutex* mtx) {
    
}

em_result emplat_mutex_lock(emplat_mutex* mtx) {
    
}

em_result emplat_mutex_timedlock(emplat_mutex* mtx, f64 ms) {
    
}

em_result emplat_mutex_trylock(emplat_mutex* mtx) {
    
}

em_result emplat_mutex_unlock(emplat_mutex* mtx) {
    
}

em_result emplat_cond_init(emplat_cond* cond) {
    
}

void emplat_cond_destroy(emplat_cond* cond) {
    
}

em_result emplat_cond_signal(emplat_cond* cond) {
    
}

em_result emplat_cond_broadcast(emplat_cond* cond) {
    
}

em_result emplat_cond_wait(emplat_cond* cond, emplat_mutex* mtx) {
    
}

em_result emplat_cond_timedwait(emplat_cond* cond, emplat_mutex* mtx, f64 ms) {
    
}