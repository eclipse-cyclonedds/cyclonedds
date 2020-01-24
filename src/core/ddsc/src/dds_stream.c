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
#include <ctype.h>

#include "dds/ddsrt/endian.h"
#include "dds/ddsrt/md5.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsi/q_bswap.h"
#include "dds/ddsi/q_config.h"
#include "dds__stream.h"
#include "dds__alloc.h"

#if DDSRT_ENDIAN == DDSRT_LITTLE_ENDIAN
#define DDS_ENDIAN true
#else
#define DDS_ENDIAN false
#endif

static void dds_stream_write (dds_ostream_t * __restrict os, const char * __restrict data, const uint32_t * __restrict ops);
static void dds_stream_read (dds_istream_t * __restrict is, char * __restrict data, const uint32_t * __restrict ops);

static void dds_ostream_grow (dds_ostream_t * __restrict st, uint32_t size)
{
  uint32_t needed = size + st->m_index;

  /* Reallocate on 4k boundry */

  uint32_t newSize = (needed & ~(uint32_t)0xfff) + 0x1000;
  uint8_t *old = st->m_buffer;

  st->m_buffer = ddsrt_realloc (old, newSize);
  st->m_size = newSize;
}

static void dds_cdr_resize (dds_ostream_t * __restrict s, uint32_t l)
{
  if (s->m_size < l + s->m_index)
    dds_ostream_grow (s, l);
}

void dds_ostream_init (dds_ostream_t * __restrict st, uint32_t size)
{
  memset (st, 0, sizeof (*st));
  st->m_index = 0;
  dds_cdr_resize (st, size);
}

void dds_ostreamBE_init (dds_ostreamBE_t * __restrict st, uint32_t size)
{
  dds_ostream_init (&st->x, size);
}

void dds_ostream_fini (dds_ostream_t * __restrict st)
{
  if (st->m_size)
    dds_free (st->m_buffer);
}

void dds_ostreamBE_fini (dds_ostreamBE_t * __restrict st)
{
  dds_ostream_fini (&st->x);
}

static void dds_cdr_alignto (dds_istream_t * __restrict s, uint32_t a)
{
  s->m_index = (s->m_index + a - 1) & ~(a - 1);
  assert (s->m_index < s->m_size);
}

static uint32_t dds_cdr_alignto_clear_and_resize (dds_ostream_t * __restrict s, uint32_t a, uint32_t extra)
{
  const uint32_t m = s->m_index % a;
  if (m == 0)
  {
    dds_cdr_resize (s, extra);
    return 0;
  }
  else
  {
    const uint32_t pad = a - m;
    dds_cdr_resize (s, pad + extra);
    for (uint32_t i = 0; i < pad; i++)
      s->m_buffer[s->m_index++] = 0;
    return pad;
  }
}

static uint32_t dds_cdr_alignto_clear_and_resize_be (dds_ostreamBE_t * __restrict s, uint32_t a, uint32_t extra)
{
  return dds_cdr_alignto_clear_and_resize (&s->x, a, extra);
}

static uint8_t dds_is_get1 (dds_istream_t * __restrict s)
{
  assert (s->m_index < s->m_size);
  uint8_t v = *(s->m_buffer + s->m_index);
  s->m_index++;
  return v;
}

static uint16_t dds_is_get2 (dds_istream_t * __restrict s)
{
  dds_cdr_alignto (s, 2);
  uint16_t v = * ((uint16_t *) (s->m_buffer + s->m_index));
  s->m_index += 2;
  return v;
}

static uint32_t dds_is_get4 (dds_istream_t * __restrict s)
{
  dds_cdr_alignto (s, 4);
  uint32_t v = * ((uint32_t *) (s->m_buffer + s->m_index));
  s->m_index += 4;
  return v;
}

static uint64_t dds_is_get8 (dds_istream_t * __restrict s)
{
  dds_cdr_alignto (s, 8);
  uint64_t v = * ((uint64_t *) (s->m_buffer + s->m_index));
  s->m_index += 8;
  return v;
}

static void dds_is_get_bytes (dds_istream_t * __restrict s, void * __restrict b, uint32_t num, uint32_t elem_size)
{
  dds_cdr_alignto (s, elem_size);
  memcpy (b, s->m_buffer + s->m_index, num * elem_size);
  s->m_index += num * elem_size;
}

static void dds_os_put1 (dds_ostream_t * __restrict s, uint8_t v)
{
  dds_cdr_resize (s, 1);
  *((uint8_t *) (s->m_buffer + s->m_index)) = v;
  s->m_index += 1;
}

static void dds_os_put2 (dds_ostream_t * __restrict s, uint16_t v)
{
  dds_cdr_alignto_clear_and_resize (s, 2, 2);
  *((uint16_t *) (s->m_buffer + s->m_index)) = v;
  s->m_index += 2;
}

static void dds_os_put4 (dds_ostream_t * __restrict s, uint32_t v)
{
  dds_cdr_alignto_clear_and_resize (s, 4, 4);
  *((uint32_t *) (s->m_buffer + s->m_index)) = v;
  s->m_index += 4;
}

static void dds_os_put8 (dds_ostream_t * __restrict s, uint64_t v)
{
  dds_cdr_alignto_clear_and_resize (s, 8, 8);
  *((uint64_t *) (s->m_buffer + s->m_index)) = v;
  s->m_index += 8;
}

static void dds_os_put1be (dds_ostreamBE_t * __restrict s, uint8_t v)
{
  dds_os_put1 (&s->x, v);
}

static void dds_os_put2be (dds_ostreamBE_t * __restrict s, uint16_t v)
{
  dds_os_put2 (&s->x, ddsrt_toBE2u (v));
}

static void dds_os_put4be (dds_ostreamBE_t * __restrict s, uint32_t v)
{
  dds_os_put4 (&s->x, ddsrt_toBE4u (v));
}

static void dds_os_put8be (dds_ostreamBE_t * __restrict s, uint64_t v)
{
  dds_os_put8 (&s->x, ddsrt_toBE8u (v));
}

static void dds_os_put_bytes (dds_ostream_t * __restrict s, const void * __restrict b, uint32_t l)
{
  dds_cdr_resize (s, l);
  memcpy (s->m_buffer + s->m_index, b, l);
  s->m_index += l;
}

static void dds_os_put_bytes_aligned (dds_ostream_t * __restrict s, const void * __restrict b, uint32_t n, uint32_t a)
{
  const uint32_t l = n * a;
  dds_cdr_alignto_clear_and_resize (s, a, l);
  memcpy (s->m_buffer + s->m_index, b, l);
  s->m_index += l;
}

static uint32_t get_type_size (enum dds_stream_typecode type)
{
  DDSRT_STATIC_ASSERT (DDS_OP_VAL_1BY == 1 && DDS_OP_VAL_2BY == 2 && DDS_OP_VAL_4BY == 3 && DDS_OP_VAL_8BY == 4);
  assert (type == DDS_OP_VAL_1BY || type == DDS_OP_VAL_2BY || type == DDS_OP_VAL_4BY || type == DDS_OP_VAL_8BY);
  return (uint32_t)1 << ((uint32_t) type - 1);
}

static size_t dds_stream_check_optimize1 (const dds_topic_descriptor_t * __restrict desc)
{
  const uint32_t *ops = desc->m_ops;
  uint32_t insn;
  while ((insn = *ops) != DDS_OP_RTS)
  {
    if (DDS_OP (insn) != DDS_OP_ADR)
      return 0;

    switch (DDS_OP_TYPE (insn))
    {
      case DDS_OP_VAL_1BY:
      case DDS_OP_VAL_2BY:
      case DDS_OP_VAL_4BY:
      case DDS_OP_VAL_8BY:
        if ((ops[1] % get_type_size (DDS_OP_TYPE (insn))) != 0)
          return 0;
        ops += 2;
        break;

      case DDS_OP_VAL_ARR:
        switch (DDS_OP_SUBTYPE (insn))
        {
          case DDS_OP_VAL_1BY: case DDS_OP_VAL_2BY: case DDS_OP_VAL_4BY: case DDS_OP_VAL_8BY:
            if ((ops[1] % get_type_size (DDS_OP_SUBTYPE (insn))) != 0)
              return 0;
            ops += 3;
            break;
          default:
            return 0;
        }
        break;

      default:
        return 0;
    }
  }

  return desc->m_size;
}

size_t dds_stream_check_optimize (const dds_topic_descriptor_t * __restrict desc)
{
  return dds_stream_check_optimize1 (desc);
}

static void dds_stream_reuse_string_bound (dds_istream_t * __restrict is, char * __restrict str, const uint32_t bound)
{
  const uint32_t length = dds_is_get4 (is);
  const void *src = is->m_buffer + is->m_index;
  /* FIXME: validation now rejects data containing an oversize bounded string,
     so this check is superfluous, but perhaps rejecting such a sample is the
     wrong thing to do */
  assert (str != NULL);
  memcpy (str, src, length > bound ? bound : length);
  is->m_index += length;
}

static char *dds_stream_reuse_string (dds_istream_t * __restrict is, char * __restrict str)
{
  const uint32_t length = dds_is_get4 (is);
  const void *src = is->m_buffer + is->m_index;
  if (str == NULL || strlen (str) + 1 < length)
    str = dds_realloc (str, length);
  memcpy (str, src, length);
  is->m_index += length;
  return str;
}

static void dds_stream_skip_forward (dds_istream_t * __restrict is, uint32_t len, const uint32_t elem_size)
{
  if (elem_size && len)
    is->m_index += len * elem_size;
}

static void dds_stream_skip_string (dds_istream_t * __restrict is)
{
  const uint32_t length = dds_is_get4 (is);
  dds_stream_skip_forward (is, length, 1);
}

static void dds_stream_write_string (dds_ostream_t * __restrict os, const char * __restrict val)
{
  uint32_t size = 1;

  if (val)
  {
    /* Type casting is done for the warning of conversion from 'size_t' to 'uint32_t', which may cause possible loss of data */
    size += (uint32_t) strlen (val);
  }

  dds_os_put4 (os, size);

  if (val)
  {
    dds_os_put_bytes (os, val, size);
  }
  else
  {
    dds_os_put1 (os, 0);
  }
}

static void dds_streamBE_write_string (dds_ostreamBE_t * __restrict os, const char * __restrict val)
{
  uint32_t size = 1;

  if (val)
  {
    /* Type casting is done for the warning of conversion from 'size_t' to 'uint32_t', which may cause possible loss of data */
    size += (uint32_t) strlen (val);
  }

  dds_os_put4be (os, size);

  if (val)
  {
    dds_os_put_bytes (&os->x, val, size);
  }
  else
  {
    dds_os_put1be (os, 0);
  }
}

