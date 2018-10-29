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
#ifndef _DDS_STREAM_H_
#define _DDS_STREAM_H_

#include "ddsi/ddsi_serdata.h"
#include "ddsi/ddsi_serdata_default.h"

#if defined (__cplusplus)
extern "C" {
#endif

void dds_stream_write_sample
(
  dds_stream_t * os,
  const void * data,
  const struct ddsi_sertopic_default * topic
);
void dds_stream_read_sample
(
  dds_stream_t * is,
  void * data,
  const struct ddsi_sertopic_default * topic
);

size_t dds_stream_check_optimize (_In_ const dds_topic_descriptor_t * desc);
void dds_stream_from_serdata_default (dds_stream_t * s, const struct ddsi_serdata_default *d);
void dds_stream_add_to_serdata_default (dds_stream_t * s, struct ddsi_serdata_default **d);

void dds_stream_write_key (dds_stream_t * os, const char * sample, const struct ddsi_sertopic_default * topic);
uint32_t dds_stream_extract_key (dds_stream_t *is, dds_stream_t *os, const uint32_t *ops, const bool just_key);
void dds_stream_read_key
(
  dds_stream_t * is,
  char * sample,
  const dds_topic_descriptor_t * desc
);
void dds_stream_read_keyhash
(
  dds_stream_t * is,
  dds_key_hash_t * kh,
  const dds_topic_descriptor_t * desc,
  const bool just_key
);
char * dds_stream_reuse_string
(
  dds_stream_t * is,
  char * str,
  const uint32_t bound
);
DDS_EXPORT void dds_stream_swap (void * buff, uint32_t size, uint32_t num);

extern const uint32_t dds_op_size[5];

/* For marshalling op code handling */

#define DDS_OP_MASK 0xff000000
#define DDS_OP_TYPE_MASK 0x00ff0000
#define DDS_OP_SUBTYPE_MASK 0x0000ff00
#define DDS_OP_JMP_MASK 0x0000ffff
#define DDS_OP_FLAGS_MASK 0x000000ff
#define DDS_JEQ_TYPE_MASK 0x00ff0000

#define DDS_OP(o) ((o) & DDS_OP_MASK)
#define DDS_OP_TYPE(o) (((o) & DDS_OP_TYPE_MASK) >> 16)
#define DDS_OP_SUBTYPE(o) (((o) & DDS_OP_SUBTYPE_MASK) >> 8)
#define DDS_OP_FLAGS(o) ((o) & DDS_OP_FLAGS_MASK)
#define DDS_OP_ADR_JSR(o) ((o) & DDS_OP_JMP_MASK)
#define DDS_OP_JUMP(o) ((int16_t) ((o) & DDS_OP_JMP_MASK))
#define DDS_OP_ADR_JMP(o) ((o) >> 16)
#define DDS_JEQ_TYPE(o) (((o) & DDS_JEQ_TYPE_MASK) >> 16)

#if defined (__cplusplus)
}
#endif
#endif
