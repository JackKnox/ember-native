#include "defines.h"
#include "wl_types.h"

#include <ember/window/dialog.h>

em_result emplat_dialog_notify(const char* title, const char* message, emplat_popup_type type) {
    
}

em_result emplat_dialog_option(const char* title, const char* message, const char** options, u32 option_count, emplat_popup_type type) {
    
}

em_result emplat_dialog_input(const char* title, const char* message, char** out_text, u32 max_size, b8 hide_text) {
    
}

em_result emplat_dialog_save_file(const char* title, const char* patterns, u32 pattern_count, char** out_text, u32 max_size, const char* default_path) {
    
}

em_result emplat_dialog_open_file(const char* title, u32 pattern_count, const char* patterns, char** out_text, u32 max_size, const char* default_path, b8 multiple_files) {
    
}

em_result emplat_dialog_open_folder(const char* title, char** out_text, u32 max_size, const char* default_path) {
    
}

em_result emplat_dialog_colour(const char* title, u32 default_colour, u32* out_colour) {
    
}