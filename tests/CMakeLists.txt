file(GLOB_RECURSE TEST_SOURCES "**.c")
add_executable(clear_window ${TEST_SOURCES})
target_link_libraries(clear_window PRIVATE ember-shared ember-native)
