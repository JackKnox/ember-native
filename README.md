# `ember-native` 🔥

[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![Build](https://img.shields.io/badge/build-CMake-informational)](#building)
[![Platform](https://img.shields.io/badge/platform-Linux%20%7C%20Windows%20%7C%20macOS-lightgrey)](#)
[![Status](https://img.shields.io/badge/status-active-success)](#)

> The lightweight, cross-platform Ember driver for desktop, mobile, and web.

`ember-native` is the primary implementation of The Ember Protocol, suitable for most use cases outside of consoles or embedded targets.

## ✨ Features
* Supports all major desktop platforms (Windows, X11, Wayland, macOS).
* Minimal overhead, designed for high-performance systems.
* Drop-in integration — just link and go.
* Includes additional utilities to help optimize your Ember integration.
* Supports all Ember-provided domains.

## 📋 Prerequisites
* CMake `16.0` or higher
* A C99 compatible compiler
* *Yep that's it*

## ⚙️ Usage
`ember-native` can be built as either a static or dynamic library. Building statically allows aggressive per-application optimization. Building dynamically enables hot-swapping Ember Drivers at runtime. This is controlled by the `BUILD_SHARED_LIBS` CMake variable.

Link `ember-native` with your application:
```cmake
target_link_libraries(your-app PRIVATE ember-native)
```

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