cmake_minimum_required(VERSION 3.16)
project(ember-native VERSION 0.5.0 LANGUAGES C)

include(FetchContent)
set(CMAKE_C_STANDARD 99)

if(APPLE)
    if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
        set(EMBER_NATIVE_PLATFORM_MACOS ON)
    # elseif(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    #     set(EMBER_NATIVE_PLATFORM_IOS ON)
    endif()
endif()

# Build system configuration
option(EMBER_NATIVE_ENABLE_POSIX           "Enable POSIX support in PLATFORM domain"                ${UNIX})
option(EMBER_NATIVE_ENABLE_WAYLAND         "Enable Wayland backend in WINDOW domain"                ${UNIX})
option(EMBER_NATIVE_ENABLE_COCOA           "Enable Cocoa backend in WINDOW domain"                  ${EMBER_NATIVE_PLATFORM_MACOS})
option(EMBER_NATIVE_ENABLE_WIN32_PLAT      "Enable Win32 backend in PLATFORM domain"                ${WIN32})
option(EMBER_NATIVE_ENABLE_WIN32_WIN       "Enable Win32 backend in WINDOW domain"                  ${WIN32})
option(EMBER_NATIVE_ENABLE_DX12            "Enable DirectX 12 in GPU domain"                        ${WIN32})
option(EMBER_NATIVE_ENABLE_METAL           "Enable Metal in GPU domain"                             ${APPLE})
option(EMBER_NATIVE_ENABLE_VULKAN          "Enable Vulkan in GPU domain"                            ON)
option(EMBER_NATIVE_USE_WINDOW             "Build WINDOW domain in ember-native"                    ON)
option(EMBER_NATIVE_USE_GPU                "Build GPU domain in ember-native"                       ON)
option(EMBER_NATIVE_USE_AUDIO              "Build AUDIO domain in ember-native"                     ON)
option(EMBER_NATIVE_USE_NET                "Build NET domain in ember-native"                       ON)
option(EMBER_NATIVE_ENABLE_LEGACY          "Additionally builds legacy platforms, signifcantly increases binary size"      OFF)
option(EMBER_NATIVE_SPLIT_DOMAINS          "Splits domains into seperate binaries, either static or dynamic"               OFF)

# Compiler configurations
option(EMBER_NATIVE_USE_VULKAN_LOADER      "Load Vulkan functions from system library"              ON)
option(EMBER_NATIVE_USE_IPC                "Enable IPC support in PLATFORM domain"                  ON)
option(EMBER_NATIVE_USE_FILESYSTEM         "Enable filesystem support in PLATFORM domain"           ON)
option(EMBER_NATIVE_DIST                   "Distribution/release build options"                     OFF)
option(EMBER_NATIVE_VERBOSE                "Enable verbose logging"                                 OFF)
option(EMBER_NATIVE_MEMORY_TRACKER         "Enable memory tracking"                                 OFF)
option(EMBER_NATIVE_POISON_MEMORY          "Poison freed/uninitialized memory"                      OFF)

add_subdirectory(src)

FetchContent_Declare(
    Ember
    GIT_REPOSITORY https://github.com/JackKnox/Ember.git
    GIT_TAG        master
)
FetchContent_MakeAvailable(Ember)