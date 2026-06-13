file(GLOB_RECURSE EMBER_NATIVE_SOURCES "ember-native/**.c")
add_library(ember-native ${EMBER_NATIVE_SOURCES})

target_include_directories(ember-native PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/ember-native")
target_link_libraries(ember-native PUBLIC ember-shared)