#ifndef NDEBUG
static bool insn_key_ok_p (uint32_t insn)
{
  return (DDS_OP (insn) == DDS_OP_ADR && (insn & DDS_OP_FLAG_KEY) &&
          (DDS_OP_TYPE (insn) <= DDS_OP_VAL_BST ||
           (DDS_OP_TYPE (insn) == DDS_OP_VAL_ARR && DDS_OP_SUBTYPE (insn) <= DDS_OP_VAL_8BY)));
}
#endif

static uint32_t read_union_discriminant (dds_istream_t * __restrict is, enum dds_stream_typecode type)
{
  assert (type == DDS_OP_VAL_1BY || type == DDS_OP_VAL_2BY || type == DDS_OP_VAL_4BY);
  switch (type)
  {
    case DDS_OP_VAL_1BY: return dds_is_get1 (is);
    case DDS_OP_VAL_2BY: return dds_is_get2 (is);
    case DDS_OP_VAL_4BY: return dds_is_get4 (is);
    default: return 0;
  }
}

static uint32_t write_union_discriminant (dds_ostream_t * __restrict os, enum dds_stream_typecode type, const void * __restrict addr)
{
  assert (type == DDS_OP_VAL_1BY || type == DDS_OP_VAL_2BY || type == DDS_OP_VAL_4BY);
  switch (type)
  {
    case DDS_OP_VAL_1BY: { uint8_t  d8  = *((const uint8_t *) addr); dds_os_put1 (os, d8); return d8; }
    case DDS_OP_VAL_2BY: { uint16_t d16 = *((const uint16_t *) addr); dds_os_put2 (os, d16); return d16; }
    case DDS_OP_VAL_4BY: { uint32_t d32 = *((const uint32_t *) addr); dds_os_put4 (os, d32); return d32; }
    default: return 0;
  }
}

static const uint32_t *find_union_case (const uint32_t * __restrict union_ops, uint32_t disc)
{
  assert (DDS_OP_TYPE (*union_ops) == DDS_OP_VAL_UNI);
  const bool has_default = *union_ops & DDS_OP_FLAG_DEF;
  const uint32_t numcases = union_ops[2];
  const uint32_t *jeq_op = union_ops + DDS_OP_ADR_JSR (union_ops[3]);
  /* Find union case; default case is always the last one */
  assert (numcases > 0);
  uint32_t ci;
#ifndef NDEBUG
  for (ci = 0; ci < numcases; ci++)
    assert (DDS_OP (jeq_op[3 * ci]) == DDS_OP_JEQ);
#endif
  for (ci = 0; ci < numcases - (has_default ? 1 : 0); ci++, jeq_op += 3)
    if (jeq_op[1] == disc)
      return jeq_op;
  return (ci < numcases) ? jeq_op : NULL;
}

static const uint32_t *skip_sequence_insns (const uint32_t * __restrict ops, uint32_t insn)
{
  switch (DDS_OP_SUBTYPE (insn))
  {
    case DDS_OP_VAL_1BY: case DDS_OP_VAL_2BY: case DDS_OP_VAL_4BY: case DDS_OP_VAL_8BY: case DDS_OP_VAL_STR:
      return ops + 2;
    case DDS_OP_VAL_BST:
      return ops + 3; /* bound */
    case DDS_OP_VAL_SEQ: case DDS_OP_VAL_ARR: case DDS_OP_VAL_UNI: case DDS_OP_VAL_STU: {
      const uint32_t jmp = DDS_OP_ADR_JMP (ops[3]);
      return ops + (jmp ? jmp : 4); /* FIXME: why would jmp be 0? */
    }
  }
  return NULL;
}

static const uint32_t *dds_stream_write_seq (dds_ostream_t * __restrict os, const char * __restrict addr, const uint32_t * __restrict ops, uint32_t insn)
{
  const dds_sequence_t * const seq = (const dds_sequence_t *) addr;
  const uint32_t num = seq->_length;

  dds_os_put4 (os, num);
  if (num == 0)
    return skip_sequence_insns (ops, insn);

  const enum dds_stream_typecode subtype = DDS_OP_SUBTYPE (insn);
  /* following length, stream is aligned to mod 4 */
  switch (subtype)
  {
    case DDS_OP_VAL_1BY: case DDS_OP_VAL_2BY: case DDS_OP_VAL_4BY: case DDS_OP_VAL_8BY:
      dds_os_put_bytes_aligned (os, seq->_buffer, num, get_type_size (subtype));
      return ops + 2;
    case DDS_OP_VAL_STR: {
      const char **ptr = (const char **) seq->_buffer;
      for (uint32_t i = 0; i < num; i++)
        dds_stream_write_string (os, ptr[i]);
      return ops + 2;
    }
    case DDS_OP_VAL_BST: {
      const char *ptr = (const char *) seq->_buffer;
      const uint32_t elem_size = ops[2];
      for (uint32_t i = 0; i < num; i++)
        dds_stream_write_string (os, ptr + i * elem_size);
      return ops + 3;
    }
    case DDS_OP_VAL_SEQ: case DDS_OP_VAL_ARR: case DDS_OP_VAL_UNI: case DDS_OP_VAL_STU: {
      const uint32_t elem_size = ops[2];
      const uint32_t jmp = DDS_OP_ADR_JMP (ops[3]);
      uint32_t const * const jsr_ops = ops + DDS_OP_ADR_JSR (ops[3]);
      const char *ptr = (const char *) seq->_buffer;
      for (uint32_t i = 0; i < num; i++)
        dds_stream_write (os, ptr + i * elem_size, jsr_ops);
      return ops + (jmp ? jmp : 4); /* FIXME: why would jmp be 0? */
    }
  }
  return NULL;
}

static const uint32_t *dds_stream_write_arr (dds_ostream_t * __restrict os, const char * __restrict addr, const uint32_t * __restrict ops, uint32_t insn)
{
  const enum dds_stream_typecode subtype = DDS_OP_SUBTYPE (insn);
  const uint32_t num = ops[2];
  switch (subtype)
  {
    case DDS_OP_VAL_1BY: case DDS_OP_VAL_2BY: case DDS_OP_VAL_4BY: case DDS_OP_VAL_8BY:
      dds_os_put_bytes_aligned (os, addr, num, get_type_size (subtype));
      return ops + 3;
    case DDS_OP_VAL_STR: {
      const char **ptr = (const char **) addr;
      for (uint32_t i = 0; i < num; i++)
        dds_stream_write_string (os, ptr[i]);
      return ops + 3;
    }
    case DDS_OP_VAL_BST: {
      const char *ptr = (const char *) addr;
      const uint32_t elem_size = ops[4];
      for (uint32_t i = 0; i < num; i++)
        dds_stream_write_string (os, ptr + i * elem_size);
      return ops + 5;
      break;
    }
    case DDS_OP_VAL_SEQ: case DDS_OP_VAL_ARR: case DDS_OP_VAL_UNI: case DDS_OP_VAL_STU: {
      const uint32_t * jsr_ops = ops + DDS_OP_ADR_JSR (ops[3]);
      const uint32_t jmp = DDS_OP_ADR_JMP (ops[3]);
      const uint32_t elem_size = ops[4];
      for (uint32_t i = 0; i < num; i++)
        dds_stream_write (os, addr + i * elem_size, jsr_ops);
      return ops + (jmp ? jmp : 5);
    }
  }
  return NULL;
}

static const uint32_t *dds_stream_write_uni (dds_ostream_t * __restrict os, const char * __restrict discaddr, const char * __restrict baseaddr, const uint32_t * __restrict ops, uint32_t insn)
{
  const uint32_t disc = write_union_discriminant (os, DDS_OP_SUBTYPE (insn), discaddr);
  uint32_t const * const jeq_op = find_union_case (ops, disc);
  ops += DDS_OP_ADR_JMP (ops[3]);
  if (jeq_op)
  {
    const enum dds_stream_typecode valtype = DDS_JEQ_TYPE (jeq_op[0]);
    const void *valaddr = baseaddr + jeq_op[2];
    switch (valtype)
    {
      case DDS_OP_VAL_1BY: dds_os_put1 (os, *(const uint8_t *) valaddr); break;
      case DDS_OP_VAL_2BY: dds_os_put2 (os, *(const uint16_t *) valaddr); break;
      case DDS_OP_VAL_4BY: dds_os_put4 (os, *(const uint32_t *) valaddr); break;
      case DDS_OP_VAL_8BY: dds_os_put8 (os, *(const uint64_t *) valaddr); break;
      case DDS_OP_VAL_STR: dds_stream_write_string (os, *(const char **) valaddr); break;
      case DDS_OP_VAL_BST: dds_stream_write_string (os, (const char *) valaddr); break;
      case DDS_OP_VAL_SEQ: case DDS_OP_VAL_ARR: case DDS_OP_VAL_UNI: case DDS_OP_VAL_STU:
        dds_stream_write (os, valaddr, jeq_op + DDS_OP_ADR_JSR (jeq_op[0]));
        break;
    }
  }
  return ops;
}

static void dds_stream_write (dds_ostream_t * __restrict os, const char * __restrict data, const uint32_t * __restrict ops)
{
  uint32_t insn;
  while ((insn = *ops) != DDS_OP_RTS)
  {
    switch (DDS_OP (insn))
    {
      case DDS_OP_ADR: {
        const void *addr = data + ops[1];
        switch (DDS_OP_TYPE (insn))
        {
          case DDS_OP_VAL_1BY: dds_os_put1 (os, *((const uint8_t *) addr)); ops += 2; break;
          case DDS_OP_VAL_2BY: dds_os_put2 (os, *((const uint16_t *) addr)); ops += 2; break;
          case DDS_OP_VAL_4BY: dds_os_put4 (os, *((const uint32_t *) addr)); ops += 2; break;
          case DDS_OP_VAL_8BY: dds_os_put8 (os, *((const uint64_t *) addr)); ops += 2; break;
          case DDS_OP_VAL_STR: dds_stream_write_string (os, *((const char **) addr)); ops += 2; break;
          case DDS_OP_VAL_BST: dds_stream_write_string (os, (const char *) addr); ops += 3; break;
          case DDS_OP_VAL_SEQ: ops = dds_stream_write_seq (os, addr, ops, insn); break;
          case DDS_OP_VAL_ARR: ops = dds_stream_write_arr (os, addr, ops, insn); break;
          case DDS_OP_VAL_UNI: ops = dds_stream_write_uni (os, addr, data, ops, insn); break;
          case DDS_OP_VAL_STU: abort (); break;
        }
        break;
      }
      case DDS_OP_JSR: {
        dds_stream_write (os, data, ops + DDS_OP_JUMP (insn));
        ops++;
        break;
      }
      case DDS_OP_RTS: case DDS_OP_JEQ: {
        abort ();
        break;
      }
    }
  }
}

