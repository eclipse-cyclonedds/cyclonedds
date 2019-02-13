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
#include "ddsi/q_bswap.h"
#include "ddsi/q_config.h"
#include "dds__stream.h"
#include "dds__key.h"
#include "dds__alloc.h"
#include "os/os.h"
#include "ddsi/q_md5.h"

//#define OP_DEBUG_READ 1
//#define OP_DEBUG_WRITE 1
//#define OP_DEBUG_KEY 1


#if defined OP_DEBUG_WRITE || defined OP_DEBUG_READ || defined OP_DEBUG_KEY
static const char * stream_op_type[11] =
{
  NULL, "1Byte", "2Byte", "4Byte", "8Byte", "String",
  "BString", "Sequence", "Array", "Union", "Struct"
};
#endif

#if OS_ENDIANNESS == OS_LITTLE_ENDIAN
#define DDS_ENDIAN true
#else
#define DDS_ENDIAN false
#endif

const uint32_t dds_op_size[5] = { 0, 1u, 2u, 4u, 8u };

static void dds_stream_write (dds_stream_t * os, const char * data, const uint32_t * ops);
static void dds_stream_read (dds_stream_t * is, char * data, const uint32_t * ops);

#define DDS_SWAP16(v) \
  ((uint16_t)(((v) >> 8) | ((v) << 8)))
#define DDS_SWAP32(v) \
  (((v) >> 24) | \
  (((v) & 0x00ff0000) >> 8) | \
  (((v) & 0x0000ff00) << 8) | \
  ((v) << 24))
#define DDS_SWAP64(v) \
  (((v)) >> 56) | \
  ((((v)) & 0x00ff000000000000) >> 40) | \
  ((((v)) & 0x0000ff0000000000) >> 24) | \
  ((((v)) & 0x000000ff00000000) >> 8) | \
  ((((v)) & 0x00000000ff000000) << 8) | \
  ((((v)) & 0x0000000000ff0000) << 24) | \
  ((((v)) & 0x000000000000ff00) << 40) | \
  (((v)) << 56)

#define DDS_CDR_ALIGN2(s) ((s)->m_index = ((s)->m_index + 1U) & ~1U)
#define DDS_CDR_ALIGN4(s) ((s)->m_index = ((s)->m_index + 3U) & ~3U)
#define DDS_CDR_ALIGN8(s) ((s)->m_index = ((s)->m_index + 7U) & ~7U)
#define DDS_CDR_ALIGNTO(s,n) ((s)->m_index = ((s)->m_index + (n-1)) & ~(n-1))
#define DDS_CDR_ALIGNED(s,n) ((n) && ((s)->m_index % (n)) == 0)

#define DDS_CDR_ADDRESS(c, type) ((type*) &((c)->m_buffer.p8[(c)->m_index]))
#define DDS_CDR_RESET(c) \
  (c)->m_index = 0ul; \
  (c)->m_failed = false;
#define DDS_CDR_RESIZE(c,l) if (((c)->m_size < ((l) + (c)->m_index))) dds_stream_grow (c, l)
#define DDS_CDR_REINIT(c,s) \
  DDS_CDR_RESET(c); \
  (c)->m_endian = DDS_ENDIAN; \
  if ((c)->m_size < (s)) dds_stream_grow (c, s)

#ifndef NDEBUG
#define DDS_IS_OK(s,n) (!((s)->m_failed = ((s)->m_index + (n)) > (s)->m_size))
#else
#define DDS_IS_OK(s,n) (true)
#endif

#define DDS_IS_GET1(s) (s)->m_buffer.p8[(s)->m_index++]

#define DDS_IS_GET2(s,v) \
  (v) = *DDS_CDR_ADDRESS ((s), uint16_t); \
  if ((s)->m_endian != DDS_ENDIAN) (v) = DDS_SWAP16 ((v)); \
  (s)->m_index += 2

#define DDS_IS_GET4(s,v,t) \
  (v) = *DDS_CDR_ADDRESS ((s), t); \
  if ((s)->m_endian != DDS_ENDIAN) (v) = (t)(DDS_SWAP32 ((uint32_t) (v))); \
  (s)->m_index += 4;

#define DDS_IS_GET8(s,v,t) \
  (v) = *DDS_CDR_ADDRESS ((s), t); \
  if ((s)->m_endian != DDS_ENDIAN) (v) = (t)(DDS_SWAP64 ((uint64_t) (v))); \
  (s)->m_index += 8

#define DDS_IS_GET_BYTES(s,b,l) \
  memcpy (b, DDS_CDR_ADDRESS ((s), void), l); \
  (s)->m_index += (l)

#define DDS_OS_PUT1(s,v) \
  DDS_CDR_RESIZE (s, 1u); \
  *DDS_CDR_ADDRESS (s, uint8_t) = v; \
  (s)->m_index += 1

#define DDS_OS_PUT2(s,v) \
  DDS_CDR_ALIGN2 (s); \
  DDS_CDR_RESIZE (s, 2u); \
  *DDS_CDR_ADDRESS (s, uint16_t) = ((s)->m_endian == DDS_ENDIAN) ? \
    v : DDS_SWAP16 ((uint16_t) (v)); \
  (s)->m_index += 2

#define DDS_OS_PUT4(s,v,t) \
  DDS_CDR_ALIGN4 (s); \
  DDS_CDR_RESIZE (s, 4u); \
  *DDS_CDR_ADDRESS (s, t) = ((s)->m_endian == DDS_ENDIAN) ? \
    v : DDS_SWAP32 ((uint32_t) (v)); \
  (s)->m_index += 4

#define DDS_OS_PUT8(s,v,t) \
  DDS_CDR_ALIGN8 (s); \
  DDS_CDR_RESIZE (s, 8u); \
  *DDS_CDR_ADDRESS (s, t) = ((s)->m_endian == DDS_ENDIAN) ? \
    v : DDS_SWAP64 ((uint64_t) (v)); \
  (s)->m_index += 8

#define DDS_OS_PUT_BYTES(s,b,l) \
  DDS_CDR_RESIZE (s, l); \
  memcpy (DDS_CDR_ADDRESS (s, void), b, l); \
  (s)->m_index += (l)

bool dds_stream_endian (void)
{
  return DDS_ENDIAN;
}

