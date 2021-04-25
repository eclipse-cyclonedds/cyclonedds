/*
 * Copyright(c) 2021 Christophe Bedard
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */

// Provide fake header guard for cpplint
#undef _DDS_LTTNG_H_
#ifndef _DDS_LTTNG_H_
#define _DDS_LTTNG_H_

#include "dds/features.h"

#ifndef DDS_HAS_LTTNG_TRACING

#define TRACEPOINT(event_name, ...) ((void) (0))

#else  // DDS_HAS_LTTNG_TRACING

#undef TRACEPOINT_PROVIDER
#define TRACEPOINT_PROVIDER dds

#define TRACEPOINT(event_name, ...) \
  tracepoint(TRACEPOINT_PROVIDER, event_name, __VA_ARGS__)

#undef TRACEPOINT_INCLUDE
#define TRACEPOINT_INCLUDE "tracing_lttng.h"

#if !defined(__DDS_LTTNG_H_) || defined(TRACEPOINT_HEADER_MULTI_READ)
#define __DDS_LTTNG_H_

#include <lttng/tracepoint.h>

#ifndef LTTNG_UST_HAVE_SDT_INTEGRATION
#  ifdef _MSC_VER
#    pragma message ("lttng-ust has not been configured & built with SDT integration (--with-sdt)")
#  else
#    warning lttng-ust has not been configured & built with SDT integration (--with-sdt)
#  endif
#endif

#include <stdint.h>
#include <stdbool.h>

#define DDS_GID_STORAGE_SIZE 16u

TRACEPOINT_EVENT(
  TRACEPOINT_PROVIDER,
  create_writer,
  TP_ARGS(
    const void *, writer_arg,
    char *, topic_name_arg,
    const uint8_t *, gid_arg
  ),
  TP_FIELDS(
    ctf_integer_hex(const void *, writer, writer_arg)
    ctf_string(topic_name, topic_name_arg)
    ctf_array(uint8_t, gid, gid_arg, DDS_GID_STORAGE_SIZE)
  )
)

TRACEPOINT_EVENT(
  TRACEPOINT_PROVIDER,
  create_reader,
  TP_ARGS(
    const void *, reader_arg,
    char *, topic_name_arg,
    const uint8_t *, gid_arg
  ),
  TP_FIELDS(
    ctf_integer_hex(const void *, reader, reader_arg)
    ctf_string(topic_name, topic_name_arg)
    ctf_array(uint8_t, gid, gid_arg, DDS_GID_STORAGE_SIZE)
  )
)

TRACEPOINT_EVENT(
  TRACEPOINT_PROVIDER,
  write,
  TP_ARGS(
    const void *, writer_arg,
    const void *, data_arg,
    int64_t, timestamp_arg
  ),
  TP_FIELDS(
    ctf_integer_hex(const void *, writer, writer_arg)
    ctf_integer_hex(const void *, data, data_arg)
    ctf_integer(int64_t, timestamp, timestamp_arg)
  )
)

TRACEPOINT_EVENT(
  TRACEPOINT_PROVIDER,
  read,
  TP_ARGS(
    const void *, reader_arg,
    const void *, buffer_arg
  ),
  TP_FIELDS(
    ctf_integer_hex(const void *, reader, reader_arg)
    ctf_integer_hex(const void *, buffer, buffer_arg)
  )
)

#endif  // __DDS_LTTNG_H_

#include <lttng/tracepoint-event.h>

#endif  // DDS_HAS_LTTNG_TRACING

#endif  // _DDS_LTTNG_H_