static void realloc_sequence_buffer_if_needed (dds_sequence_t * __restrict seq, uint32_t num, uint32_t elem_size, bool init)
{
  const uint32_t size = num * elem_size;

  /* maintain max sequence length (may not have been set by caller) */
  if (seq->_length > seq->_maximum)
    seq->_maximum = seq->_length;

  if (num > seq->_maximum && seq->_release)
  {
    seq->_buffer = ddsrt_realloc (seq->_buffer, size);
    if (init)
    {
      const uint32_t off = seq->_maximum * elem_size;
      memset (seq->_buffer + off, 0, size - off);
    }
    seq->_maximum = num;
  }
  else if (num > 0 && seq->_maximum == 0)
  {
    seq->_buffer = ddsrt_malloc (size);
    if (init)
      memset (seq->_buffer, 0, size);
    seq->_release = true;
    seq->_maximum = num;
  }
}

static const uint32_t *dds_stream_read_seq (dds_istream_t * __restrict is, char * __restrict addr, const uint32_t * __restrict ops, uint32_t insn)
{
  dds_sequence_t * const seq = (dds_sequence_t *) addr;
  const enum dds_stream_typecode subtype = DDS_OP_SUBTYPE (insn);
  const uint32_t num = dds_is_get4 (is);
  if (num == 0)
  {
    seq->_length = 0;
    return skip_sequence_insns (ops, insn);
  }

  switch (subtype)
  {
    case DDS_OP_VAL_1BY: case DDS_OP_VAL_2BY: case DDS_OP_VAL_4BY: case DDS_OP_VAL_8BY: {
      const uint32_t elem_size = get_type_size (subtype);
      realloc_sequence_buffer_if_needed (seq, num, elem_size, false);
      seq->_length = (num <= seq->_maximum) ? num : seq->_maximum;
      dds_is_get_bytes (is, seq->_buffer, seq->_length, elem_size);
      if (seq->_length < num)
        dds_stream_skip_forward (is, num - seq->_length, elem_size);
      return ops + 2;
    }
    case DDS_OP_VAL_STR: {
      realloc_sequence_buffer_if_needed (seq, num, sizeof (char *), true);
      seq->_length = (num <= seq->_maximum) ? num : seq->_maximum;
      char **ptr = (char **) seq->_buffer;
      for (uint32_t i = 0; i < seq->_length; i++)
        ptr[i] = dds_stream_reuse_string (is, ptr[i]);
      for (uint32_t i = seq->_length; i < num; i++)
        dds_stream_skip_string (is);
      return ops + 2;
    }
    case DDS_OP_VAL_BST: {
      const uint32_t elem_size = ops[2];
      realloc_sequence_buffer_if_needed (seq, num, elem_size, false);
      seq->_length = (num <= seq->_maximum) ? num : seq->_maximum;
      char *ptr = (char *) seq->_buffer;
      for (uint32_t i = 0; i < seq->_length; i++)
        dds_stream_reuse_string_bound (is, ptr + i * elem_size, elem_size);
      for (uint32_t i = seq->_length; i < num; i++)
        dds_stream_skip_string (is);
      return ops + 3;
    }
    case DDS_OP_VAL_SEQ: case DDS_OP_VAL_ARR: case DDS_OP_VAL_UNI: case DDS_OP_VAL_STU: {
      const uint32_t elem_size = ops[2];
      const uint32_t jmp = DDS_OP_ADR_JMP (ops[3]);
      uint32_t const * const jsr_ops = ops + DDS_OP_ADR_JSR (ops[3]);
      realloc_sequence_buffer_if_needed (seq, num, elem_size, true);
      seq->_length = (num <= seq->_maximum) ? num : seq->_maximum;
      char *ptr = (char *) seq->_buffer;
      for (uint32_t i = 0; i < num; i++)
        dds_stream_read (is, ptr + i * elem_size, jsr_ops);
      return ops + (jmp ? jmp : 4); /* FIXME: why would jmp be 0? */
    }
  }
  return NULL;
}

static const uint32_t *dds_stream_read_arr (dds_istream_t * __restrict is, char * __restrict addr, const uint32_t * __restrict ops, uint32_t insn)
{
  const enum dds_stream_typecode subtype = DDS_OP_SUBTYPE (insn);
  const uint32_t num = ops[2];
  switch (subtype)
  {
    case DDS_OP_VAL_1BY: case DDS_OP_VAL_2BY: case DDS_OP_VAL_4BY: case DDS_OP_VAL_8BY: {
      const uint32_t elem_size = get_type_size (subtype);
      dds_is_get_bytes (is, addr, num, elem_size);
      return ops + 3;
    }
    case DDS_OP_VAL_STR: {
      char **ptr = (char **) addr;
      for (uint32_t i = 0; i < num; i++)
        ptr[i] = dds_stream_reuse_string (is, ptr[i]);
      return ops + 3;
    }
    case DDS_OP_VAL_BST: {
      char *ptr = (char *) addr;
      const uint32_t elem_size = ops[4];
      for (uint32_t i = 0; i < num; i++)
        dds_stream_reuse_string_bound (is, ptr + i * elem_size, elem_size);
      return ops + 5;
    }
    case DDS_OP_VAL_SEQ: case DDS_OP_VAL_ARR: case DDS_OP_VAL_UNI: case DDS_OP_VAL_STU: {
      const uint32_t *jsr_ops = ops + DDS_OP_ADR_JSR (ops[3]);
      const uint32_t jmp = DDS_OP_ADR_JMP (ops[3]);
      const uint32_t elem_size = ops[4];
      for (uint32_t i = 0; i < num; i++)
        dds_stream_read (is, addr + i * elem_size, jsr_ops);
      return ops + (jmp ? jmp : 5);
    }
  }
  return NULL;
}

static const uint32_t *dds_stream_read_uni (dds_istream_t * __restrict is, char * __restrict discaddr, char * __restrict baseaddr, const uint32_t * __restrict ops, uint32_t insn)
{
  const uint32_t disc = read_union_discriminant (is, DDS_OP_SUBTYPE (insn));
  switch (DDS_OP_SUBTYPE (insn))
  {
    case DDS_OP_VAL_1BY: *((uint8_t *) discaddr) = (uint8_t) disc; break;
    case DDS_OP_VAL_2BY: *((uint16_t *) discaddr) = (uint16_t) disc; break;
    case DDS_OP_VAL_4BY: *((uint32_t *) discaddr) = disc; break;
    default: break;
  }
  uint32_t const * const jeq_op = find_union_case (ops, disc);
  ops += DDS_OP_ADR_JMP (ops[3]);
  if (jeq_op)
  {
    const enum dds_stream_typecode valtype = DDS_JEQ_TYPE (jeq_op[0]);
    void *valaddr = baseaddr + jeq_op[2];
    switch (valtype)
    {
      case DDS_OP_VAL_1BY: *((uint8_t *) valaddr) = dds_is_get1 (is); break;
      case DDS_OP_VAL_2BY: *((uint16_t *) valaddr) = dds_is_get2 (is); break;
      case DDS_OP_VAL_4BY: *((uint32_t *) valaddr) = dds_is_get4 (is); break;
      case DDS_OP_VAL_8BY: *((uint64_t *) valaddr) = dds_is_get8 (is); break;
      case DDS_OP_VAL_STR: *(char **) valaddr = dds_stream_reuse_string (is, *((char **) valaddr)); break;
      case DDS_OP_VAL_BST: case DDS_OP_VAL_SEQ: case DDS_OP_VAL_ARR: case DDS_OP_VAL_UNI: case DDS_OP_VAL_STU:
        dds_stream_read (is, valaddr, jeq_op + DDS_OP_ADR_JSR (jeq_op[0]));
        break;
    }
  }
  return ops;
}

static void dds_stream_read (dds_istream_t * __restrict is, char * __restrict data, const uint32_t * __restrict ops)
{
  uint32_t insn;
  while ((insn = *ops) != DDS_OP_RTS)
  {
    switch (DDS_OP (insn))
    {
      case DDS_OP_ADR: {
        void *addr = data + ops[1];
        switch (DDS_OP_TYPE (insn))
        {
          case DDS_OP_VAL_1BY: *((uint8_t *) addr) = dds_is_get1 (is); ops += 2; break;
          case DDS_OP_VAL_2BY: *((uint16_t *) addr) = dds_is_get2 (is); ops += 2; break;
          case DDS_OP_VAL_4BY: *((uint32_t *) addr) = dds_is_get4 (is); ops += 2; break;
          case DDS_OP_VAL_8BY: *((uint64_t *) addr) = dds_is_get8 (is); ops += 2; break;
          case DDS_OP_VAL_STR: *((char **) addr) = dds_stream_reuse_string (is, *((char **) addr)); ops += 2; break;
          case DDS_OP_VAL_BST: dds_stream_reuse_string_bound (is, (char *) addr, ops[2]); ops += 3; break;
          case DDS_OP_VAL_SEQ: ops = dds_stream_read_seq (is, addr, ops, insn); break;
          case DDS_OP_VAL_ARR: ops = dds_stream_read_arr (is, addr, ops, insn); break;
          case DDS_OP_VAL_UNI: ops = dds_stream_read_uni (is, addr, data, ops, insn); break;
          case DDS_OP_VAL_STU: abort (); break;
        }
        break;
      }
      case DDS_OP_JSR: {
        dds_stream_read (is, data, ops + DDS_OP_JUMP (insn));
        ops++;
        break;
      }
      case DDS_OP_RTS: case DDS_OP_JEQ: {
        abort ();
        break;
      }
    }
  }
}

/*******************************************************************************************
 **
 **  Validation and conversion to native endian.
 **
 *******************************************************************************************/

/* Limit the size of the input buffer so we don't need to worry about adding
   padding and a primitive type overflowing our offset */
#define CDR_SIZE_MAX ((uint32_t) 0xfffffff0)

static bool stream_normalize (char * __restrict data, uint32_t * __restrict off, uint32_t size, bool bswap, const uint32_t * __restrict ops);

static uint32_t check_align_prim (uint32_t off, uint32_t size, uint32_t a_lg2)
{
  assert (a_lg2 <= 3);
  const uint32_t a = 1u << a_lg2;
  assert (size <= CDR_SIZE_MAX);
  assert (off <= size);
  const uint32_t off1 = (off + a - 1) & ~(a - 1);
  assert (off <= off1 && off1 <= CDR_SIZE_MAX);
  if (size < off1 + a)
    return UINT32_MAX;
  return off1;
}

static uint32_t check_align_prim_many (uint32_t off, uint32_t size, uint32_t a_lg2, uint32_t n)
{
  assert (a_lg2 <= 3);
  const uint32_t a = 1u << a_lg2;
  assert (size <= CDR_SIZE_MAX);
  assert (off <= size);
  const uint32_t off1 = (off + a - 1) & ~(a - 1);
  assert (off <= off1 && off1 <= CDR_SIZE_MAX);
  if (size < off1 || ((size - off1) >> a_lg2) < n)
    return UINT32_MAX;
  return off1;
}