size_t dds_stream_check_optimize (_In_ const dds_topic_descriptor_t * desc)
{
  dds_stream_t os;
  void * sample = dds_alloc (desc->m_size);
  uint8_t * ptr1;
  uint8_t * ptr2;
  uint32_t size = desc->m_size;
  uint8_t val = 1;

  dds_stream_init (&os, size);
  ptr1 = (uint8_t*) sample;
  ptr2 = os.m_buffer.p8;
  while (size--)
  {
    *ptr1++ = val;
    *ptr2++ = val++;
  }

  dds_stream_write (&os, sample, desc->m_ops);
  size = (memcmp (sample, os.m_buffer.p8, desc->m_size) == 0) ? os.m_index : 0;

  dds_sample_free_contents (sample, desc->m_ops);
  dds_free (sample);
  dds_stream_fini (&os);
  DDS_TRACE("Marshalling for type: %s is%s optimised\n", desc->m_typename, size ? "" : " not");
  return size;
}

dds_stream_t * dds_stream_create (uint32_t size)
{
  dds_stream_t * stream = (dds_stream_t*) dds_alloc (sizeof (*stream));
  dds_stream_init (stream, size);
  return stream;
}

void dds_stream_delete (dds_stream_t * st)
{
  dds_stream_fini (st);
  dds_free (st);
}

void dds_stream_fini (dds_stream_t * st)
{
  if (st->m_size)
  {
    dds_free (st->m_buffer.p8);
  }
}

void dds_stream_init (dds_stream_t * st, uint32_t size)
{
  memset (st, 0, sizeof (*st));
  DDS_CDR_REINIT (st, size);
}

void dds_stream_reset (dds_stream_t * st)
{
  DDS_CDR_RESET (st);
}

void dds_stream_grow (dds_stream_t * st, uint32_t size)
{
  uint32_t needed = size + st->m_index;

  /* Reallocate on 4k boundry */

  uint32_t newSize = (needed & ~(uint32_t)0xfff) + 0x1000;
  uint8_t * old = st->m_buffer.p8;

  st->m_buffer.p8 = dds_realloc (old, newSize);
  memset (st->m_buffer.p8 + st->m_size, 0, newSize - st->m_size);
  st->m_size = newSize;
}

bool dds_stream_read_bool (dds_stream_t * is)
{
  return (dds_stream_read_uint8 (is) != 0);
}

uint8_t dds_stream_read_uint8 (dds_stream_t * is)
{
  return DDS_IS_OK (is, 1) ? DDS_IS_GET1 (is) : 0;
}

uint16_t dds_stream_read_uint16 (dds_stream_t * is)
{
  uint16_t val = 0;
  DDS_CDR_ALIGN2 (is);
  if (DDS_IS_OK (is, 2))
  {
    DDS_IS_GET2 (is, val);
  }
  return val;
}

uint32_t dds_stream_read_uint32 (dds_stream_t * is)
{
  uint32_t val = 0;
  DDS_CDR_ALIGN4 (is);
  if (DDS_IS_OK (is, 4))
  {
    DDS_IS_GET4 (is, val, uint32_t);
  }
  return val;
}

uint64_t dds_stream_read_uint64 (dds_stream_t * is)
{
  uint64_t val = 0;
  DDS_CDR_ALIGN8 (is);
  if (DDS_IS_OK (is, 8))
  {
    DDS_IS_GET8 (is, val, uint64_t);
  }
  return val;
}

extern inline char dds_stream_read_char (dds_stream_t *is);
extern inline int8_t dds_stream_read_int8 (dds_stream_t *is);
extern inline int16_t dds_stream_read_int16 (dds_stream_t *is);
extern inline int32_t dds_stream_read_int32 (dds_stream_t *is);
extern inline int64_t dds_stream_read_int64 (dds_stream_t *is);

float dds_stream_read_float (dds_stream_t * is)
{
  float val = 0.0;
  DDS_CDR_ALIGN4 (is);
  if (DDS_IS_OK (is, 4))
  {
    DDS_IS_GET4 (is, val, float);
  }
  return val;
}

double dds_stream_read_double (dds_stream_t * is)
{
  double val = 0.0;
  DDS_CDR_ALIGN8 (is);
  if (DDS_IS_OK (is, 8))
  {
    DDS_IS_GET8 (is, val, double);
  }
  return val;
}

char * dds_stream_reuse_string
  (dds_stream_t * is, char * str, const uint32_t bound)
{
  uint32_t length;
  void * src;

  DDS_CDR_ALIGN4 (is);
  if (DDS_IS_OK (is, 4))
  {
    DDS_IS_GET4 (is, length, uint32_t);
    if (DDS_IS_OK (is, length))
    {
      src = DDS_CDR_ADDRESS (is, void);
      if (bound)
      {
        memcpy (str, src, length > bound ? bound : length);
      }
      else
      {
        if ((str == NULL) || (strlen (str) + 1 < length))
        {
          str = dds_realloc (str, length);
        }
        memcpy (str, src, length);
      }
      is->m_index += length;
    }
  }

  return str;
}

char * dds_stream_read_string (dds_stream_t * is)
{
  return dds_stream_reuse_string (is, NULL, 0);
}

void dds_stream_swap (void * buff, uint32_t size, uint32_t num)
{
  assert (size == 2 || size == 4 || size == 8);

  switch (size)
  {
    case 2:
    {
      uint16_t * ptr = (uint16_t*) buff;
      while (num--)
      {
        *ptr = DDS_SWAP16 (*ptr);
        ptr++;
      }
      break;
    }
    case 4:
    {
      uint32_t * ptr = (uint32_t*) buff;
      while (num--)
      {
        *ptr = DDS_SWAP32 (*ptr);
        ptr++;
      }
      break;
    }
    default:
    {
      uint64_t * ptr = (uint64_t*) buff;
      while (num--)
      {
        *ptr = DDS_SWAP64 (*ptr);
        ptr++;
      }
      break;
    }
  }
}

static void dds_stream_read_fixed_buffer
  (dds_stream_t * is, void * buff, uint32_t len, const uint32_t size, const bool swap)
{
  if (size && len)
  {
    DDS_CDR_ALIGNTO (is, size);
    DDS_IS_GET_BYTES (is, buff, len * size);
    if (swap && (size > 1))
    {
      dds_stream_swap (buff, size, len);
    }
  }
}

void dds_stream_read_buffer (dds_stream_t * is, uint8_t * buffer, uint32_t len)
{
  if (DDS_IS_OK (is, len))
  {
    DDS_IS_GET_BYTES (is, buffer, len);
  }
}

