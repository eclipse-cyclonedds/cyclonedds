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
 * Eclipse Cyclone DDS C language binding.
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
  uint32_t m_size;      /* Buffer size */
  uint32_t m_index;     /* Read/write offset from start of buffer */
  bool m_endian;        /* Endian: big (false) or little (true) */
  bool m_failed;        /* Attempt made to read beyond end of buffer */
}
dds_stream_t;

#define DDS_STREAM_BE false
#define DDS_STREAM_LE true

DDS_EXPORT dds_stream_t * dds_stream_create (uint32_t size);
DDS_EXPORT dds_stream_t * dds_stream_from_buffer (const void *buf, size_t sz, int bswap);
DDS_EXPORT void dds_stream_delete (dds_stream_t * st);
DDS_EXPORT void dds_stream_fini (dds_stream_t * st);
DDS_EXPORT void dds_stream_reset (dds_stream_t * st);
DDS_EXPORT void dds_stream_init (dds_stream_t * st, uint32_t size);
DDS_EXPORT void dds_stream_grow (dds_stream_t * st, uint32_t size);
DDS_EXPORT bool dds_stream_endian (void);

struct dds_topic_descriptor;
DDS_EXPORT void dds_stream_read_sample_w_desc (dds_stream_t * is, void * data, const struct dds_topic_descriptor * desc);
DDS_EXPORT bool dds_stream_read_bool (dds_stream_t * is);
DDS_EXPORT uint8_t dds_stream_read_uint8 (dds_stream_t * is);
DDS_EXPORT uint16_t dds_stream_read_uint16 (dds_stream_t * is);
DDS_EXPORT uint32_t dds_stream_read_uint32 (dds_stream_t * is);
DDS_EXPORT uint64_t dds_stream_read_uint64 (dds_stream_t * is);
DDS_EXPORT float dds_stream_read_float (dds_stream_t * is);
DDS_EXPORT double dds_stream_read_double (dds_stream_t * is);
DDS_EXPORT char * dds_stream_read_string (dds_stream_t * is);
DDS_EXPORT void dds_stream_read_buffer (dds_stream_t * is, uint8_t * buffer, uint32_t len);

inline char dds_stream_read_char (dds_stream_t *is) { return (char) dds_stream_read_uint8 (is); }
inline int8_t dds_stream_read_int8 (dds_stream_t *is) { return (int8_t) dds_stream_read_uint8 (is); }
inline int16_t dds_stream_read_int16 (dds_stream_t *is) { return (int16_t) dds_stream_read_uint16 (is); }
inline int32_t dds_stream_read_int32 (dds_stream_t *is) { return (int32_t) dds_stream_read_uint32 (is); }
inline int64_t dds_stream_read_int64 (dds_stream_t *is) { return (int64_t) dds_stream_read_uint64 (is); }

DDS_EXPORT void dds_stream_write_bool (dds_stream_t * os, bool val);
DDS_EXPORT void dds_stream_write_uint8 (dds_stream_t * os, uint8_t val);
DDS_EXPORT void dds_stream_write_uint16 (dds_stream_t * os, uint16_t val);
DDS_EXPORT void dds_stream_write_uint32 (dds_stream_t * os, uint32_t val);
DDS_EXPORT void dds_stream_write_uint64 (dds_stream_t * os, uint64_t val);
DDS_EXPORT void dds_stream_write_float (dds_stream_t * os, float val);
DDS_EXPORT void dds_stream_write_double (dds_stream_t * os, double val);
DDS_EXPORT void dds_stream_write_string (dds_stream_t * os, const char * val);
DDS_EXPORT void dds_stream_write_buffer (dds_stream_t * os, uint32_t len, const uint8_t * buffer);
DDS_EXPORT void *dds_stream_address (dds_stream_t * s);
DDS_EXPORT void *dds_stream_alignto (dds_stream_t * s, uint32_t a);

inline void dds_stream_write_char (dds_stream_t * os, char val) { dds_stream_write_uint8 (os, (uint8_t) val); }
inline void dds_stream_write_int8 (dds_stream_t * os, int8_t val) { dds_stream_write_uint8 (os, (uint8_t) val); }
inline void dds_stream_write_int16 (dds_stream_t * os, int16_t val) { dds_stream_write_uint16 (os, (uint16_t) val); }
inline void dds_stream_write_int32 (dds_stream_t * os, int32_t val) { dds_stream_write_uint32 (os, (uint32_t) val); }
inline void dds_stream_write_int64 (dds_stream_t * os, int64_t val) { dds_stream_write_uint64 (os, (uint64_t) val); }

#if defined (__cplusplus)
}
#endif
#endif