static bool normalize_uint8 (uint32_t *off, uint32_t size)
{
  if (*off == size)
    return false;
  (*off)++;
  return true;
}

static bool normalize_uint16 (char * __restrict data, uint32_t * __restrict off, uint32_t size, bool bswap)
{
  if ((*off = check_align_prim (*off, size, 1)) == UINT32_MAX)
    return false;
  if (bswap)
    *((uint16_t *) (data + *off)) = ddsrt_bswap2u (*((uint16_t *) (data + *off)));
  (*off) += 2;
  return true;
}

static bool normalize_uint32 (char * __restrict data, uint32_t * __restrict off, uint32_t size, bool bswap)
{
  if ((*off = check_align_prim (*off, size, 2)) == UINT32_MAX)
    return false;
  if (bswap)
    *((uint32_t *) (data + *off)) = ddsrt_bswap4u (*((uint32_t *) (data + *off)));
  (*off) += 4;
  return true;
}

static bool read_and_normalize_uint32 (uint32_t * __restrict val, char * __restrict data, uint32_t * __restrict off, uint32_t size, bool bswap)
{
  if ((*off = check_align_prim (*off, size, 2)) == UINT32_MAX)
    return false;
  if (bswap)
    *((uint32_t *) (data + *off)) = ddsrt_bswap4u (*((uint32_t *) (data + *off)));
  *val = *((uint32_t *) (data + *off));
  (*off) += 4;
  return true;
}

static bool normalize_uint64 (char * __restrict data, uint32_t * __restrict off, uint32_t size, bool bswap)
{
  if ((*off = check_align_prim (*off, size, 3)) == UINT32_MAX)
    return false;
  if (bswap)
    *((uint64_t *) (data + *off)) = ddsrt_bswap8u (*((uint64_t *) (data + *off)));
  (*off) += 8;
  return true;
}

static bool normalize_string (char * __restrict data, uint32_t * __restrict off, uint32_t size, bool bswap, size_t maxsz)
{
  uint32_t sz;
  if (!read_and_normalize_uint32 (&sz, data, off, size, bswap))
    return false;
  if (sz == 0 || size - *off < sz || maxsz < sz)
    return false;
  if (data[*off + sz - 1] != 0)
    return false;
  *off += sz;
  return true;
}

static bool normalize_primarray (char * __restrict data, uint32_t * __restrict off, uint32_t size, bool bswap, uint32_t num, enum dds_stream_typecode type)
{
  switch (type)
  {
    case DDS_OP_VAL_1BY:
      if ((*off = check_align_prim_many (*off, size, 0, num)) == UINT32_MAX)
        return false;
      *off += num;
      return true;
    case DDS_OP_VAL_2BY:
      if ((*off = check_align_prim_many (*off, size, 1, num)) == UINT32_MAX)
        return false;
      if (bswap)
      {
        uint16_t *xs = (uint16_t *) (data + *off);
        for (uint32_t i = 0; i < num; i++)
          xs[i] = ddsrt_bswap2u (xs[i]);
      }
      *off += 2 * num;
      return true;
    case DDS_OP_VAL_4BY:
      if ((*off = check_align_prim_many (*off, size, 2, num)) == UINT32_MAX)
        return false;
      if (bswap)
      {
        uint32_t *xs = (uint32_t *) (data + *off);
        for (uint32_t i = 0; i < num; i++)
          xs[i] = ddsrt_bswap4u (xs[i]);
      }
      *off += 4 * num;
      return true;
    case DDS_OP_VAL_8BY:
      if ((*off = check_align_prim_many (*off, size, 3, num)) == UINT32_MAX)
        return false;
      if (bswap)
      {
        uint64_t *xs = (uint64_t *) (data + *off);
        for (uint32_t i = 0; i < num; i++)
          xs[i] = ddsrt_bswap8u (xs[i]);
      }
      *off += 8 * num;
      return true;
    default:
      abort ();
      break;
  }
  return false;
}

static const uint32_t *normalize_seq (char * __restrict data, uint32_t * __restrict off, uint32_t size, bool bswap, const uint32_t * __restrict ops, uint32_t insn)
{
  const enum dds_stream_typecode subtype = DDS_OP_SUBTYPE (insn);
  uint32_t num;
  if (!read_and_normalize_uint32 (&num, data, off, size, bswap))
    return NULL;
  if (num == 0)
    return skip_sequence_insns (ops, insn);
  switch (subtype)
  {
    case DDS_OP_VAL_1BY: case DDS_OP_VAL_2BY: case DDS_OP_VAL_4BY: case DDS_OP_VAL_8BY:
      if (!normalize_primarray (data, off, size, bswap, num, subtype))
        return NULL;
      return ops + 2;
    case DDS_OP_VAL_STR: case DDS_OP_VAL_BST: {
      const size_t maxsz = (subtype == DDS_OP_VAL_STR) ? SIZE_MAX : ops[2];
      for (uint32_t i = 0; i < num; i++)
        if (!normalize_string (data, off, size, bswap, maxsz))
          return NULL;
      return ops + (subtype == DDS_OP_VAL_STR ? 2 : 3);
    }
    case DDS_OP_VAL_SEQ: case DDS_OP_VAL_ARR: case DDS_OP_VAL_UNI: case DDS_OP_VAL_STU: {
      const uint32_t jmp = DDS_OP_ADR_JMP (ops[3]);
      uint32_t const * const jsr_ops = ops + DDS_OP_ADR_JSR (ops[3]);
      for (uint32_t i = 0; i < num; i++)
        if (!stream_normalize (data, off, size, bswap, jsr_ops))
          return NULL;
      return ops + (jmp ? jmp : 4); /* FIXME: why would jmp be 0? */
    }
  }
  return NULL;
}

static const uint32_t *normalize_arr (char * __restrict data, uint32_t * __restrict off, uint32_t size, bool bswap, const uint32_t * __restrict ops, uint32_t insn)
{
  const enum dds_stream_typecode subtype = DDS_OP_SUBTYPE (insn);
  const uint32_t num = ops[2];
  switch (subtype)
  {
    case DDS_OP_VAL_1BY: case DDS_OP_VAL_2BY: case DDS_OP_VAL_4BY: case DDS_OP_VAL_8BY:
      if (!normalize_primarray (data, off, size, bswap, num, subtype))
        return NULL;
      return ops + 3;
    case DDS_OP_VAL_STR: case DDS_OP_VAL_BST: {
      const size_t maxsz = (subtype == DDS_OP_VAL_STR) ? SIZE_MAX : ops[4];
      for (uint32_t i = 0; i < num; i++)
        if (!normalize_string (data, off, size, bswap, maxsz))
          return NULL;
      return ops + (subtype == DDS_OP_VAL_STR ? 3 : 5);
    }
    case DDS_OP_VAL_SEQ: case DDS_OP_VAL_ARR: case DDS_OP_VAL_UNI: case DDS_OP_VAL_STU: {
      const uint32_t *jsr_ops = ops + DDS_OP_ADR_JSR (ops[3]);
      const uint32_t jmp = DDS_OP_ADR_JMP (ops[3]);
      for (uint32_t i = 0; i < num; i++)
        if (!stream_normalize (data, off, size, bswap, jsr_ops))
          return NULL;
      return ops + (jmp ? jmp : 5);
    }
  }
  return NULL;
}

static bool normalize_uni_disc (uint32_t * __restrict val, char * __restrict data, uint32_t * __restrict off, uint32_t size, bool bswap, enum dds_stream_typecode disctype)
{
  switch (disctype)
  {
    case DDS_OP_VAL_1BY:
      if ((*off = check_align_prim (*off, size, 0)) == UINT32_MAX)
        return false;
      *val = *((uint8_t *) (data + *off));
      (*off) += 1;
      return true;
    case DDS_OP_VAL_2BY:
      if ((*off = check_align_prim (*off, size, 1)) == UINT32_MAX)
        return false;
      if (bswap)
        *((uint16_t *) (data + *off)) = ddsrt_bswap2u (*((uint16_t *) (data + *off)));
      *val = *((uint16_t *) (data + *off));
      (*off) += 2;
      return true;
    case DDS_OP_VAL_4BY:
      if ((*off = check_align_prim (*off, size, 2)) == UINT32_MAX)
        return false;
      if (bswap)
        *((uint32_t *) (data + *off)) = ddsrt_bswap4u (*((uint32_t *) (data + *off)));
      *val = *((uint32_t *) (data + *off));
      (*off) += 4;
      return true;
    default:
      abort ();
  }
  return false;
}

static const uint32_t *normalize_uni (char * __restrict data, uint32_t * __restrict off, uint32_t size, bool bswap, const uint32_t * __restrict ops, uint32_t insn)
{
  uint32_t disc;
  if (!normalize_uni_disc (&disc, data, off, size, bswap, DDS_OP_SUBTYPE (insn)))
    return NULL;
  uint32_t const * const jeq_op = find_union_case (ops, disc);
  ops += DDS_OP_ADR_JMP (ops[3]);
  if (jeq_op)
  {
    const enum dds_stream_typecode valtype = DDS_JEQ_TYPE (jeq_op[0]);
    switch (valtype)
    {
      case DDS_OP_VAL_1BY: if (!normalize_uint8 (off, size)) return NULL; break;
      case DDS_OP_VAL_2BY: if (!normalize_uint16 (data, off, size, bswap)) return NULL; break;
      case DDS_OP_VAL_4BY: if (!normalize_uint32 (data, off, size, bswap)) return NULL; break;
      case DDS_OP_VAL_8BY: if (!normalize_uint64 (data, off, size, bswap)) return NULL; break;
      case DDS_OP_VAL_STR: if (!normalize_string (data, off, size, bswap, SIZE_MAX)) return NULL; break;
      case DDS_OP_VAL_BST: case DDS_OP_VAL_SEQ: case DDS_OP_VAL_ARR: case DDS_OP_VAL_UNI: case DDS_OP_VAL_STU:
        if (!stream_normalize (data, off, size, bswap, jeq_op + DDS_OP_ADR_JSR (jeq_op[0])))
          return NULL;
        break;
    }
  }
  return ops;
}

