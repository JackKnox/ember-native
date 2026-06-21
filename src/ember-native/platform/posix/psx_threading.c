#include "defines.h"
#include "psx_types.h"

#include <ember/platform/threading.h>

// TODO: Cannot yet implement due to limitations with the Ember protocol.
// TODO: Must not allocate memory just to create thread/mutex/cond, that's just ridiculous.

em_result emplat_thread_create(PFN_thread_start func, void* arg, emplat_thread* out_thr) {
    
}

emplat_thread emplat_thread_current() {
    
}

b8 emplat_thread_equal(emplat_thread thr0, emplat_thread thr1) {
    
}

em_result emplat_thread_join(emplat_thread thr, void** res) {
    
}

em_result emplat_mutex_create(emplat_mutex_type type, emplat_mutex* out_mtx) {
    
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

em_result emplat_cond_create(emplat_cond* out_cond) {
    
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