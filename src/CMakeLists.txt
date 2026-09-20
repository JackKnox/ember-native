file(GLOB_RECURSE EMBER_NATIVE_SOURCES "./**.c")
add_library(ember-native ${EMBER_NATIVE_SOURCES})

target_include_directories(ember-native PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(ember-native PRIVATE ember-headers wayland-client vulkan)