static bool stream_normalize (char * __restrict data, uint32_t * __restrict off, uint32_t size, bool bswap, const uint32_t * __restrict ops)
{
  uint32_t insn;
  while ((insn = *ops) != DDS_OP_RTS)
  {
    switch (DDS_OP (insn))
    {
      case DDS_OP_ADR: {
        switch (DDS_OP_TYPE (insn))
        {
          case DDS_OP_VAL_1BY: if (!normalize_uint8 (off, size)) return false; ops += 2; break;
          case DDS_OP_VAL_2BY: if (!normalize_uint16 (data, off, size, bswap)) return false; ops += 2; break;
          case DDS_OP_VAL_4BY: if (!normalize_uint32 (data, off, size, bswap)) return false; ops += 2; break;
          case DDS_OP_VAL_8BY: if (!normalize_uint64 (data, off, size, bswap)) return false; ops += 2; break;
          case DDS_OP_VAL_STR: if (!normalize_string (data, off, size, bswap, SIZE_MAX)) return false; ops += 2; break;
          case DDS_OP_VAL_BST: if (!normalize_string (data, off, size, bswap, ops[2])) return false; ops += 3; break;
          case DDS_OP_VAL_SEQ: ops = normalize_seq (data, off, size, bswap, ops, insn); if (!ops) return false; break;
          case DDS_OP_VAL_ARR: ops = normalize_arr (data, off, size, bswap, ops, insn); if (!ops) return false; break;
          case DDS_OP_VAL_UNI: ops = normalize_uni (data, off, size, bswap, ops, insn); if (!ops) return false; break;
          case DDS_OP_VAL_STU: abort (); break;
        }
        break;
      }
      case DDS_OP_JSR: {
        if (!stream_normalize (data, off, size, bswap, ops + DDS_OP_JUMP (insn)))
          return false;
        ops++;
        break;
      }
      case DDS_OP_RTS: case DDS_OP_JEQ: {
        abort ();
        break;
      }
    }
  }
  return true;
}

static bool stream_normalize_key (void * __restrict data, uint32_t size, bool bswap, const struct dds_topic_descriptor * __restrict desc)
{
  uint32_t off = 0;
  for (uint32_t i = 0; i < desc->m_nkeys; i++)
  {
    const uint32_t *op = desc->m_ops + desc->m_keys[i].m_index;
    assert (insn_key_ok_p (*op));
    switch (DDS_OP_TYPE (*op))
    {
      case DDS_OP_VAL_1BY: if (!normalize_uint8 (&off, size)) return false; break;
      case DDS_OP_VAL_2BY: if (!normalize_uint16 (data, &off, size, bswap)) return false; break;
      case DDS_OP_VAL_4BY: if (!normalize_uint32 (data, &off, size, bswap)) return false; break;
      case DDS_OP_VAL_8BY: if (!normalize_uint64 (data, &off, size, bswap)) return false; break;
      case DDS_OP_VAL_STR: if (!normalize_string (data, &off, size, bswap, SIZE_MAX)) return false; break;
      case DDS_OP_VAL_BST: if (!normalize_string (data, &off, size, bswap, op[2])) return false; break;
      case DDS_OP_VAL_ARR: if (!normalize_arr (data, &off, size, bswap, op, *op)) return false; break;
      case DDS_OP_VAL_SEQ: case DDS_OP_VAL_UNI: case DDS_OP_VAL_STU:
        abort ();
        break;
    }
  }
  return true;
}

bool dds_stream_normalize (void * __restrict data, uint32_t size, bool bswap, const struct ddsi_sertopic_default * __restrict topic, bool just_key)
{
  if (size > CDR_SIZE_MAX)
    return false;
  if (just_key)
    return stream_normalize_key (data, size, bswap, topic->type);
  else
  {
    uint32_t off = 0;
    return stream_normalize (data, &off, size, bswap, topic->type->m_ops);
  }
}

/*******************************************************************************************
 **
 **  Read/write of samples and keys -- i.e., DDSI payloads.
 **
 *******************************************************************************************/

void dds_stream_read_sample (dds_istream_t * __restrict is, void * __restrict data, const struct ddsi_sertopic_default * __restrict topic)
{
  const struct dds_topic_descriptor *desc = topic->type;
  if (topic->opt_size)
    dds_is_get_bytes (is, data, desc->m_size, 1);
  else
  {
    if (desc->m_flagset & DDS_TOPIC_CONTAINS_UNION)
    {
      /* Switching union cases causes big trouble if some cases have sequences or strings,
         and other cases have other things mapped to those addresses.  So, pretend to be
         nice by freeing whatever was allocated, then clearing all memory.  This will
         make any preallocated buffers go to waste, but it does allow reusing the message
         from read-to-read, at the somewhat reasonable price of a slower deserialization
         and not being able to use preallocated sequences in topics containing unions. */
      dds_sample_free_contents (data, desc->m_ops);
      memset (data, 0, desc->m_size);
    }
    dds_stream_read (is, data, desc->m_ops);
  }
}

void dds_stream_write_sample (dds_ostream_t * __restrict os, const void * __restrict data, const struct ddsi_sertopic_default * __restrict topic)
{
  const struct dds_topic_descriptor *desc = topic->type;
  if (topic->opt_size && desc->m_align && (os->m_index % desc->m_align) == 0)
    dds_os_put_bytes (os, data, desc->m_size);
  else
    dds_stream_write (os, data, desc->m_ops);
}

void dds_stream_read_key (dds_istream_t * __restrict is, char * __restrict sample, const struct ddsi_sertopic_default * __restrict topic)
{
  const dds_topic_descriptor_t *desc = topic->type;
  for (uint32_t i = 0; i < desc->m_nkeys; i++)
  {
    const uint32_t *op = desc->m_ops + desc->m_keys[i].m_index;
    char *dst = sample + op[1];
    assert (insn_key_ok_p (*op));
    switch (DDS_OP_TYPE (*op))
    {
      case DDS_OP_VAL_1BY: *((uint8_t *) dst) = dds_is_get1 (is); break;
      case DDS_OP_VAL_2BY: *((uint16_t *) dst) = dds_is_get2 (is); break;
      case DDS_OP_VAL_4BY: *((uint32_t *) dst) = dds_is_get4 (is); break;
      case DDS_OP_VAL_8BY: *((uint64_t *) dst) = dds_is_get8 (is); break;
      case DDS_OP_VAL_STR: *((char **) dst) = dds_stream_reuse_string (is, *((char **) dst)); break;
      case DDS_OP_VAL_BST: dds_stream_reuse_string_bound (is, dst, op[2]); break;
      case DDS_OP_VAL_ARR:
        dds_is_get_bytes (is, dst, op[2], get_type_size (DDS_OP_SUBTYPE (*op)));
        break;
      case DDS_OP_VAL_SEQ: case DDS_OP_VAL_UNI: case DDS_OP_VAL_STU:
        abort ();
        break;
    }
  }
}

void dds_stream_write_key (dds_ostream_t * __restrict os, const char * __restrict sample, const struct ddsi_sertopic_default * __restrict topic)
{
  const struct dds_topic_descriptor *desc = (const struct dds_topic_descriptor *) topic->type;
  for (uint32_t i = 0; i < desc->m_nkeys; i++)
  {
    const uint32_t *insnp = desc->m_ops + desc->m_keys[i].m_index;
    const void *src = sample + insnp[1];
    assert (insn_key_ok_p (*insnp));
    switch (DDS_OP_TYPE (*insnp))
    {
      case DDS_OP_VAL_1BY: dds_os_put1 (os, *((uint8_t *) src)); break;
      case DDS_OP_VAL_2BY: dds_os_put2 (os, *((uint16_t *) src)); break;
      case DDS_OP_VAL_4BY: dds_os_put4 (os, *((uint32_t *) src)); break;
      case DDS_OP_VAL_8BY: dds_os_put8 (os, *((uint64_t *) src)); break;
      case DDS_OP_VAL_STR: dds_stream_write_string (os, *(char **) src); break;
      case DDS_OP_VAL_BST: dds_stream_write_string (os, src); break;
      case DDS_OP_VAL_ARR: {
        const uint32_t elem_size = get_type_size (DDS_OP_SUBTYPE (*insnp));
        const uint32_t num = insnp[2];
        dds_cdr_alignto_clear_and_resize(os, elem_size, num * elem_size);
        dds_os_put_bytes (os, src, num * elem_size);
        break;
      }
      case DDS_OP_VAL_SEQ: case DDS_OP_VAL_UNI: case DDS_OP_VAL_STU: {
        abort ();
        break;
      }
    }
  }
}

#if DDSRT_ENDIAN == DDSRT_LITTLE_ENDIAN
static void dds_stream_swap_insitu (void * __restrict vbuf, uint32_t size, uint32_t num)
{
  assert (size == 1 || size == 2 || size == 4 || size == 8);
  switch (size)
  {
    case 1:
      break;
    case 2: {
      uint16_t *buf = vbuf;
      for (uint32_t i = 0; i < num; i++)
        buf[i] = ddsrt_bswap2u (buf[i]);
      break;
    }
    case 4: {
      uint32_t *buf = vbuf;
      for (uint32_t i = 0; i < num; i++)
        buf[i] = ddsrt_bswap4u (buf[i]);
      break;
    }
    case 8: {
      uint64_t *buf = vbuf;
      for (uint32_t i = 0; i < num; i++)
        buf[i] = ddsrt_bswap8u (buf[i]);
      break;
    }
  }
}

void dds_stream_write_keyBE (dds_ostreamBE_t * __restrict os, const char * __restrict sample, const struct ddsi_sertopic_default * __restrict topic)
{
  const struct dds_topic_descriptor *desc = (const struct dds_topic_descriptor *) topic->type;
  for (uint32_t i = 0; i < desc->m_nkeys; i++)
  {
    const uint32_t *insnp = desc->m_ops + desc->m_keys[i].m_index;
    const void *src = sample + insnp[1];
    assert (insn_key_ok_p (*insnp));
    switch (DDS_OP_TYPE (*insnp))
    {
      case DDS_OP_VAL_1BY: dds_os_put1be (os, *((uint8_t *) src)); break;
      case DDS_OP_VAL_2BY: dds_os_put2be (os, *((uint16_t *) src)); break;
      case DDS_OP_VAL_4BY: dds_os_put4be (os, *((uint32_t *) src)); break;
      case DDS_OP_VAL_8BY: dds_os_put8be (os, *((uint64_t *) src)); break;
      case DDS_OP_VAL_STR: dds_streamBE_write_string (os, *(char **) src); break;
      case DDS_OP_VAL_BST: dds_streamBE_write_string (os, src); break;
      case DDS_OP_VAL_ARR: {
        const uint32_t elem_size = get_type_size (DDS_OP_SUBTYPE (*insnp));
        const uint32_t num = insnp[2];
        dds_cdr_alignto_clear_and_resize_be (os, elem_size, num * elem_size);
        void * const dst = os->x.m_buffer + os->x.m_index;
        dds_os_put_bytes (&os->x, src, num * elem_size);
        dds_stream_swap_insitu (dst, elem_size, num);
        break;
      }
      case DDS_OP_VAL_SEQ: case DDS_OP_VAL_UNI: case DDS_OP_VAL_STU: {
        abort ();
        break;
      }
    }
  }
}
#elif DDSRT_ENDIAN == DDSRT_BIG_ENDIAN
void dds_stream_write_keyBE (dds_ostreamBE_t * __restrict os, const char * __restrict sample, const struct ddsi_sertopic_default * __restrict topic)
{
  dds_stream_write_key (&os->x, sample, topic);
}
#else
#error "DDSRT_ENDIAN neither LITTLE nor BIG"
#endif