void dds_stream_read_sample (dds_stream_t * is, void * data, const struct ddsi_sertopic_default * topic)
{
  const struct dds_topic_descriptor * desc = topic->type;
  /* Check if can copy directly from stream buffer */
  if (topic->opt_size && DDS_IS_OK (is, desc->m_size) && (is->m_endian == DDS_ENDIAN))
  {
    DDS_IS_GET_BYTES (is, data, desc->m_size);
  }
  else
  {
    dds_stream_read (is, data, desc->m_ops);
  }
}

void dds_stream_write_bool (dds_stream_t * os, bool val)
{
  dds_stream_write_uint8 (os, val ? 1 : 0);
}

void dds_stream_write_uint8 (dds_stream_t * os, uint8_t val)
{
  DDS_OS_PUT1 (os, val);
}

void dds_stream_write_uint16 (dds_stream_t * os, uint16_t val)
{
  DDS_OS_PUT2 (os, val);
}

void dds_stream_write_uint32 (dds_stream_t * os, uint32_t val)
{
  DDS_OS_PUT4 (os, val, uint32_t);
}

void dds_stream_write_uint64 (dds_stream_t * os, uint64_t val)
{
  DDS_OS_PUT8 (os, val, uint64_t);
}

extern inline void dds_stream_write_char (dds_stream_t * os, char val);
extern inline void dds_stream_write_int8 (dds_stream_t * os, int8_t val);
extern inline void dds_stream_write_int16 (dds_stream_t * os, int16_t val);
extern inline void dds_stream_write_int32 (dds_stream_t * os, int32_t val);
extern inline void dds_stream_write_int64 (dds_stream_t * os, int64_t val);

void dds_stream_write_float (dds_stream_t * os, float val)
{
  union { float f; uint32_t u; } u;
  u.f = val;
  dds_stream_write_uint32 (os, u.u);
}

void dds_stream_write_double (dds_stream_t * os, double val)
{
  union { double f; uint64_t u; } u;
  u.f = val;
  dds_stream_write_uint64 (os, u.u);
}

void dds_stream_write_string (dds_stream_t * os, const char * val)
{
  uint32_t size = 1;

  if (val)
  {
    size += (uint32_t)strlen (val); /* Type casting is done for the warning of conversion from 'size_t' to 'uint32_t', which may cause possible loss of data */
  }

  DDS_OS_PUT4 (os, size, uint32_t);

  if (val)
  {
    DDS_OS_PUT_BYTES (os, (uint8_t*) val, size);
  }
  else
  {
    DDS_OS_PUT1 (os, 0U);
  }
}

void dds_stream_write_buffer (dds_stream_t * os, uint32_t len, const uint8_t * buffer)
{
  DDS_OS_PUT_BYTES (os, buffer, len);
}

void *dds_stream_address (dds_stream_t * s)
{
  return DDS_CDR_ADDRESS(s, void);
}

void *dds_stream_alignto (dds_stream_t * s, uint32_t a)
{
  DDS_CDR_ALIGNTO (s, a);
  return DDS_CDR_ADDRESS (s, void);
}

