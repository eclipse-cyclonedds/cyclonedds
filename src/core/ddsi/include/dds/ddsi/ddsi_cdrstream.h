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
#ifndef DDSI_CDRSTREAM_H
#define DDSI_CDRSTREAM_H

#include "dds/ddsi/ddsi_serdata.h"
#include "dds/ddsi/ddsi_serdata_default.h"

#if defined (__cplusplus)
extern "C" {
#endif

typedef struct dds_istream {
  const unsigned char *m_buffer;
  uint32_t m_size;      /* Buffer size */
  uint32_t m_index;     /* Read/write offset from start of buffer */
} dds_istream_t;

typedef struct dds_ostream {
  unsigned char *m_buffer;
  uint32_t m_size;      /* Buffer size */
  uint32_t m_index;     /* Read/write offset from start of buffer */
} dds_ostream_t;

typedef struct dds_ostreamBE {
  dds_ostream_t x;
} dds_ostreamBE_t;

DDS_EXPORT void dds_ostream_init (dds_ostream_t * __restrict st, uint32_t size);
DDS_EXPORT void dds_ostream_fini (dds_ostream_t * __restrict st);
DDS_EXPORT void dds_ostreamBE_init (dds_ostreamBE_t * __restrict st, uint32_t size);
DDS_EXPORT void dds_ostreamBE_fini (dds_ostreamBE_t * __restrict st);

bool dds_stream_normalize (void * __restrict data, uint32_t size, bool bswap, const struct ddsi_sertopic_default * __restrict topic, bool just_key);

void dds_stream_write_sample (dds_ostream_t * __restrict os, const void * __restrict data, const struct ddsi_sertopic_default * __restrict topic);
void dds_stream_read_sample (dds_istream_t * __restrict is, void * __restrict data, const struct ddsi_sertopic_default * __restrict topic);
void dds_stream_free_sample (void *data, const uint32_t * ops);

uint32_t dds_stream_countops (const uint32_t * __restrict ops);
size_t dds_stream_check_optimize (const struct ddsi_sertopic_default_desc * __restrict desc);
void dds_istream_from_serdata_default (dds_istream_t * __restrict s, const struct ddsi_serdata_default * __restrict d);
void dds_ostream_from_serdata_default (dds_ostream_t * __restrict s, struct ddsi_serdata_default * __restrict d);
void dds_ostream_add_to_serdata_default (dds_ostream_t * __restrict s, struct ddsi_serdata_default ** __restrict d);
void dds_ostreamBE_from_serdata_default (dds_ostreamBE_t * __restrict s, struct ddsi_serdata_default * __restrict d);
void dds_ostreamBE_add_to_serdata_default (dds_ostreamBE_t * __restrict s, struct ddsi_serdata_default ** __restrict d);

void dds_stream_write_key (dds_ostream_t * __restrict os, const char * __restrict sample, const struct ddsi_sertopic_default * __restrict topic);
void dds_stream_write_keyBE (dds_ostreamBE_t * __restrict os, const char * __restrict sample, const struct ddsi_sertopic_default * __restrict topic);
void dds_stream_extract_key_from_data (dds_istream_t * __restrict is, dds_ostream_t * __restrict os, const struct ddsi_sertopic_default * __restrict topic);
void dds_stream_extract_keyBE_from_data (dds_istream_t * __restrict is, dds_ostreamBE_t * __restrict os, const struct ddsi_sertopic_default * __restrict topic);
void dds_stream_extract_keyhash (dds_istream_t * __restrict is, dds_keyhash_t * __restrict kh, const struct ddsi_sertopic_default * __restrict topic, const bool just_key);

void dds_stream_read_key (dds_istream_t * __restrict is, char * __restrict sample, const struct ddsi_sertopic_default * __restrict topic);

size_t dds_stream_print_key (dds_istream_t * __restrict is, const struct ddsi_sertopic_default * __restrict topic, char * __restrict buf, size_t size);

size_t dds_stream_print_sample (dds_istream_t * __restrict is, const struct ddsi_sertopic_default * __restrict topic, char * __restrict buf, size_t size);

/* For marshalling op code handling */

#define DDS_OP_MASK 0xff000000
#define DDS_OP_TYPE_MASK 0x00ff0000
#define DDS_OP_SUBTYPE_MASK 0x0000ff00
#define DDS_OP_JMP_MASK 0x0000ffff
#define DDS_OP_FLAGS_MASK 0x000000ff
#define DDS_JEQ_TYPE_MASK 0x00ff0000

#define DDS_OP(o)         ((enum dds_stream_opcode) ((o) & DDS_OP_MASK))
#define DDS_OP_TYPE(o)    ((enum dds_stream_typecode) (((o) & DDS_OP_TYPE_MASK) >> 16))
#define DDS_OP_SUBTYPE(o) ((enum dds_stream_typecode) (((o) & DDS_OP_SUBTYPE_MASK) >> 8))
#define DDS_OP_FLAGS(o)   ((o) & DDS_OP_FLAGS_MASK)
#define DDS_OP_ADR_JSR(o) ((o) & DDS_OP_JMP_MASK)
#define DDS_OP_JUMP(o)    ((int16_t) ((o) & DDS_OP_JMP_MASK))
#define DDS_OP_ADR_JMP(o) ((o) >> 16)
#define DDS_JEQ_TYPE(o)   ((enum dds_stream_typecode) (((o) & DDS_JEQ_TYPE_MASK) >> 16))

#if defined (__cplusplus)
}
#endif
#endif
