#pragma once

#include "ember/core.h"

#include <string.h>
#include <stdlib.h>

#ifndef EM_LOG
// General logging function for Ember.
#define EM_LOG(level, subsystem, message, ...) emlog_console(level, "Ember/" subsystem, message __VA_OPT__(,) __VA_ARGS__)
#endif

#ifndef EM_FATAL
/** @brief Logs an fatal-level message. */
#define EM_FATAL(subsystem, message, ...) EM_LOG(LOG_LEVEL_FATAL, subsystem, message, __VA_ARGS__)
#endif

#ifndef EM_ERROR
/** @brief Logs an error-level message. */
#define EM_ERROR(subsystem, message, ...) EM_LOG(LOG_LEVEL_ERROR, subsystem, message, __VA_ARGS__)
#endif

#ifndef EM_WARN
/** @brief Logs an warn-level message. */
#define EM_WARN(subsystem, message, ...) EM_LOG(LOG_LEVEL_WARN, subsystem, message, __VA_ARGS__)
#endif

#ifndef EM_INFO
/** @brief Logs an info-level message. */
#define EM_INFO(subsystem, message, ...) EM_LOG(LOG_LEVEL_INFO, subsystem, message, __VA_ARGS__)
#endif

#ifndef EM_TRACE
/** @brief Logs an trace-level message. */
#define EM_TRACE(subsystem, message, ...) EM_LOG(LOG_LEVEL_TRACE, subsystem, message, __VA_ARGS__)
#endif

#ifndef EM_DEV
/** @brief Logs an dev-level message. */
#define EM_DEV(subsystem, message, ...) EM_LOG(LOG_LEVEL_DEV, subsystem, message, __VA_ARGS__)
#endif

#if EMBER_DIST
#   define EM_ASSERT(x) ((void)0)
#else
// * NOTE: Assertions in the library are used to validate internal state and function arguments 
// *       that originate from within the library itself. Assertions should only be used to detect 
// *       programming errors or invalid library state. Any user-provided input must be validated explicitly 
// *       and handled gracefully, with clear and descriptive error messages, rather than using assertions.
#   define EM_ASSERT(x) do { if (!(x)) EM_FATAL("ASSERT", #x, __FILE__, __LINE__); } while (0)
#endif