static void dds_stream_write
(
  dds_stream_t * os,
  const char * data,
  const uint32_t * ops
)
{
  uint32_t align;
  uint32_t op;
  uint32_t type;
  uint32_t subtype;
  uint32_t num;
  const char * addr;

  while ((op = *ops) != DDS_OP_RTS)
  {
    switch (DDS_OP_MASK & op)
    {
      case DDS_OP_ADR:
      {
        type = DDS_OP_TYPE (op);
#ifdef OP_DEBUG_WRITE
        DDS_TRACE("W-ADR: %s offset %d\n", stream_op_type[type], ops[1]);
#endif
        addr = data + ops[1];
        ops += 2;
        switch (type)
        {
          case DDS_OP_VAL_1BY:
          {
            DDS_OS_PUT1 (os, *(uint8_t*) addr);
            break;
          }
          case DDS_OP_VAL_2BY:
          {
            DDS_OS_PUT2 (os, *(uint16_t*) addr);
            break;
          }
          case DDS_OP_VAL_4BY:
          {
            DDS_OS_PUT4 (os, *(uint32_t*) addr, uint32_t);
            break;
          }
          case DDS_OP_VAL_8BY:
          {
            DDS_OS_PUT8 (os, *(uint64_t*) addr, uint64_t);
            break;
          }
          case DDS_OP_VAL_STR:
          {
#ifdef OP_DEBUG_WRITE
            DDS_TRACE("W-STR: %s\n", *((char**) addr));
#endif
            dds_stream_write_string (os, *((char**) addr));
            break;
          }
          case DDS_OP_VAL_SEQ:
          {
            dds_sequence_t * seq = (dds_sequence_t*) addr;
            subtype = DDS_OP_SUBTYPE (op);
            num = seq->_length;

#ifdef OP_DEBUG_WRITE
            DDS_TRACE("W-SEQ: %s <%d>\n", stream_op_type[subtype], num);
#endif
            DDS_OS_PUT4 (os, num, uint32_t);
            if (num || (subtype > DDS_OP_VAL_STR))
            {
              switch (subtype)
              {
                case DDS_OP_VAL_1BY:
                case DDS_OP_VAL_2BY:
                case DDS_OP_VAL_4BY:
                {
                  num = num * dds_op_size[subtype];
                  DDS_OS_PUT_BYTES (os, seq->_buffer, num);
                  break;
                }
                case DDS_OP_VAL_8BY:
                {
                  DDS_CDR_ALIGN8 (os);
                  DDS_OS_PUT_BYTES (os, seq->_buffer, num * 8u);
                  break;
                }
                case DDS_OP_VAL_STR:
                {
                  char ** ptr = (char**) seq->_buffer;
                  while (num--)
                  {
#ifdef OP_DEBUG_WRITE
                    DDS_TRACE("W-SEQ STR: %s\n", *ptr);
#endif
                    dds_stream_write_string (os, *ptr);
                    ptr++;
                  }
                  break;
                }
                case DDS_OP_VAL_BST:
                {
                  char * ptr = (char*) seq->_buffer;
                  align = *ops++;
                  while (num--)
                  {
#ifdef OP_DEBUG_WRITE
                    DDS_TRACE("W-SEQ BST[%d]: %s\n", align, ptr);
#endif
                    dds_stream_write_string (os, ptr);
                    ptr += align;
                  }
                  break;
                }
                default:
                {
                  const uint32_t elem_size = *ops++;
                  const uint32_t * jsr_ops = ops + DDS_OP_ADR_JSR (*ops) - 3;
                  const uint32_t jmp = DDS_OP_ADR_JMP (*ops);
                  char * ptr = (char*) seq->_buffer;
                  while (num--)
                  {
                    dds_stream_write (os, ptr, jsr_ops);
                    ptr += elem_size;
                  }
                  ops += jmp ? (jmp - 3) : 1;
                  break;
                }
              }
            }
            break;
          }
          case DDS_OP_VAL_ARR:
          {
            subtype = DDS_OP_SUBTYPE (op);
            num = *ops++;

#ifdef OP_DEBUG_WRITE
            DDS_TRACE("W-ARR: %s [%d]\n", stream_op_type[subtype], num);
#endif
            switch (subtype)
            {
              case DDS_OP_VAL_1BY:
              case DDS_OP_VAL_2BY:
              case DDS_OP_VAL_4BY:
              case DDS_OP_VAL_8BY:
              {
                align = dds_op_size[subtype];
                DDS_CDR_ALIGNTO (os, align);
                DDS_OS_PUT_BYTES (os, addr, num * align);
                break;
              }
              case DDS_OP_VAL_STR:
              {
                char ** ptr = (char**) addr;
                while (num--)
                {
                  dds_stream_write_string (os, *ptr);
                  ptr++;
                }
                break;
              }
              case DDS_OP_VAL_BST:
              {
                char * ptr = (char*) addr;
                align = ops[1];
                while (num--)
                {
                  dds_stream_write_string (os, ptr);
                  ptr += align;
                }
                ops += 2;
                break;
              }
              default:
              {
                const uint32_t * jsr_ops = ops + DDS_OP_ADR_JSR (*ops) - 3;
                const uint32_t jmp = DDS_OP_ADR_JMP (*ops);
                const uint32_t elem_size = ops[1];

                while (num--)
                {
                  dds_stream_write (os, addr, jsr_ops);
                  addr += elem_size;
                }
                ops += jmp ? (jmp - 3) : 2;
                break;
              }
            }
            break;
          }
          case DDS_OP_VAL_UNI:
          {
            const bool has_default = op & DDS_OP_FLAG_DEF;
            subtype = DDS_OP_SUBTYPE (op);
            num = ops[0];
            const uint32_t * jeq_op = ops + DDS_OP_ADR_JSR (ops[1]) - 2;
            uint32_t disc = 0;

            assert (subtype <= DDS_OP_VAL_4BY);

            /* Write discriminant */

            switch (subtype)
            {
              case DDS_OP_VAL_1BY:
              {
                uint8_t d8 = *((uint8_t*) addr);
                DDS_OS_PUT1 (os, d8);
                disc = d8;
                break;
              }
              case DDS_OP_VAL_2BY:
              {
                uint16_t d16 = *((uint16_t*) addr);
                DDS_OS_PUT2 (os, d16);
                disc = d16;
                break;
              }
              case DDS_OP_VAL_4BY:
              {
                disc = *((uint32_t*) addr);
                DDS_OS_PUT4 (os, disc, uint32_t);
                break;
              }
              default: assert (0);
            }
#ifdef OP_DEBUG_WRITE
            DDS_TRACE("W-UNI: switch %s case %d/%d\n", stream_op_type[subtype], disc, num);
#endif

            /* Write case matching discriminant */

            while (num--)
            {
              assert ((DDS_OP_MASK & jeq_op[0]) == DDS_OP_JEQ);

              /* Select matching or default case */

              if ((jeq_op[1] == disc) || (has_default && (num == 0)))
              {
                subtype = DDS_JEQ_TYPE (jeq_op[0]);
                addr = data + jeq_op[2];

#ifdef OP_DEBUG_WRITE
                DDS_TRACE("W-UNI: case type %s\n", stream_op_type[subtype]);
#endif
                switch (subtype)
                {
                  case DDS_OP_VAL_1BY:
                  case DDS_OP_VAL_2BY:
                  case DDS_OP_VAL_4BY:
                  case DDS_OP_VAL_8BY:
                  {
                    align = dds_op_size[subtype];
                    DDS_CDR_ALIGNTO (os, align);
                    DDS_OS_PUT_BYTES (os, addr, align);
                    break;
                  }
                  case DDS_OP_VAL_STR:
                  {
                    dds_stream_write_string (os, *(char**) addr);
                    break;
                  }
                  case DDS_OP_VAL_BST:
                  {
                    dds_stream_write_string (os, (char*) addr);
                    break;
                  }
                  default:
                  {
                    dds_stream_write (os, addr, jeq_op + DDS_OP_ADR_JSR (jeq_op[0]));
                    break;
                  }
                }
                break;
              }
              jeq_op += 3;
            }

            /* Jump to next instruction */

            ops += DDS_OP_ADR_JMP (ops[1]) - 2;
            break;
          }
          case DDS_OP_VAL_BST:
          {
#ifdef OP_DEBUG_WRITE
            DDS_TRACE("W-BST: %s\n", (char*) addr);
#endif
            dds_stream_write_string (os, (char*) addr);
            ops++;
            break;
          }
          default: assert (0);
        }
        break;
      }
      case DDS_OP_JSR: /* Implies nested type */
      {
#ifdef OP_DEBUG_WRITE
        DDS_TRACE("W-JSR: %d\n", DDS_OP_JUMP (op));
#endif
        dds_stream_write (os, data, ops + DDS_OP_JUMP (op));
        ops++;
        break;
      }
      default: assert (0);
    }
  }
#ifdef OP_DEBUG_WRITE
  DDS_TRACE("W-RTS:\n");
#endif
}