/*******************************************************************************************
 **
 **  Extracting key/keyhash (the only difference that a keyhash MUST be big-endian,
 **  padding MUST be cleared, and that it may be necessary to run the value through
 **  MD5.
 **
 *******************************************************************************************/

static void dds_stream_extract_key_from_data1 (dds_istream_t * __restrict is, dds_ostream_t * __restrict os, const uint32_t * __restrict ops, uint32_t * __restrict keys_remaining);

static void dds_stream_extract_key_from_key_prim_op (dds_istream_t * __restrict is, dds_ostream_t * __restrict os, const uint32_t * __restrict op)
{
  assert ((*op & DDS_OP_FLAG_KEY) && ((DDS_OP (*op)) == DDS_OP_ADR));
  switch (DDS_OP_TYPE (*op))
  {
    case DDS_OP_VAL_1BY: dds_os_put1 (os, dds_is_get1 (is)); break;
    case DDS_OP_VAL_2BY: dds_os_put2 (os, dds_is_get2 (is)); break;
    case DDS_OP_VAL_4BY: dds_os_put4 (os, dds_is_get4 (is)); break;
    case DDS_OP_VAL_8BY: dds_os_put8 (os, dds_is_get8 (is)); break;
    case DDS_OP_VAL_STR: case DDS_OP_VAL_BST: {
      uint32_t sz = dds_is_get4 (is);
      dds_os_put4 (os, sz);
      dds_os_put_bytes (os, is->m_buffer + is->m_index, sz);
      is->m_index += sz;
      break;
    }
    case DDS_OP_VAL_ARR: {
      const uint32_t subtype = DDS_OP_SUBTYPE (*op);
      assert (subtype <= DDS_OP_VAL_8BY);
      const uint32_t align = get_type_size (subtype);
      const uint32_t num = op[2];
      dds_cdr_alignto_clear_and_resize (os, align, num * align);
      void * const dst = os->m_buffer + os->m_index;
      dds_is_get_bytes (is, dst, num, align);
      os->m_index += num * align;
      is->m_index += num * align;
      break;
    }
    case DDS_OP_VAL_SEQ: case DDS_OP_VAL_UNI: case DDS_OP_VAL_STU: {
      abort ();
      break;
    }
  }
}

#if DDSRT_ENDIAN == DDSRT_LITTLE_ENDIAN
static void dds_stream_swap_copy (void * __restrict vdst, const void * __restrict vsrc, uint32_t size, uint32_t num)
{
  assert (size == 1 || size == 2 || size == 4 || size == 8);
  switch (size)
  {
    case 1:
      memcpy (vdst, vsrc, num);
      break;
    case 2: {
      const uint16_t *src = vsrc;
      uint16_t *dst = vdst;
      for (uint32_t i = 0; i < num; i++)
        dst[i] = ddsrt_bswap2u (src[i]);
      break;
    }
    case 4: {
      const uint32_t *src = vsrc;
      uint32_t *dst = vdst;
      for (uint32_t i = 0; i < num; i++)
        dst[i] = ddsrt_bswap4u (src[i]);
      break;
    }
    case 8: {
      const uint64_t *src = vsrc;
      uint64_t *dst = vdst;
      for (uint32_t i = 0; i < num; i++)
        dst[i] = ddsrt_bswap8u (src[i]);
      break;
    }
  }
}
#endif

static void dds_stream_extract_keyBE_from_key_prim_op (dds_istream_t * __restrict is, dds_ostreamBE_t * __restrict os, const uint32_t * __restrict op)
{
  assert ((*op & DDS_OP_FLAG_KEY) && ((DDS_OP (*op)) == DDS_OP_ADR));
  switch (DDS_OP_TYPE (*op))
  {
    case DDS_OP_VAL_1BY: dds_os_put1be (os, dds_is_get1 (is)); break;
    case DDS_OP_VAL_2BY: dds_os_put2be (os, dds_is_get2 (is)); break;
    case DDS_OP_VAL_4BY: dds_os_put4be (os, dds_is_get4 (is)); break;
    case DDS_OP_VAL_8BY: dds_os_put8be (os, dds_is_get8 (is)); break;
    case DDS_OP_VAL_STR: case DDS_OP_VAL_BST: {
      uint32_t sz = dds_is_get4 (is);
      dds_os_put4be (os, sz);
      dds_os_put_bytes (&os->x, is->m_buffer + is->m_index, sz);
      is->m_index += sz;
      break;
    }
    case DDS_OP_VAL_ARR: {
      const uint32_t subtype = DDS_OP_SUBTYPE (*op);
      assert (subtype <= DDS_OP_VAL_8BY);
      const uint32_t align = get_type_size (subtype);
      const uint32_t num = op[2];
      dds_cdr_alignto (is, align);
      dds_cdr_alignto_clear_and_resize_be (os, align, num * align);
      void const * const src = is->m_buffer + is->m_index;
      void * const dst = os->x.m_buffer + os->x.m_index;
#if DDSRT_ENDIAN == DDSRT_LITTLE_ENDIAN
      dds_stream_swap_copy (dst, src, align, num);
#else
      memcpy (dst, src, num * align);
#endif
      os->x.m_index += num * align;
      is->m_index += num * align;
      break;
    }
    case DDS_OP_VAL_SEQ: case DDS_OP_VAL_UNI: case DDS_OP_VAL_STU: {
      abort ();
      break;
    }
  }
}

static void dds_stream_extract_keyBE_from_key (dds_istream_t * __restrict is, dds_ostreamBE_t * __restrict os, const struct ddsi_sertopic_default * __restrict topic)
{
  const dds_topic_descriptor_t *desc = topic->type;
  for (uint32_t i = 0; i < desc->m_nkeys; i++)
  {
    uint32_t const * const op = desc->m_ops + desc->m_keys[i].m_index;
    dds_stream_extract_keyBE_from_key_prim_op (is, os, op);
  }
}

static void dds_stream_extract_key_from_data_skip_subtype (dds_istream_t * __restrict is, uint32_t num, uint32_t subtype, const uint32_t * __restrict subops)
{
  switch (subtype)
  {
    case DDS_OP_VAL_1BY: case DDS_OP_VAL_2BY: case DDS_OP_VAL_4BY: case DDS_OP_VAL_8BY: {
      const uint32_t elem_size = get_type_size (subtype);
      dds_cdr_alignto (is, elem_size);
      is->m_index += num * elem_size;
      break;
    }
    case DDS_OP_VAL_STR: case DDS_OP_VAL_BST: {
      for (uint32_t i = 0; i < num; i++)
      {
        const uint32_t len = dds_is_get4 (is);
        is->m_index += len;
      }
      break;
    }
    case DDS_OP_VAL_SEQ: case DDS_OP_VAL_ARR: case DDS_OP_VAL_UNI: case DDS_OP_VAL_STU: {
      uint32_t remain = UINT32_MAX;
      for (uint32_t i = 0; i < num; i++)
        dds_stream_extract_key_from_data1 (is, NULL, subops, &remain);
      break;
    }
  }
}

static const uint32_t *dds_stream_extract_key_from_data_skip_array (dds_istream_t * __restrict is, const uint32_t * __restrict ops)
{
  const uint32_t op = *ops;
  assert (DDS_OP_TYPE (op) == DDS_OP_VAL_ARR);
  const uint32_t subtype = DDS_OP_SUBTYPE (op);
  const uint32_t num = ops[2];
  if (subtype > DDS_OP_VAL_BST)
  {
    const uint32_t *jsr_ops = ops + DDS_OP_ADR_JSR (ops[3]);
    const uint32_t jmp = DDS_OP_ADR_JMP (ops[3]);
    dds_stream_extract_key_from_data_skip_subtype (is, num, subtype, jsr_ops);
    return ops + (jmp ? jmp : 5);
  }
  else
  {
    dds_stream_extract_key_from_data_skip_subtype (is, num, subtype, NULL);
    return ops + 3;
  }
}

static const uint32_t *dds_stream_extract_key_from_data_skip_sequence (dds_istream_t * __restrict is, const uint32_t * __restrict ops)
{
  const uint32_t op = *ops;
  const enum dds_stream_typecode type = DDS_OP_TYPE (op);
  assert (type == DDS_OP_VAL_SEQ);
  const uint32_t subtype = DDS_OP_SUBTYPE (op);
  const uint32_t num = dds_is_get4 (is);
  if (num == 0)
    return ops + 2 + (type == DDS_OP_VAL_BST || type == DDS_OP_VAL_ARR);
  else if (subtype > DDS_OP_VAL_BST)
  {
    const uint32_t * jsr_ops = ops + DDS_OP_ADR_JSR (ops[3]);
    const uint32_t jmp = DDS_OP_ADR_JMP (ops[3]);
    dds_stream_extract_key_from_data_skip_subtype (is, num, subtype, jsr_ops);
    return ops + (jmp ? jmp : 4);
  }
  else
  {
    dds_stream_extract_key_from_data_skip_subtype (is, num, subtype, NULL);
    return ops + 2 + (type == DDS_OP_VAL_BST || type == DDS_OP_VAL_ARR);
  }
}

static const uint32_t *dds_stream_extract_key_from_data_skip_union (dds_istream_t * __restrict is, const uint32_t * __restrict ops)
{
  const uint32_t op = *ops;
  assert (DDS_OP_TYPE (op) == DDS_OP_VAL_UNI);
  const uint32_t disc = read_union_discriminant (is, DDS_OP_SUBTYPE (op));
  uint32_t const * const jeq_op = find_union_case (ops, disc);
  if (jeq_op)
    dds_stream_extract_key_from_data_skip_subtype (is, 1, DDS_JEQ_TYPE (jeq_op[0]), jeq_op + DDS_OP_ADR_JSR (jeq_op[0]));
  return ops + DDS_OP_ADR_JMP (ops[3]);
}

