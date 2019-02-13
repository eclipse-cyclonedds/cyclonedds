/*
 * Copyright(c) 2006 to 2018 ADLINK Technology Limited and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */

/** @file
 *
 * @brief DDS C Logging API
 *
 * This header file defines the public API for logging and controlling logging
 * in the DDS C language binding.
 */
#ifndef DDS_LOG_H
#define DDS_LOG_H

#include "os/os_defs.h"

#if defined (__cplusplus)
extern "C" {
#endif

/** @defgroup log_categories Convenience log category definitions.
 *
 * These defines expand into numeric values that can be ORed together to
 * specify messages of which categories must be passed to the respective sinks.
 *
 * Every category other than DDS_LC_FATAL, DDS_LC_ERROR, DDS_LC_WARNING and
 * DDS_LC_INFO automatically falls into the trace category.
 *
 * @{
 */
/** Fatal error condition. Immediate abort on sink return. */
#define DDS_LC_FATAL (1u)
/** Error condition. */
#define DDS_LC_ERROR (2u)
/** Warning condition. */
#define DDS_LC_WARNING (4u)
/** Informational message. */
#define DDS_LC_INFO (8u)
/** Debug/trace messages related to configuration settings. */
#define DDS_LC_CONFIG (16u)
/** Debug/trace messages related to node discovery. */
#define DDS_LC_DISCOVERY (32u)
/** Currently unused. */
#define DDS_LC_DATA (64u)
/** Debug/trace messages for which no specialized category exists (yet). */
#define DDS_LC_TRACE (128u)
/** Debug/trace messages related to receive administration. */
#define DDS_LC_RADMIN (256u)
/** Debug/trace messages related to timing. */
#define DDS_LC_TIMING (512u)
/** Debug/trace messages related to send administration. */
#define DDS_LC_TRAFFIC (1024u)
/** Currently unused. */
#define DDS_LC_TOPIC (2048u)
/** Debug/trace messages related to TCP communication. */
#define DDS_LC_TCP (4096u)
/** Debug/trace messages related to parameter list processing. */
#define DDS_LC_PLIST (8192u)
/** Debug/trace messages related to the writer history cache. */
#define DDS_LC_WHC (16384u)
/** Debug/trace messages related to throttling. */
#define DDS_LC_THROTTLE (32768u)
/** All common trace categories. */
#define DDS_LC_RHC (65536u)
/** All common trace categories. */
#define DDS_LC_ALL \
    (DDS_LC_FATAL | DDS_LC_ERROR | DDS_LC_WARNING | DDS_LC_INFO | \
     DDS_LC_CONFIG | DDS_LC_DISCOVERY | DDS_LC_DATA | DDS_LC_TRACE | \
     DDS_LC_TIMING | DDS_LC_TRAFFIC | DDS_LC_TCP | DDS_LC_THROTTLE)
/** @}*/

#define DDS_LOG_MASK \
    (DDS_LC_FATAL | DDS_LC_ERROR | DDS_LC_WARNING | DDS_LC_INFO)

#define DDS_TRACE_MASK \
    (~DDS_LOG_MASK)

/** Structure with log message and meta data passed to callbacks. */
typedef struct {
  /** Log category the message falls into. */
  uint32_t priority;
  /** Filename where message was generated. */
  const char *file;
  /** Line number in file where message was generated. */
  uint32_t line;
  /** Name of function message where message was generated. */
  const char *function;
  /** Log message. */
  const char *message;
  /** Size of log message. */
  size_t size;
} dds_log_data_t;

/** Function signature that log and trace callbacks must adhere too. */
typedef void(*dds_log_write_fn_t)(void *, const dds_log_data_t *);

extern uint32_t *const dds_log_mask;

/**
 * @brief Get currently enabled log and trace categories.
 *
 * @returns A uint32_t with enabled categories set.
 */
inline uint32_t
dds_get_log_mask(void)
{
    return *dds_log_mask;
}

/**
 * @brief Set enabled log and trace categories.
 *
 * @param[in]  cats  Log and trace categories to enable.
 */
OSAPI_EXPORT void
dds_set_log_mask(
    _In_ uint32_t cats);

/**
 * @private
 */
OSAPI_EXPORT void
dds_set_log_file(
    _In_ FILE *file);

/**
 * @private
 */
OSAPI_EXPORT void
dds_set_trace_file(
    _In_ FILE *file);

/**
 * @brief Register callback to receive log messages
 *
 * Callbacks registered to handle log messages will receive messages of type
 * info, warning, error and fatal. Messages that fall into the trace category
 * will never be delivered to the callback.
 *
 * This operation is synchronous and only returns once the operation is
 * registered with all threads. Meaning that neither callback or userdata will
 * be referenced by the DDS stack on return.
 *
 * @param[in]  callback  Function pointer matching dds_log_write_fn signature
 *                       or a null pointer to restore the default sink.
 * @param[in]  userdata  User specified data passed along with each invocation
 *                       of callback.
 */
OSAPI_EXPORT void
dds_set_log_sink(
    _In_opt_ dds_log_write_fn_t callback,
    _In_opt_ void *userdata);

/**
 * @brief Register callback to receive trace messages
 *
 * Callbacks registered to handle trace messages will receive messages of type
 * info, warning, error and fatal as well as all message types that fall into
 * the trace category depending on the log mask.
 *
 * This operation is synchronous and only returns once the operation is
 * registered with all threads. Meaning that neither callback or
 * userdata will be referenced by the DDS stack on return.
 *
 * @param[in]  callback  Function pointer matching dds_log_write_fn_t signature
 *                       or a null pointer to restore the default sink.
 * @param[in]  userdata  User specified data passed along with each invocation
 *                       of callback.
 */
OSAPI_EXPORT void
dds_set_trace_sink(
    _In_opt_ dds_log_write_fn_t callback,
    _In_opt_ void *userdata);

/**
 * @brief Write a log or trace message.
 *
 * Write a log or trace message to one (or both) of the currently active sinks.
 *
 * Direct use of #dds_log is discouraged. Use #DDS_INFO, #DDS_WARNING,
 * #DDS_ERROR, #DDS_FATAL or #DDS_LOG instead.
 */
OSAPI_EXPORT int
dds_log(
    _In_ uint32_t prio,
    _In_z_ const char *file,
    _In_ uint32_t line,
    _In_z_ const char *func,
    _In_z_ _Printf_format_string_ const char *fmt,
    ...) __attribute_format__((printf,5,6));

/**
 * @brief Write a log message.
 *
 * Write a log or trace message to the currently active log and/or trace sinks
 * if the log category is enabled. Whether or not the category is enabled is
 * checked before any dds_log-related activities to save a couple of % CPU.
 *
 * Only messages that fall into one of the log categories are passed onto
 * dds_log. While messages that fall into a trace category could have been
 * passed just as easily, they are rejected so that tracing is kept entirely
 * separate from logging, if only cosmetic.
 */
#define DDS_LOG(cat, ...) \
    ((dds_get_log_mask() & (cat)) ? \
      dds_log(cat, __FILE__, __LINE__, OS_FUNCTION, __VA_ARGS__) : 0)

/** Write a log message of type #DDS_LC_INFO. */
#define DDS_INFO(...) \
  DDS_LOG(DDS_LC_INFO, __VA_ARGS__)
/** Write a log message of type #DDS_LC_WARNING. */
#define DDS_WARNING(...) \
  DDS_LOG(DDS_LC_WARNING, __VA_ARGS__)
/** Write a log message of type #DDS_LC_ERROR. */
#define DDS_ERROR(...) \
  DDS_LOG(DDS_LC_ERROR, __VA_ARGS__)
/** Write a log message of type #DDS_LC_ERROR and abort. */
#define DDS_FATAL(...) \
  dds_log(DDS_LC_FATAL, __FILE__, __LINE__, OS_FUNCTION, __VA_ARGS__)
/** Write a #DDS_LC_TRACE message. */
#define DDS_TRACE(...) \
  DDS_LOG(DDS_LC_TRACE, __VA_ARGS__)

#if defined (__cplusplus)
}
#endif

#endif /* DDS_LOG_H */