static void dds_stream_read (dds_stream_t * is, char * data, const uint32_t * ops)
{
  uint32_t align;
  uint32_t op;
  uint32_t type;
  uint32_t subtype;
  uint32_t num;
  char * addr;

  while ((op = *ops) != DDS_OP_RTS)
  {
    switch (DDS_OP_MASK & op)
    {
      case DDS_OP_ADR:
      {
        type = DDS_OP_TYPE (op);
#ifdef OP_DEBUG_READ
        DDS_TRACE("R-ADR: %s offset %d\n", stream_op_type[type], ops[1]);
#endif
        addr = data + ops[1];
        ops += 2;
        switch (type)
        {
          case DDS_OP_VAL_1BY:
          {
            *(uint8_t*) addr = dds_stream_read_uint8 (is);
            break;
          }
          case DDS_OP_VAL_2BY:
          {
            *(uint16_t*) addr = dds_stream_read_uint16 (is);
            break;
          }
          case DDS_OP_VAL_4BY:
          {
            *(uint32_t*) addr = dds_stream_read_uint32 (is);
            break;
          }
          case DDS_OP_VAL_8BY:
          {
            *(uint64_t*) addr = dds_stream_read_uint64 (is);
            break;
          }
          case DDS_OP_VAL_STR:
          {
#ifdef OP_DEBUG_READ
            DDS_TRACE("R-STR: @ %p\n", addr);
#endif
            *(char**) addr = dds_stream_reuse_string (is, *((char**) addr), 0);
            break;
          }
          case DDS_OP_VAL_SEQ:
          {
            dds_sequence_t * seq = (dds_sequence_t*) addr;
            subtype = DDS_OP_SUBTYPE (op);
            num = dds_stream_read_uint32 (is);

#ifdef OP_DEBUG_READ
            DDS_TRACE("R-SEQ: %s <%d>\n", stream_op_type[subtype], num);
#endif
            /* Maintain max sequence length (may not have been set by caller) */

            if (seq->_length > seq->_maximum)
            {
              seq->_maximum = seq->_length;
            }

            switch (subtype)
            {
              case DDS_OP_VAL_1BY:
              case DDS_OP_VAL_2BY:
              case DDS_OP_VAL_4BY:
              case DDS_OP_VAL_8BY:
              {
                align = dds_op_size[subtype];

                /* Reuse sequence buffer if big enough */

                if (num > seq->_length)
                {
                  if (seq->_release && seq->_length)
                  {
                    seq->_buffer = dds_realloc_zero (seq->_buffer, num * align);
                  }
                  else
                  {
                    seq->_buffer = dds_alloc (num * align);
                  }
                  seq->_release = true;
                  seq->_maximum = num;
                }
                seq->_length = num;
                dds_stream_read_fixed_buffer (is, seq->_buffer, seq->_length, align, is->m_endian != DDS_ENDIAN);
                break;
              }
              case DDS_OP_VAL_STR:
              {
                char ** ptr;

                /* Reuse sequence buffer if big enough */

                if (num > seq->_maximum)
                {
                  if (seq->_release && seq->_maximum)
                  {
                    seq->_buffer = dds_realloc_zero (seq->_buffer, num * sizeof (char*));
                  }
                  else
                  {
                    seq->_buffer = dds_alloc (num * sizeof (char*));
                  }
                  seq->_release = true;
                  seq->_maximum = num;
                }
                seq->_length = num;

                ptr = (char**) seq->_buffer;
                while (num--)
                {
                  *ptr = dds_stream_reuse_string (is, *ptr, 0);
                  ptr++;
                }
                break;
              }
              case DDS_OP_VAL_BST:
              {
                char * ptr;
                align = *ops++;

                /* Reuse sequence buffer if big enough */

                if (num > seq->_maximum)
                {
                  if (seq->_release && seq->_maximum)
                  {
                    seq->_buffer = dds_realloc_zero (seq->_buffer, num * align);
                  }
                  else
                  {
                    seq->_buffer = dds_alloc (num * align);
                  }
                  seq->_release = true;
                  seq->_maximum = num;
                }
                seq->_length = num;

                ptr = (char*) seq->_buffer;
                while (num--)
                {
                  dds_stream_reuse_string (is, ptr, align);
                  ptr += align;
                }
                break;
              }
              default:
              {
                const uint32_t elem_size = *ops++;
                const uint32_t * jsr_ops = ops + DDS_OP_ADR_JSR (*ops) - 3;
                const uint32_t jmp = DDS_OP_ADR_JMP (*ops);
                uint32_t i;
                char * ptr;

                /* Reuse sequence buffer if big enough */

                if (num > seq->_maximum)
                {
                  if (seq->_release && seq->_maximum)
                  {
                    if (seq->_buffer)
                    {
                      i = seq->_length;
                      ptr = (char*) seq->_buffer;
                      while (i--)
                      {
                        dds_sample_free_contents (ptr, jsr_ops);
                        ptr += elem_size;
                      }
                    }
                    seq->_buffer = dds_realloc_zero (seq->_buffer, num * elem_size);
                  }
                  else
                  {
                    seq->_buffer = dds_alloc (num * elem_size);
                  }
                  seq->_release = true;
                  seq->_maximum = num;
                }
                seq->_length = num;

                ptr = (char*) seq->_buffer;
                while (num--)
                {
                  dds_stream_read (is, ptr, jsr_ops);
                  ptr += elem_size;
                }
                ops += jmp ? (jmp - 3) : 1;
                break;
              }
            }
            break;
          }
          case DDS_OP_VAL_ARR:
          {
            subtype = DDS_OP_SUBTYPE (op);
            num = *ops++;

#ifdef OP_DEBUG_READ
            DDS_TRACE("R-ARR: %s [%d]\n", stream_op_type[subtype], num);
#endif
            switch (subtype)
            {
              case DDS_OP_VAL_1BY:
              case DDS_OP_VAL_2BY:
              case DDS_OP_VAL_4BY:
              case DDS_OP_VAL_8BY:
              {
                align = dds_op_size[subtype];
                if (DDS_IS_OK (is, num * align))
                {
                  dds_stream_read_fixed_buffer (is, addr, num, align, is->m_endian != DDS_ENDIAN);
                }
                break;
              }
              case DDS_OP_VAL_STR:
              {
                char ** ptr = (char**) addr;
                while (num--)
                {
                  *ptr = dds_stream_reuse_string (is, *ptr, 0);
                  ptr++;
                }
                break;
              }
              case DDS_OP_VAL_BST:
              {
                char * ptr = (char*) addr;
                align = ops[1];
                while (num--)
                {
                  dds_stream_reuse_string (is, ptr, align);
                  ptr += align;
                }
                ops += 2;
                break;
              }
              default:
              {
                const uint32_t * jsr_ops = ops + DDS_OP_ADR_JSR (*ops) - 3;
                const uint32_t jmp = DDS_OP_ADR_JMP (*ops);
                const uint32_t elem_size = ops[1];

                while (num--)
                {
                  dds_stream_read (is, addr, jsr_ops);
                  addr += elem_size;
                }
                ops += jmp ? (jmp - 3) : 2;
                break;
              }
            }
            break;
          }
          case DDS_OP_VAL_UNI:
          {
            const bool has_default = op & DDS_OP_FLAG_DEF;
            subtype = DDS_OP_SUBTYPE (op);
            num = ops[0];
            const uint32_t * jeq_op = ops + DDS_OP_ADR_JSR (ops[1]) - 2;
            uint32_t disc = 0;

            assert (subtype <= DDS_OP_VAL_4BY);

            /* Read discriminant */

            switch (subtype)
            {
              case DDS_OP_VAL_1BY:
              {
                uint8_t d8 = dds_stream_read_uint8 (is);
                *(uint8_t*) addr = d8;
                disc = d8;
                break;
              }
              case DDS_OP_VAL_2BY:
              {
                uint16_t d16 = dds_stream_read_uint16 (is);
                *(uint16_t*) addr = d16;
                disc = d16;
                break;
              }
              case DDS_OP_VAL_4BY:
              {
                disc = dds_stream_read_uint32 (is);
                *(uint32_t*) addr = disc;
                break;
              }
              default: assert (0);
            }

#ifdef OP_DEBUG_READ
            DDS_TRACE("R-UNI: switch %s case %d/%d\n", stream_op_type[subtype], disc, num);
#endif

            /* Read case matching discriminant */

            while (num--)
            {
              assert ((DDS_OP_MASK & jeq_op[0]) == DDS_OP_JEQ);
              if ((jeq_op[1] == disc) || (has_default && (num == 0)))
              {
                subtype = DDS_JEQ_TYPE (jeq_op[0]);
                addr = data + jeq_op[2];

#ifdef OP_DEBUG_READ
                DDS_TRACE("R-UNI: case type %s\n", stream_op_type[subtype]);
#endif
                switch (subtype)
                {
                  case DDS_OP_VAL_1BY:
                  {
                    *(uint8_t*) addr = dds_stream_read_uint8 (is);
                    break;
                  }
                  case DDS_OP_VAL_2BY:
                  {
                    *(uint16_t*) addr = dds_stream_read_uint16 (is);
                    break;
                  }
                  case DDS_OP_VAL_4BY:
                  {
                    *(uint32_t*) addr = dds_stream_read_uint32 (is);
                    break;
                  }
                  case DDS_OP_VAL_8BY:
                  {
                    *(uint64_t*) addr = dds_stream_read_uint64 (is);
                    break;
                  }
                  case DDS_OP_VAL_STR:
                  {
                    *(char**) addr = dds_stream_reuse_string (is, *((char**) addr), 0);
                    break;
                  }
                  default:
                  {
                    dds_stream_read (is, addr, jeq_op + DDS_OP_ADR_JSR (jeq_op[0]));
                    break;
                  }
                }
                break;
              }
              jeq_op += 3;
            }

            /* Jump to next instruction */

            ops += DDS_OP_ADR_JMP (ops[1]) - 2;
            break;
          }
          case DDS_OP_VAL_BST:
          {
#ifdef OP_DEBUG_READ
            DDS_TRACE("R-BST: @ %p\n", addr);
#endif
            dds_stream_reuse_string (is, (char*) addr, *ops);
            ops++;
            break;
          }
          default: assert (0);
        }
        break;
      }
      case DDS_OP_JSR: /* Implies nested type */
      {
#ifdef OP_DEBUG_READ
        DDS_TRACE("R-JSR: %d\n", DDS_OP_JUMP (op));
#endif
        dds_stream_read (is, data, ops + DDS_OP_JUMP (op));
        ops++;
        break;
      }
      default: assert (0);
    }
  }
#ifdef OP_DEBUG_READ
  DDS_TRACE("R-RTS:\n");
#endif
}