static void dds_stream_extract_key_from_data1 (dds_istream_t * __restrict is, dds_ostream_t * __restrict os, const uint32_t * __restrict ops, uint32_t * __restrict keys_remaining)
{
  uint32_t op;
  while ((op = *ops) != DDS_OP_RTS)
  {
    switch (DDS_OP (op))
    {
      case DDS_OP_ADR: {
        const uint32_t type = DDS_OP_TYPE (op);
        const bool is_key = (op & DDS_OP_FLAG_KEY) && (os != NULL);
        if (is_key)
        {
          dds_stream_extract_key_from_key_prim_op (is, os, ops);
          if (--(*keys_remaining) == 0)
            return;
          ops += 2 + (type == DDS_OP_VAL_BST || type == DDS_OP_VAL_ARR);
        }
        else
        {
          switch (type)
          {
            case DDS_OP_VAL_1BY: case DDS_OP_VAL_2BY: case DDS_OP_VAL_4BY: case DDS_OP_VAL_8BY: case DDS_OP_VAL_STR: case DDS_OP_VAL_BST:
              dds_stream_extract_key_from_data_skip_subtype (is, 1, type, NULL);
              ops += 2 + (type == DDS_OP_VAL_BST || type == DDS_OP_VAL_ARR);
              break;
            case DDS_OP_VAL_SEQ:
              ops = dds_stream_extract_key_from_data_skip_sequence (is, ops);
              break;
            case DDS_OP_VAL_ARR:
              ops = dds_stream_extract_key_from_data_skip_array (is, ops);
              break;
            case DDS_OP_VAL_UNI:
              ops = dds_stream_extract_key_from_data_skip_union (is, ops);
              break;
            case DDS_OP_VAL_STU:
              abort ();
          }
        }
        break;
      }
      case DDS_OP_JSR: { /* Implies nested type */
        ops += 2;
        dds_stream_extract_key_from_data1 (is, os, ops + DDS_OP_JUMP (op), keys_remaining);
        if (--(*keys_remaining) == 0)
          return;
        ops++;
        break;
      }
      case DDS_OP_RTS: case DDS_OP_JEQ: {
        abort ();
        break;
      }
    }
  }
}

static void dds_stream_extract_keyBE_from_data1 (dds_istream_t * __restrict is, dds_ostreamBE_t * __restrict os, const uint32_t * __restrict ops, uint32_t * __restrict keys_remaining)
{
  uint32_t op;
  while ((op = *ops) != DDS_OP_RTS)
  {
    switch (DDS_OP (op))
    {
      case DDS_OP_ADR: {
        const uint32_t type = DDS_OP_TYPE (op);
        const bool is_key = (op & DDS_OP_FLAG_KEY) && (os != NULL);
        if (is_key)
        {
          dds_stream_extract_keyBE_from_key_prim_op (is, os, ops);
          if (--(*keys_remaining) == 0)
            return;
          ops += 2 + (type == DDS_OP_VAL_BST || type == DDS_OP_VAL_ARR);
        }
        else
        {
          switch (type)
          {
            case DDS_OP_VAL_1BY: case DDS_OP_VAL_2BY: case DDS_OP_VAL_4BY: case DDS_OP_VAL_8BY: case DDS_OP_VAL_STR: case DDS_OP_VAL_BST:
              dds_stream_extract_key_from_data_skip_subtype (is, 1, type, NULL);
              ops += 2 + (type == DDS_OP_VAL_BST || type == DDS_OP_VAL_ARR);
              break;
            case DDS_OP_VAL_SEQ:
              ops = dds_stream_extract_key_from_data_skip_sequence (is, ops);
              break;
            case DDS_OP_VAL_ARR:
              ops = dds_stream_extract_key_from_data_skip_array (is, ops);
              break;
            case DDS_OP_VAL_UNI:
              ops = dds_stream_extract_key_from_data_skip_union (is, ops);
              break;
            case DDS_OP_VAL_STU:
              abort ();
          }
        }
        break;
      }
      case DDS_OP_JSR: { /* Implies nested type */
        ops += 2;
        dds_stream_extract_keyBE_from_data1 (is, os, ops + DDS_OP_JUMP (op), keys_remaining);
        if (--(*keys_remaining) == 0)
          return;
        ops++;
        break;
      }
      case DDS_OP_RTS: case DDS_OP_JEQ: {
        abort ();
        break;
      }
    }
  }
}

void dds_stream_extract_key_from_data (dds_istream_t * __restrict is, dds_ostream_t * __restrict os, const struct ddsi_sertopic_default * __restrict topic)
{
  const dds_topic_descriptor_t *desc = topic->type;
  uint32_t keys_remaining = desc->m_nkeys;
  dds_stream_extract_key_from_data1 (is, os, desc->m_ops, &keys_remaining);
}

void dds_stream_extract_keyBE_from_data (dds_istream_t * __restrict is, dds_ostreamBE_t * __restrict os, const struct ddsi_sertopic_default * __restrict topic)
{
  const dds_topic_descriptor_t *desc = topic->type;
  uint32_t keys_remaining = desc->m_nkeys;
  dds_stream_extract_keyBE_from_data1 (is, os, desc->m_ops, &keys_remaining);
}

void dds_stream_extract_keyhash (dds_istream_t * __restrict is, dds_keyhash_t * __restrict kh, const struct ddsi_sertopic_default * __restrict topic, const bool just_key)
{
  const dds_topic_descriptor_t *desc = topic->type;
  kh->m_set = 1;
  if (desc->m_nkeys == 0)
  {
    kh->m_iskey = 1;
    kh->m_keysize = 0;
  }
  else if (desc->m_flagset & DDS_TOPIC_FIXED_KEY)
  {
    dds_ostreamBE_t os;
    kh->m_iskey = 1;
    dds_ostreamBE_init (&os, 0);
    os.x.m_buffer = kh->m_hash;
    os.x.m_size = 16;
    if (just_key)
      dds_stream_extract_keyBE_from_key (is, &os, topic);
    else
      dds_stream_extract_keyBE_from_data (is, &os, topic);
    assert (os.x.m_index <= 16);
    kh->m_keysize = (unsigned)os.x.m_index & 0x1f;
  }
  else
  {
    dds_ostreamBE_t os;
    ddsrt_md5_state_t md5st;
    kh->m_iskey = 0;
    kh->m_keysize = 16;
    dds_ostreamBE_init (&os, 0);
    if (just_key)
      dds_stream_extract_keyBE_from_key (is, &os, topic);
    else
      dds_stream_extract_keyBE_from_data (is, &os, topic);
    ddsrt_md5_init (&md5st);
    ddsrt_md5_append (&md5st, os.x.m_buffer, os.x.m_index);
    ddsrt_md5_finish (&md5st, kh->m_hash);
    dds_ostreamBE_fini (&os);
  }
}

/*******************************************************************************************
 **
 **  Pretty-printing
 **
 *******************************************************************************************/

/* Returns true if buffer not yet exhausted, false otherwise */
static bool prtf (char * __restrict *buf, size_t * __restrict bufsize, const char *fmt, ...)
{
  va_list ap;
  if (*bufsize == 0)
    return false;
  va_start (ap, fmt);
  int n = vsnprintf (*buf, *bufsize, fmt, ap);
  va_end (ap);
  if (n < 0)
  {
    **buf = 0;
    return false;
  }
  else if ((size_t) n <= *bufsize)
  {
    *buf += (size_t) n;
    *bufsize -= (size_t) n;
    return (*bufsize > 0);
  }
  else
  {
    *buf += *bufsize;
    *bufsize = 0;
    return false;
  }
}

static bool prtf_str (char * __restrict *buf, size_t * __restrict bufsize, dds_istream_t * __restrict is)
{
  size_t sz = dds_is_get4 (is);
  bool ret = prtf (buf, bufsize, "\"%s\"", is->m_buffer + is->m_index);
  is->m_index += (uint32_t) sz;
  return ret;
}

static size_t isprint_runlen (const unsigned char *s, size_t n)
{
  size_t m;
  for (m = 0; m < n && s[m] != '"' && isprint (s[m]); m++)
    ;
  return m;
}

static bool prtf_simple (char * __restrict *buf, size_t * __restrict bufsize, dds_istream_t * __restrict is, enum dds_stream_typecode type)
{
  switch (type)
  {
    case DDS_OP_VAL_1BY: return prtf (buf, bufsize, "%"PRIu8, dds_is_get1 (is));
    case DDS_OP_VAL_2BY: return prtf (buf, bufsize, "%"PRIu16, dds_is_get2 (is));
    case DDS_OP_VAL_4BY: return prtf (buf, bufsize, "%"PRIu32, dds_is_get4 (is));
    case DDS_OP_VAL_8BY: return prtf (buf, bufsize, "%"PRIu64, dds_is_get8 (is));
    case DDS_OP_VAL_STR: case DDS_OP_VAL_BST: return prtf_str (buf, bufsize, is);
    case DDS_OP_VAL_ARR: case DDS_OP_VAL_SEQ: case DDS_OP_VAL_UNI: case DDS_OP_VAL_STU:
      abort ();
  }
  return false;
}

static bool prtf_simple_array (char * __restrict *buf, size_t * __restrict bufsize, dds_istream_t * __restrict is, uint32_t num, enum dds_stream_typecode type)
{
  bool cont = prtf (buf, bufsize, "{");
  switch (type)
  {
    case DDS_OP_VAL_1BY: {
      size_t i = 0, j;
      while (cont && i < num)
      {
        size_t m = isprint_runlen ((unsigned char *) (is->m_buffer + is->m_index), num - i);
        if (m >= 4)
        {
          cont = prtf (buf, bufsize, "%s\"", i != 0 ? "," : "");
          for (j = 0; cont && j < m; j++)
            cont = prtf (buf, bufsize, "%c", is->m_buffer[is->m_index + j]);
          cont = prtf (buf, bufsize, "\"");
          is->m_index += (uint32_t) m;
          i += m;
        }
        else
        {
          if (i != 0)
            (void) prtf (buf, bufsize, ",");
          cont = prtf_simple (buf, bufsize, is, type);
          i++;
        }
      }
      break;
    }
    case DDS_OP_VAL_2BY: case DDS_OP_VAL_4BY: case DDS_OP_VAL_8BY:
    case DDS_OP_VAL_STR: case DDS_OP_VAL_BST:
      for (size_t i = 0; cont && i < num; i++)
      {
        if (i != 0)
          (void) prtf (buf, bufsize, ",");
        cont = prtf_simple (buf, bufsize, is, type);
      }
      break;
    default:
      abort ();
      break;
  }
  return prtf (buf, bufsize, "}");
}

static bool dds_stream_print_sample1 (char * __restrict *buf, size_t * __restrict bufsize, dds_istream_t * __restrict is, const uint32_t * __restrict ops, bool add_braces);

static const uint32_t *prtf_seq (char * __restrict *buf, size_t *bufsize, dds_istream_t * __restrict is, const uint32_t * __restrict ops, uint32_t insn)
{
  const enum dds_stream_typecode subtype = DDS_OP_SUBTYPE (insn);
  uint32_t num;
  num = dds_is_get4 (is);
  if (num == 0)
  {
    (void) prtf (buf, bufsize, "{}");
    return skip_sequence_insns (ops, insn);
  }
  switch (subtype)
  {
    case DDS_OP_VAL_1BY: case DDS_OP_VAL_2BY: case DDS_OP_VAL_4BY: case DDS_OP_VAL_8BY:
      (void) prtf_simple_array (buf, bufsize, is, num, subtype);
      return ops + 2;
    case DDS_OP_VAL_STR: case DDS_OP_VAL_BST:
      (void) prtf_simple_array (buf, bufsize, is, num, subtype);
      return ops + (subtype == DDS_OP_VAL_STR ? 2 : 3);
    case DDS_OP_VAL_SEQ: case DDS_OP_VAL_ARR: case DDS_OP_VAL_UNI: case DDS_OP_VAL_STU: {
      const uint32_t jmp = DDS_OP_ADR_JMP (ops[3]);
      uint32_t const * const jsr_ops = ops + DDS_OP_ADR_JSR (ops[3]);
      bool cont = prtf (buf, bufsize, "{");
      for (uint32_t i = 0; cont && i < num; i++)
      {
        if (i > 0) (void) prtf (buf, bufsize, ",");
        cont = dds_stream_print_sample1 (buf, bufsize, is, jsr_ops, subtype == DDS_OP_VAL_STU);
      }
      (void) prtf (buf, bufsize, "}");
      return ops + (jmp ? jmp : 4); /* FIXME: why would jmp be 0? */
    }
  }
  return NULL;
}

