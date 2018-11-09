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
#include <assert.h>
#include <string.h>
#include "dds__key.h"
#include "dds__stream.h"
#include "ddsi/ddsi_serdata.h"
#include "ddsi/q_bswap.h"
#include "ddsi/q_md5.h"

#ifndef NDEBUG
static bool keyhash_is_reset(const dds_key_hash_t *kh)
{
  return !kh->m_set;
}
#endif

/*
  dds_key_gen: Generates key and keyhash for a sample.
  See section 9.6.3.3 of DDSI spec.
*/

static void dds_key_gen_stream (const dds_topic_descriptor_t * const desc, dds_stream_t *os, const char *sample)
{
  const char * src;
  const uint32_t * op;
  uint32_t i;
  uint32_t len = 0;

  for (i = 0; i < desc->m_nkeys; i++)
  {
    op = desc->m_ops + desc->m_keys[i].m_index;
    src = sample + op[1];
    assert ((*op & DDS_OP_FLAG_KEY) && ((DDS_OP_MASK & *op) == DDS_OP_ADR));

    switch (DDS_OP_TYPE (*op))
    {
      case DDS_OP_VAL_1BY:
      {
        dds_stream_write_uint8 (os, *((const uint8_t *) src));
        break;
      }
      case DDS_OP_VAL_2BY:
      {
        dds_stream_write_uint16 (os, *((const uint16_t *) src));
        break;
      }
      case DDS_OP_VAL_4BY:
      {
        dds_stream_write_uint32 (os, *((const uint32_t *) src));
        break;
      }
      case DDS_OP_VAL_8BY:
      {
        dds_stream_write_uint64 (os, *((const uint64_t *) src));
        break;
      }
      case DDS_OP_VAL_STR:
      {
        src = *((char**) src);
      }
        /* FALLS THROUGH */
      case DDS_OP_VAL_BST:
      {
        len = (uint32_t) (strlen (src) + 1);
        dds_stream_write_uint32 (os, len);
        dds_stream_write_buffer (os, len, (const uint8_t *) src);
        break;
      }
      case DDS_OP_VAL_ARR:
      {
        uint32_t size = dds_op_size[DDS_OP_SUBTYPE (*op)];
        char *dst;
        len = size * op[2];
        dst = dds_stream_alignto (os, op[2]);
        dds_stream_write_buffer (os, len, (const uint8_t *) src);
        if (dds_stream_endian () && (size != 1u))
          dds_stream_swap (dst, size, op[2]);
        break;
      }
      default: assert (0);
    }
  }
}

void dds_key_gen (const dds_topic_descriptor_t * const desc, dds_key_hash_t * kh, const char * sample)
{
  assert(keyhash_is_reset(kh));

  kh->m_set = 1;
  if (desc->m_nkeys == 0)
    kh->m_iskey = 1;
  else if (desc->m_flagset & DDS_TOPIC_FIXED_KEY)
  {
    dds_stream_t os;
    kh->m_iskey = 1;
    dds_stream_init(&os, 0);
    os.m_endian = 0;
    os.m_buffer.pv = kh->m_hash;
    os.m_size = 16;
    dds_key_gen_stream (desc, &os, sample);
  }
  else
  {
    dds_stream_t os;
    md5_state_t md5st;
    kh->m_iskey = 0;
    dds_stream_init(&os, 64);
    os.m_endian = 0;
    dds_key_gen_stream (desc, &os, sample);
    md5_init (&md5st);
    md5_append (&md5st, os.m_buffer.p8, os.m_index);
    md5_finish (&md5st, (unsigned char *) kh->m_hash);
    dds_stream_fini (&os);
  }
}
