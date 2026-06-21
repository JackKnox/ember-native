# `ember-native` 🔥

[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![Build](https://img.shields.io/badge/build-CMake-informational)](#building)
[![Platform](https://img.shields.io/badge/platform-Linux%20%7C%20Windows%20%7C%20macOS-lightgrey)](#)
[![Status](https://img.shields.io/badge/status-active-success)](#)

> The lightweight, cross-platform Ember driver for desktop, mobile, and web.

`ember-native` is the primary implementation of [The Ember Protocol](https://github.com/JackKnox/Ember), suitable for most use cases outside of consoles or embedded targets.

## ✨ Features
* Supports all major desktop platforms (Windows, X11, Wayland, macOS).
* Minimal overhead, designed for high-performance systems.
* Drop-in integration — just link and go.
* Supports all Ember-provided domains.

## 📋 Prerequisites
* CMake `16.0` or higher
* A C99 compatible compiler
* *Yep that's it*

## ⚙️ Usage
`ember-native` can be built as either a static or dynamic library. Building statically is for general use cases. Building dynamically enables hot-swapping Ember Drivers at runtime or using Ember across programming languages. This is controlled by the `BUILD_SHARED_LIBS` CMake variable. It's extremely recommended to built `ember-native` from source alongside your project but pre-built binaries are offered in the [releases tab](https://github.com/JackKnox/ember-native/releases).

Link `ember-native` with your application:
```cmake
target_link_libraries(your-app PRIVATE ember-native)
```

### CMake Variables
CMake variables may be used to configure your build of `ember-native`:
|  CMake Variable     |  Description |
|---------------------|--------------|
| **EMBER_NATIVE_ENABLE_POSIX**      | Enable POSIX support in PLATFORM domain                                  |
| **EMBER_NATIVE_ENABLE_WAYLAND**    | Enable Wayland backend in WINDOW domain                                  |
| **EMBER_NATIVE_ENABLE_COCOA**      | Enable Cocoa backend in WINDOW domain                                    |
| **EMBER_NATIVE_ENABLE_WIN32_PLAT** | Enable Win32 backend in PLATFORM domain                                  |
| **EMBER_NATIVE_ENABLE_WIN32_WIN**  | Enable Win32 backend in WINDOW domain                                    |
| **EMBER_NATIVE_ENABLE_DX12**       | Enable DirectX 12 in GPU domain                                          |
| **EMBER_NATIVE_ENABLE_METAL**      | Enable Metal in GPU domain                                               |
| **EMBER_NATIVE_ENABLE_VULKAN**     | Enable Vulkan in GPU domain                                              |
| **EMBER_NATIVE_USE_WINDOW**        | Build WINDOW domain in ember-native                                      |
| **EMBER_NATIVE_USE_GPU**           | Build GPU domain in ember-native                                         |
| **EMBER_NATIVE_USE_AUDIO**         | Build AUDIO domain in ember-native                                       |
| **EMBER_NATIVE_USE_NET**           | Build NET domain in ember-native                                         |
| **EMBER_NATIVE_ENABLE_LEGACY**     | Additionally builds legacy platforms, signifcantly increases binary size |
| **EMBER_NATIVE_SPLIT_DOMAINS**     | Splits domains into seperate binaries, either static or dynamic          |
| **EMBER_NATIVE_USE_VULKAN_LOADER** | Load Vulkan functions from system library                                |
| **EMBER_NATIVE_USE_IPC**           | Enable IPC support in PLATFORM domain                                    |
| **EMBER_NATIVE_USE_FILESYSTEM**    | Enable filesystem support in PLATFORM domain                             |
| **EMBER_NATIVE_DIST**              | Distribution/release build options                                       |
| **EMBER_NATIVE_VERBOSE**           | Enable verbose logging                                                   |
| **EMBER_NATIVE_MEMORY_TRACKER**    | Enable memory tracking                                                   |
| **EMBER_NATIVE_POISON_MEMORY**     | Poison freed/uninitialized memory                                        |

## 🔨 Building
```bash
git clone https://github.com/JackKnox/ember-native.git
mkdir build && cd build
cmake ..
cmake --build .
cmake --install .
```

## 📄 License
Distributed under the MIT License. See [`LICENSE`](LICENSE) for details.