static const uint32_t *prtf_arr (char * __restrict *buf, size_t *bufsize, dds_istream_t * __restrict is, const uint32_t * __restrict ops, uint32_t insn)
{
  const enum dds_stream_typecode subtype = DDS_OP_SUBTYPE (insn);
  const uint32_t num = ops[2];
  switch (subtype)
  {
    case DDS_OP_VAL_1BY: case DDS_OP_VAL_2BY: case DDS_OP_VAL_4BY: case DDS_OP_VAL_8BY:
      (void) prtf_simple_array (buf, bufsize, is, num, subtype);
      return ops + 3;
    case DDS_OP_VAL_STR: case DDS_OP_VAL_BST:
      (void) prtf_simple_array (buf, bufsize, is, num, subtype);
      return ops + (subtype == DDS_OP_VAL_STR ? 3 : 5);
    case DDS_OP_VAL_SEQ: case DDS_OP_VAL_ARR: case DDS_OP_VAL_UNI: case DDS_OP_VAL_STU: {
      const uint32_t *jsr_ops = ops + DDS_OP_ADR_JSR (ops[3]);
      const uint32_t jmp = DDS_OP_ADR_JMP (ops[3]);
      bool cont = prtf (buf, bufsize, "{");
      for (uint32_t i = 0; cont && i < num; i++)
      {
        if (i > 0) (void) prtf (buf, bufsize, ",");
        cont = dds_stream_print_sample1 (buf, bufsize, is, jsr_ops, subtype == DDS_OP_VAL_STU);
      }
      (void) prtf (buf, bufsize, "}");
      return ops + (jmp ? jmp : 5);
    }
  }
  return NULL;
}

static const uint32_t *prtf_uni (char * __restrict *buf, size_t *bufsize, dds_istream_t * __restrict is, const uint32_t * __restrict ops, uint32_t insn)
{
  const uint32_t disc = read_union_discriminant (is, DDS_OP_SUBTYPE (insn));
  uint32_t const * const jeq_op = find_union_case (ops, disc);
  (void) prtf (buf, bufsize, "%"PRIu32":", disc);
  ops += DDS_OP_ADR_JMP (ops[3]);
  if (jeq_op)
  {
    const enum dds_stream_typecode valtype = DDS_JEQ_TYPE (jeq_op[0]);
    switch (valtype)
    {
      case DDS_OP_VAL_1BY: case DDS_OP_VAL_2BY: case DDS_OP_VAL_4BY: case DDS_OP_VAL_8BY:
      case DDS_OP_VAL_STR: case DDS_OP_VAL_BST:
        (void) prtf_simple (buf, bufsize, is, valtype);
        break;
      case DDS_OP_VAL_SEQ: case DDS_OP_VAL_ARR: case DDS_OP_VAL_UNI: case DDS_OP_VAL_STU:
        (void) dds_stream_print_sample1 (buf, bufsize, is, jeq_op + DDS_OP_ADR_JSR (jeq_op[0]), valtype == DDS_OP_VAL_STU);
        break;
    }
  }
  return ops;
}

static bool dds_stream_print_sample1 (char * __restrict *buf, size_t * __restrict bufsize, dds_istream_t * __restrict is, const uint32_t * __restrict ops, bool add_braces)
{
  uint32_t insn;
  bool cont = true;
  bool needs_comma = false;
  if (add_braces)
    (void) prtf (buf, bufsize, "{");
  while (cont && (insn = *ops) != DDS_OP_RTS)
  {
    if (needs_comma)
      (void) prtf (buf, bufsize, ",");
    needs_comma = true;
    switch (DDS_OP (insn))
    {
      case DDS_OP_ADR: {
        switch (DDS_OP_TYPE (insn))
        {
          case DDS_OP_VAL_1BY: case DDS_OP_VAL_2BY: case DDS_OP_VAL_4BY: case DDS_OP_VAL_8BY:
          case DDS_OP_VAL_STR:
            cont = prtf_simple (buf, bufsize, is, DDS_OP_TYPE (insn));
            ops += 2;
            break;
          case DDS_OP_VAL_BST:
            cont = prtf_simple (buf, bufsize, is, DDS_OP_TYPE (insn));
            ops += 3;
            break;
          case DDS_OP_VAL_SEQ:
            ops = prtf_seq (buf, bufsize, is, ops, insn);
            break;
          case DDS_OP_VAL_ARR:
            ops = prtf_arr (buf, bufsize, is, ops, insn);
            break;
          case DDS_OP_VAL_UNI:
            ops = prtf_uni (buf, bufsize, is, ops, insn);
            break;
          case DDS_OP_VAL_STU:
            abort ();
            break;
        }
        break;
      }
      case DDS_OP_JSR: {
        cont = dds_stream_print_sample1 (buf, bufsize, is, ops + DDS_OP_JUMP (insn), true);
        ops++;
        break;
      }
      case DDS_OP_RTS: case DDS_OP_JEQ: {
        abort ();
        break;
      }
    }
  }
  if (add_braces)
    (void) prtf (buf, bufsize, "}");
  return cont;
}

size_t dds_stream_print_sample (dds_istream_t * __restrict is, const struct ddsi_sertopic_default * __restrict topic, char * __restrict buf, size_t bufsize)
{
  (void) dds_stream_print_sample1 (&buf, &bufsize, is, topic->type->m_ops, true);
  return bufsize;
}

size_t dds_stream_print_key (dds_istream_t * __restrict is, const struct ddsi_sertopic_default * __restrict topic, char * __restrict buf, size_t bufsize)
{
  const dds_topic_descriptor_t *desc = topic->type;
  bool cont = prtf (&buf, &bufsize, ":k:{");
  for (uint32_t i = 0; cont && i < desc->m_nkeys; i++)
  {
    const uint32_t *op = desc->m_ops + desc->m_keys[i].m_index;
    assert (insn_key_ok_p (*op));
    switch (DDS_OP_TYPE (*op))
    {
      case DDS_OP_VAL_1BY: case DDS_OP_VAL_2BY: case DDS_OP_VAL_4BY: case DDS_OP_VAL_8BY:
      case DDS_OP_VAL_STR: case DDS_OP_VAL_BST:
        cont = prtf_simple (&buf, &bufsize, is, DDS_OP_TYPE (*op));
        break;
      case DDS_OP_VAL_ARR:
        cont = prtf_simple_array (&buf, &bufsize, is, op[2], DDS_OP_SUBTYPE (*op));
        break;
      case DDS_OP_VAL_SEQ: case DDS_OP_VAL_UNI: case DDS_OP_VAL_STU:
        abort ();
        break;
    }
  }
  (void) prtf (&buf, &bufsize, "}");
  return bufsize;
}

/*******************************************************************************************
 **
 **  Stuff to make it possible to treat a ddsi_serdata_default as a stream
 **
 *******************************************************************************************/

DDSRT_STATIC_ASSERT ((offsetof (struct ddsi_serdata_default, data) % 8) == 0);

void dds_istream_from_serdata_default (dds_istream_t * __restrict s, const struct ddsi_serdata_default * __restrict d)
{
  s->m_buffer = (const unsigned char *) d;
  s->m_index = (uint32_t) offsetof (struct ddsi_serdata_default, data);
  s->m_size = d->size + s->m_index;
#if DDSRT_ENDIAN == DDSRT_LITTLE_ENDIAN
  assert (d->hdr.identifier == CDR_LE);
#elif DDSRT_ENDIAN == DDSRT_BIG_ENDIAN
  assert (d->hdr.identifier == CDR_BE);
#else
#error "DDSRT_ENDIAN neither LITTLE nor BIG"
#endif
}

void dds_ostream_from_serdata_default (dds_ostream_t * __restrict s, struct ddsi_serdata_default * __restrict d)
{
  s->m_buffer = (unsigned char *) d;
  s->m_index = (uint32_t) offsetof (struct ddsi_serdata_default, data);
  s->m_size = d->size + s->m_index;
#if DDSRT_ENDIAN == DDSRT_LITTLE_ENDIAN
  assert (d->hdr.identifier == CDR_LE);
#elif DDSRT_ENDIAN == DDSRT_BIG_ENDIAN
  assert (d->hdr.identifier == CDR_BE);
#else
#error "DDSRT_ENDIAN neither LITTLE nor BIG"
#endif
}

void dds_ostream_add_to_serdata_default (dds_ostream_t * __restrict s, struct ddsi_serdata_default ** __restrict d)
{
  /* DDSI requires 4 byte alignment */

  const uint32_t pad = dds_cdr_alignto_clear_and_resize (s, 4, 0);
  assert (pad <= 3);

  /* Reset data pointer as stream may have reallocated */

  (*d) = (void *) s->m_buffer;
  (*d)->pos = (s->m_index - (uint32_t) offsetof (struct ddsi_serdata_default, data));
  (*d)->size = (s->m_size - (uint32_t) offsetof (struct ddsi_serdata_default, data));
  (*d)->hdr.options = ddsrt_toBE2u ((uint16_t) pad);
}

void dds_ostreamBE_from_serdata_default (dds_ostreamBE_t * __restrict s, struct ddsi_serdata_default * __restrict d)
{
  s->x.m_buffer = (unsigned char *) d;
  s->x.m_index = (uint32_t) offsetof (struct ddsi_serdata_default, data);
  s->x.m_size = d->size + s->x.m_index;
  assert (d->hdr.identifier == CDR_BE);
}

void dds_ostreamBE_add_to_serdata_default (dds_ostreamBE_t * __restrict s, struct ddsi_serdata_default ** __restrict d)
{
  /* DDSI requires 4 byte alignment */

  const uint32_t pad = dds_cdr_alignto_clear_and_resize_be (s, 4, 0);
  assert (pad <= 3);

  /* Reset data pointer as stream may have reallocated */

  (*d) = (void *) s->x.m_buffer;
  (*d)->pos = (s->x.m_index - (uint32_t) offsetof (struct ddsi_serdata_default, data));
  (*d)->size = (s->x.m_size - (uint32_t) offsetof (struct ddsi_serdata_default, data));
  (*d)->hdr.options = ddsrt_toBE2u ((uint16_t) pad);
}