void dds_stream_write_sample (dds_stream_t * os, const void * data, const struct ddsi_sertopic_default * topic)
{
  const struct dds_topic_descriptor * desc = topic->type;

  if (topic->opt_size && DDS_CDR_ALIGNED (os, desc->m_align))
  {
    DDS_OS_PUT_BYTES (os, data, desc->m_size);
  }
  else
  {
    dds_stream_write (os, data, desc->m_ops);
  }
}

void dds_stream_from_serdata_default (_Out_ dds_stream_t * s, _In_ const struct ddsi_serdata_default *d)
{
  s->m_failed = false;
  s->m_buffer.p8 = (uint8_t*) d;
  s->m_index = (uint32_t) offsetof (struct ddsi_serdata_default, data);
  s->m_size = d->size + s->m_index;
  assert (d->hdr.identifier == CDR_LE || d->hdr.identifier == CDR_BE);
  s->m_endian = (d->hdr.identifier == CDR_LE);
}

void dds_stream_add_to_serdata_default (dds_stream_t * s, struct ddsi_serdata_default **d)
{
  /* DDSI requires 4 byte alignment */

  DDS_CDR_ALIGN4 (s);

  /* Reset data pointer as stream may have reallocated */

  (*d) = s->m_buffer.pv;
  (*d)->pos = (s->m_index - (uint32_t)offsetof (struct ddsi_serdata_default, data));
  (*d)->size = (s->m_size - (uint32_t)offsetof (struct ddsi_serdata_default, data));
}

void dds_stream_write_key (dds_stream_t * os, const char * sample, const struct ddsi_sertopic_default * topic)
{
  const struct dds_topic_descriptor * desc = (const struct dds_topic_descriptor *) topic->type;
  uint32_t i;
  const char * src;
  const uint32_t * op;

  for (i = 0; i < desc->m_nkeys; i++)
  {
    op = desc->m_ops + desc->m_keys[i].m_index;
    src = sample + op[1];
    assert ((*op & DDS_OP_FLAG_KEY) && ((DDS_OP_MASK & *op) == DDS_OP_ADR));
    switch (DDS_OP_TYPE (*op))
    {
      case DDS_OP_VAL_1BY:
        DDS_OS_PUT1 (os, *((uint8_t*) src));
        break;
      case DDS_OP_VAL_2BY:
        DDS_OS_PUT2 (os, *((uint16_t*) src));
        break;
      case DDS_OP_VAL_4BY:
        DDS_OS_PUT4 (os, *((uint32_t*) src), uint32_t);
        break;
      case DDS_OP_VAL_8BY:
        DDS_OS_PUT8 (os, *((uint64_t*) src), uint64_t);
        break;
      case DDS_OP_VAL_STR:
        src = *(char**) src;
        /* FALLS THROUGH */
      case DDS_OP_VAL_BST:
        dds_stream_write_string (os, src);
        break;
      case DDS_OP_VAL_ARR:
      {
        uint32_t subtype = DDS_OP_SUBTYPE (*op);
        assert (subtype <= DDS_OP_VAL_8BY);
        uint32_t align = dds_op_size[subtype];
        DDS_CDR_ALIGNTO (os, align);
        DDS_OS_PUT_BYTES (os, src, op[2] * align);
        break;
      }
      default: assert (0);
    }
  }
}

