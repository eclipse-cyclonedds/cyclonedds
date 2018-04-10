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
#include "ddsi/ddsi_ser.h"
#include "ddsi/q_bswap.h"
#include "ddsi/q_md5.h"

void dds_key_md5 (dds_key_hash_t * kh)
{
  md5_state_t md5st;
  md5_init (&md5st);
  md5_append (&md5st, (md5_byte_t*) kh->m_key_buff, kh->m_key_len);
  md5_finish (&md5st, (unsigned char *) kh->m_hash);
}

/* 
  dds_key_gen: Generates key and keyhash for a sample.
  See section 9.6.3.3 of DDSI spec.
*/

void dds_key_gen
(
  const dds_topic_descriptor_t * const desc,
  dds_key_hash_t * kh,
  const char * sample
)
{
  const char * src;
  const uint32_t * op;
  uint32_t i;
  uint32_t len = 0;
  char * dst;

  assert (desc->m_nkeys);
  assert (kh->m_hash[0] == 0 && kh->m_hash[15] == 0);

  kh->m_flags = DDS_KEY_SET | DDS_KEY_HASH_SET;

  /* Select key buffer to use */

  if (desc->m_flagset & DDS_TOPIC_FIXED_KEY)
  {
    kh->m_flags |= DDS_KEY_IS_HASH;
    kh->m_key_len = sizeof (kh->m_hash);
    dst = kh->m_hash;
  }
  else
  {
    /* Calculate key length */

    for (i = 0; i < desc->m_nkeys; i++)
    {
      op = desc->m_ops + desc->m_keys[i].m_index;
      src = sample + op[1];

      switch (DDS_OP_TYPE (*op))
      {
        case DDS_OP_VAL_1BY: len += 1; break;
        case DDS_OP_VAL_2BY: len += 2; break;
        case DDS_OP_VAL_4BY: len += 4; break;
        case DDS_OP_VAL_8BY: len += 8; break;
        case DDS_OP_VAL_STR: src = *((char**) src); /* Fall-through intentional */
        case DDS_OP_VAL_BST: len += (uint32_t) (5 + strlen (src)); break;
        case DDS_OP_VAL_ARR: 
          len += op[2] * dds_op_size[DDS_OP_SUBTYPE (*op)];
          break;
        default: assert (0);
      }
    }

    kh->m_key_len = len;
    if (len > kh->m_key_buff_size)
    {
      kh->m_key_buff = dds_realloc_zero (kh->m_key_buff, len);
      kh->m_key_buff_size = len;
    }
    dst = kh->m_key_buff;
  }

  /* Write keys to buffer (Big Endian CDR encoded with no padding) */

  for (i = 0; i < desc->m_nkeys; i++)
  {
    op = desc->m_ops + desc->m_keys[i].m_index;
    src = sample + op[1];
    assert ((*op & DDS_OP_FLAG_KEY) && ((DDS_OP_MASK & *op) == DDS_OP_ADR));

    switch (DDS_OP_TYPE (*op))
    {
      case DDS_OP_VAL_1BY:
      {
        *dst = *src;
        dst++;
        break;
      }
      case DDS_OP_VAL_2BY:
      {
        uint16_t u16 = toBE2u (*((const uint16_t*) src));
        memcpy (dst, &u16, sizeof (u16));
        dst += sizeof (u16);
        break;
      }
      case DDS_OP_VAL_4BY:
      {
        uint32_t u32 = toBE4u (*((const uint32_t*) src));
        memcpy (dst, &u32, sizeof (u32));
        dst += sizeof (u32);
        break;
      }
      case DDS_OP_VAL_8BY:
      {
        uint64_t u64 = toBE8u (*((const uint64_t*) src));
        memcpy (dst, &u64, sizeof (u64));
        dst += sizeof (u64);
        break;
      }
      case DDS_OP_VAL_STR:
      {
        src = *((char**) src);
      } /* Fall-through intentional */
      case DDS_OP_VAL_BST:
      {
        uint32_t u32;
        len = (uint32_t) (strlen (src) + 1);
        u32 = toBE4u (len);
        memcpy (dst, &u32, sizeof (u32));
        dst += sizeof (u32);
        memcpy (dst, src, len);
        dst += len;
        break;
      }
      case DDS_OP_VAL_ARR:
      {
        uint32_t size = dds_op_size[DDS_OP_SUBTYPE (*op)];
        len = size * op[2];
        memcpy (dst, src, len);
        if (dds_stream_endian () && (size != 1u))
        {
          dds_stream_swap (dst, size, op[2]);
        }
        dst += len;
        break;
      }
      default: assert (0);
    }
  }

  /* Hash is md5 of key */

  if ((kh->m_flags & DDS_KEY_IS_HASH) == 0) 
  {
    dds_key_md5 (kh);
  }
}
