file(GLOB_RECURSE EMBER_NATIVE_SOURCES "ember-native/**.c")
add_library(ember-native ${EMBER_NATIVE_SOURCES})

target_include_directories(ember-native PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/ember-native")

find_package(Wayland REQUIRED COMPONENTS Client)

# TODO: ember-shared includes the C helpers for Ember so if someone
# were using ember-native and included the helpers there would be linker errors.
target_link_libraries(ember-native PUBLIC ember-shared Wayland::Client)