/*
  dds_stream_get_keyhash: Extract key values from a stream and generate
  keyhash used for instance identification. Non key fields are skipped.
  Key hash data is big endian CDR encoded with no padding. Returns length
  of key hash. Input stream may contain full sample of just key data.
*/

uint32_t dds_stream_extract_key (dds_stream_t *is, dds_stream_t *os, const uint32_t *ops, const bool just_key)
{
  uint32_t align;
  uint32_t op;
  uint32_t type;
  uint32_t subtype;
  uint32_t num;
  uint32_t len;
  const uint32_t origin = os->m_index;
  bool is_key;
  bool have_data;

  while ((op = *ops) != DDS_OP_RTS)
  {
    switch (DDS_OP_MASK & op)
    {
      case DDS_OP_ADR:
      {
        type = DDS_OP_TYPE (op);
        is_key = (op & DDS_OP_FLAG_KEY) && (os != NULL);
        have_data = is_key || !just_key;
        ops += 2;
        if (type <= DDS_OP_VAL_8BY)
        {
          if (have_data)
          {
            align = dds_op_size[type];
            DDS_CDR_ALIGNTO (is, align);

            /* Quick skip for basic types that are not keys */

            if (! is_key)
            {
              is->m_index += align;
              break;
            }
          }
          else
          {
            break;
          }
        }
#ifdef OP_DEBUG_KEY
        if (is_key)
        {
          DDS_TRACE("K-ADR: %s\n", stream_op_type[type]);
        }
#endif
        switch (type)
        {
          case DDS_OP_VAL_1BY:
          {
            uint8_t v = DDS_IS_GET1 (is);
            DDS_OS_PUT1 (os, v);
            break;
          }
          case DDS_OP_VAL_2BY:
          {
            uint16_t v;
            DDS_IS_GET2 (is, v);
            DDS_OS_PUT2 (os, v);
            break;
          }
          case DDS_OP_VAL_4BY:
          {
            uint32_t v;
            DDS_IS_GET4 (is, v, uint32_t);
            DDS_OS_PUT4 (os, v, uint32_t);
            break;
          }
          case DDS_OP_VAL_8BY:
          {
            uint64_t v;
            DDS_IS_GET8 (is, v, uint64_t);
            DDS_OS_PUT8 (os, v, uint64_t);
            break;
          }
          case DDS_OP_VAL_STR:
          case DDS_OP_VAL_BST:
          {
            if (have_data)
            {
              len = dds_stream_read_uint32 (is);
              if (is_key)
              {
                DDS_OS_PUT4 (os, len, uint32_t);
                DDS_OS_PUT_BYTES(os, DDS_CDR_ADDRESS (is, void), len);
#ifdef OP_DEBUG_KEY
                DDS_TRACE("K-ADR: String/BString (%d)\n", len);
#endif
              }
              is->m_index += len;
            }
            if (type == DDS_OP_VAL_BST)
            {
              ops++;
            }
            break;
          }
          case DDS_OP_VAL_SEQ:
          {
            assert (! is_key);
            subtype = DDS_OP_SUBTYPE (op);
            num = have_data ? dds_stream_read_uint32 (is) : 0;

            if (num || (subtype > DDS_OP_VAL_STR))
            {
              switch (subtype)
              {
                case DDS_OP_VAL_1BY:
                case DDS_OP_VAL_2BY:
                case DDS_OP_VAL_4BY:
                case DDS_OP_VAL_8BY:
                {
                  align = dds_op_size[subtype];
                  DDS_CDR_ALIGNTO (is, align);
                  is->m_index += align * num;
                  break;
                }
                case DDS_OP_VAL_STR:
                case DDS_OP_VAL_BST:
                {
                  while (num--)
                  {
                    len = dds_stream_read_uint32 (is);
                    is->m_index += len;
                  }
                  if (subtype == DDS_OP_VAL_BST)
                  {
                    ops++;
                  }
                  break;
                }
                default:
                {
                  const uint32_t * jsr_ops = ops + DDS_OP_ADR_JSR (ops[1]) - 2;
                  const uint32_t jmp = DDS_OP_ADR_JMP (ops[1]);
                  while (num--)
                  {
                    dds_stream_extract_key (is, NULL, jsr_ops, just_key);
                  }
                  ops += jmp ? (jmp - 2) : 2;
                  break;
                }
              }
            }
            break;
          }
          case DDS_OP_VAL_ARR:
          {
            subtype = DDS_OP_SUBTYPE (op);
            assert (! is_key || subtype <= DDS_OP_VAL_8BY);
            num = have_data ? *ops : 0;
            ops++;

#ifdef OP_DEBUG_KEY
            if (is_key)
            {
              DDS_TRACE("K-ADR: %s[%d]\n", stream_op_type[subtype], num);
            }
#endif
            switch (subtype)
            {
              case DDS_OP_VAL_1BY:
              case DDS_OP_VAL_2BY:
              case DDS_OP_VAL_4BY:
              case DDS_OP_VAL_8BY:
              {
                if (num)
                {
                  align = dds_op_size[subtype];
                  if (is_key)
                  {
                    char *dst;
                    DDS_CDR_ALIGNTO (os, align);
                    DDS_CDR_RESIZE (os, num * align);
                    dst = DDS_CDR_ADDRESS(os, char);
                    dds_stream_read_fixed_buffer (is, dst, num, align, is->m_endian);
                    os->m_index += num * align;
                  }
                  is->m_index += num * align;
                }
                break;
              }
              case DDS_OP_VAL_STR:
              case DDS_OP_VAL_BST:
              {
                while (num--)
                {
                  len = dds_stream_read_uint32 (is);
                  is->m_index += len;
                }
                break;
              }
              default:
              {
                const uint32_t * jsr_ops = ops + DDS_OP_ADR_JSR (*ops) - 3;
                const uint32_t jmp = DDS_OP_ADR_JMP (*ops);
                while (num--)
                {
                  dds_stream_extract_key (is, NULL, jsr_ops, just_key);
                }
                ops += jmp ? (jmp - 3) : 2;
                break;
              }
            }
            break;
          }
          case DDS_OP_VAL_UNI:
          {
            const bool has_default = op & DDS_OP_FLAG_DEF;
            subtype = DDS_OP_SUBTYPE (op);
            num = ops[0];
            const uint32_t * jeq_op = ops + DDS_OP_ADR_JSR (ops[1]) - 2;
            uint32_t disc = 0;

            assert (subtype <= DDS_OP_VAL_4BY);
            assert (! is_key);

#ifdef OP_DEBUG_KEY
            DDS_TRACE("K-UNI: switch %s cases %d\n", stream_op_type[subtype], num);
#endif
            /* Read discriminant */

            if (have_data)
            {
              switch (subtype)
              {
                case DDS_OP_VAL_1BY:
                {
                  disc = dds_stream_read_uint8 (is);
                  break;
                }
                case DDS_OP_VAL_2BY:
                {
                  disc = dds_stream_read_uint16 (is);
                  break;
                }
                case DDS_OP_VAL_4BY:
                {
                  disc = dds_stream_read_uint32 (is);
                  break;
                }
                default: assert (0);
              }

              /* Skip union case */

              while (num--)
              {
                assert ((DDS_OP_MASK & jeq_op[0]) == DDS_OP_JEQ);
                if ((jeq_op[1] == disc) || (has_default && (num == 0)))
                {
                  subtype = DDS_JEQ_TYPE (jeq_op[0]);

                  switch (subtype)
                  {
                    case DDS_OP_VAL_1BY:
                    case DDS_OP_VAL_2BY:
                    case DDS_OP_VAL_4BY:
                    case DDS_OP_VAL_8BY:
                    {
                      align = dds_op_size[subtype];
                      DDS_CDR_ALIGNTO (is, align);
                      is->m_index += align;
                      break;
                    }
                    case DDS_OP_VAL_STR:
                    case DDS_OP_VAL_BST:
                    {
                      len = dds_stream_read_uint32 (is);
                      is->m_index += len;
                      break;
                    }
                    default:
                    {
                      dds_stream_extract_key (is, NULL, jeq_op + DDS_OP_ADR_JSR (jeq_op[0]), just_key);
                      break;
                    }
                  }
                  break;
                }
                jeq_op += 3;
              }
            }

            /* Jump to next instruction */

            ops += DDS_OP_ADR_JMP (ops[1]) - 2;
            break;
          }
          default: assert (0);
        }
        break;
      }
      case DDS_OP_JSR: /* Implies nested type */
      {
        dds_stream_extract_key (is, os, ops + DDS_OP_JUMP (op), just_key);
        ops++;
        break;
      }
      default: assert (0);
    }
  }
  return os->m_index - origin;
}

