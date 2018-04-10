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
 * @brief DDS C Stream API
 *
 * This header file defines the public API of the Streams in the
 * CycloneDDS C language binding.
 */
#ifndef DDS_STREAM_H
#define DDS_STREAM_H

#include "os/os_public.h"
#include <stdbool.h>
#include "ddsc/dds_export.h"

#if defined (__cplusplus)
extern "C" {
#endif

struct dds_sequence;

typedef union
{
  uint8_t * p8;
  uint16_t * p16;
  uint32_t * p32;
  uint64_t * p64;
  float * pf;
  double * pd;
  void * pv;
}
dds_uptr_t;

typedef struct dds_stream
{
  dds_uptr_t m_buffer;  /* Union of pointers to start of buffer */
  size_t m_size;      /* Buffer size */
  size_t m_index;     /* Read/write offset from start of buffer */
  bool m_endian;        /* Endian: big (false) or little (true) */
  bool m_failed;        /* Attempt made to read beyond end of buffer */
}
dds_stream_t;

#define DDS_STREAM_BE false
#define DDS_STREAM_LE true

DDS_EXPORT dds_stream_t * dds_stream_create (size_t size);
DDS_EXPORT void dds_stream_delete (dds_stream_t * st);
DDS_EXPORT void dds_stream_fini (dds_stream_t * st);
DDS_EXPORT void dds_stream_reset (dds_stream_t * st);
DDS_EXPORT void dds_stream_init (dds_stream_t * st, size_t size);
DDS_EXPORT void dds_stream_grow (dds_stream_t * st, size_t size);
DDS_EXPORT bool dds_stream_endian (void);

DDS_EXPORT bool dds_stream_read_bool (dds_stream_t * is);
DDS_EXPORT uint8_t dds_stream_read_uint8 (dds_stream_t * is);
DDS_EXPORT uint16_t dds_stream_read_uint16 (dds_stream_t * is);
DDS_EXPORT uint32_t dds_stream_read_uint32 (dds_stream_t * is);
DDS_EXPORT uint64_t dds_stream_read_uint64 (dds_stream_t * is);
DDS_EXPORT float dds_stream_read_float (dds_stream_t * is);
DDS_EXPORT double dds_stream_read_double (dds_stream_t * is);
DDS_EXPORT char * dds_stream_read_string (dds_stream_t * is);
DDS_EXPORT void dds_stream_read_buffer (dds_stream_t * is, uint8_t * buffer, uint32_t len);

#define dds_stream_read_char(s) ((char) dds_stream_read_uint8 (s))
#define dds_stream_read_int8(s) ((int8_t) dds_stream_read_uint8 (s))
#define dds_stream_read_int16(s) ((int16_t) dds_stream_read_uint16 (s))
#define dds_stream_read_int32(s) ((int32_t) dds_stream_read_uint32 (s))
#define dds_stream_read_int64(s) ((int64_t) dds_stream_read_uint64 (s))

DDS_EXPORT void dds_stream_write_bool (dds_stream_t * os, bool val);
DDS_EXPORT void dds_stream_write_uint8 (dds_stream_t * os, uint8_t val);
DDS_EXPORT void dds_stream_write_uint16 (dds_stream_t * os, uint16_t val);
DDS_EXPORT void dds_stream_write_uint32 (dds_stream_t * os, uint32_t val);
DDS_EXPORT void dds_stream_write_uint64 (dds_stream_t * os, uint64_t val);
DDS_EXPORT void dds_stream_write_float (dds_stream_t * os, float val);
DDS_EXPORT void dds_stream_write_double (dds_stream_t * os, double val);
DDS_EXPORT void dds_stream_write_string (dds_stream_t * os, const char * val);
DDS_EXPORT void dds_stream_write_buffer (dds_stream_t * os, uint32_t len, uint8_t * buffer);

#define dds_stream_write_char(s,v) (dds_stream_write_uint8 ((s), (uint8_t)(v)))
#define dds_stream_write_int8(s,v) (dds_stream_write_uint8 ((s), (uint8_t)(v)))
#define dds_stream_write_int16(s,v) (dds_stream_write_uint16 ((s), (uint16_t)(v)))
#define dds_stream_write_int32(s,v) (dds_stream_write_uint32 ((s), (uint32_t)(v)))
#define dds_stream_write_int64(s,v) (dds_stream_write_uint64 ((s), (uint64_t)(v)))

#if defined (__cplusplus)
}
#endif
#endif