#ifndef NDEBUG
static bool keyhash_is_reset(const dds_key_hash_t *kh)
{
  static const char nullhash[sizeof(kh->m_hash)] = { 0 };
  return !kh->m_set && memcmp(kh->m_hash, nullhash, sizeof(nullhash)) == 0;
}
#endif

void dds_stream_read_keyhash
(
  dds_stream_t * is,
  dds_key_hash_t * kh,
  const dds_topic_descriptor_t * desc,
  const bool just_key
)
{
  assert (keyhash_is_reset(kh));
  kh->m_set = 1;
  if (desc->m_nkeys == 0)
    kh->m_iskey = 1;
  else if (desc->m_flagset & DDS_TOPIC_FIXED_KEY)
  {
    dds_stream_t os;
    uint32_t ncheck;
    kh->m_iskey = 1;
    dds_stream_init(&os, 0);
    os.m_buffer.pv = kh->m_hash;
    os.m_size = 16;
    os.m_endian = 0;
    ncheck = dds_stream_extract_key (is, &os, desc->m_ops, just_key);
    assert(ncheck <= 16);
    (void)ncheck;
  }
  else
  {
    dds_stream_t os;
    md5_state_t md5st;
    kh->m_iskey = 0;
    dds_stream_init (&os, 0);
    os.m_endian = 0;
    dds_stream_extract_key (is, &os, desc->m_ops, just_key);
    md5_init (&md5st);
    md5_append (&md5st, os.m_buffer.p8, os.m_index);
    md5_finish (&md5st, (unsigned char *) kh->m_hash);
    dds_stream_fini (&os);
  }
}

void dds_stream_read_key
(
  dds_stream_t * is,
  char * sample,
  const dds_topic_descriptor_t * desc
)
{
  uint32_t i;
  char * dst;
  const uint32_t * op;

  for (i = 0; i < desc->m_nkeys; i++)
  {
    op = desc->m_ops + desc->m_keys[i].m_index;
    dst = sample + op[1];
    assert ((*op & DDS_OP_FLAG_KEY) && ((DDS_OP_MASK & *op) == DDS_OP_ADR));
    switch (DDS_OP_TYPE (*op))
    {
      case DDS_OP_VAL_1BY:
        *((uint8_t*) dst) = dds_stream_read_uint8 (is);
        break;
      case DDS_OP_VAL_2BY:
        *((uint16_t*) dst) = dds_stream_read_uint16 (is);
        break;
      case DDS_OP_VAL_4BY:
        *((uint32_t*) dst) = dds_stream_read_uint32 (is);
        break;
      case DDS_OP_VAL_8BY:
        *((uint64_t*) dst) = dds_stream_read_uint64 (is);
        break;
      case DDS_OP_VAL_STR:
        *((char**) dst) = dds_stream_reuse_string (is, *((char**) dst), 0);
        break;
      case DDS_OP_VAL_BST:
        dds_stream_reuse_string (is, dst, op[2]);
        break;
      case DDS_OP_VAL_ARR:
      {
        uint32_t subtype = DDS_OP_SUBTYPE (*op);
        assert (subtype <= DDS_OP_VAL_8BY);
        dds_stream_read_fixed_buffer (is, dst, op[2], dds_op_size[subtype], is->m_endian != DDS_ENDIAN);
        break;
      }
      default: assert (0);
    }
  }
}
