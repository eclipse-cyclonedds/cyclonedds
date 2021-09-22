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
#include "dds/ddsrt/static_assert.h"
#include "dds/ddsi/q_bswap.h"
#include "dds/ddsi/q_config.h"
#include "dds/ddsi/ddsi_cdrstream.h"
#include "dds__alloc.h"

#define TOKENPASTE(a, b) a ## b
#define TOKENPASTE2(a, b) TOKENPASTE(a, b)
#define TOKENPASTE3(a, b, c) TOKENPASTE2(a, TOKENPASTE2(b, c))

#define NAME_BYTE_ORDER(name) TOKENPASTE2(name, NAME_BYTE_ORDER_EXT)
#define NAME2_BYTE_ORDER(prefix, postfix) TOKENPASTE3(prefix, NAME_BYTE_ORDER_EXT, postfix)
#define DDS_OSTREAM_T TOKENPASTE3(dds_ostream, NAME_BYTE_ORDER_EXT, _t)

#define EMHEADER_FLAG_MASK            0x80000000u
#define EMHEADER_FLAG_MUSTUNDERSTAND  (1u << 31)
#define EMHEADER_LENGTH_CODE_MASK     0x70000000u
#define EMHEADER_LENGTH_CODE(x)       (((x) & EMHEADER_LENGTH_CODE_MASK) >> 28)
#define EMHEADER_MEMBERID_MASK        0x0fffffffu
#define EMHEADER_MEMBERID(x)          ((x) & EMHEADER_MEMBERID_MASK)

/* Length code as defined in section 7.4.3.4.2 of the XTypes spec. Values 4..7 indicate
   that the 32 bits integer that follows the EMHEADER is used to get the length of the
   member. For length code value 4, this integer is added only for this purpose. For values
   5..7 the length of the member is re-used, which is at the first position of the
   member data.
*/
#define LENGTH_CODE_1B              0
#define LENGTH_CODE_2B              1
#define LENGTH_CODE_4B              2
#define LENGTH_CODE_8B              3
#define LENGTH_CODE_NEXTINT         4
#define LENGTH_CODE_ALSO_NEXTINT    5
#define LENGTH_CODE_ALSO_NEXTINT4   6
#define LENGTH_CODE_ALSO_NEXTINT8   7

#define to_BO4u                                       NAME2_BYTE_ORDER(ddsrt_to, 4u)
#define dds_os_put1BO                                 NAME_BYTE_ORDER(dds_os_put1)
#define dds_os_put2BO                                 NAME_BYTE_ORDER(dds_os_put2)
#define dds_os_put4BO                                 NAME_BYTE_ORDER(dds_os_put4)
#define dds_os_put8BO                                 NAME_BYTE_ORDER(dds_os_put8)
#define dds_os_reserve4BO                             NAME_BYTE_ORDER(dds_os_reserve4)
#define dds_stream_write_stringBO                     NAME_BYTE_ORDER(dds_stream_write_string)
#define dds_stream_writeBO                            NAME_BYTE_ORDER(dds_stream_write)
#define dds_stream_write_seqBO                        NAME_BYTE_ORDER(dds_stream_write_seq)
#define dds_stream_write_arrBO                        NAME_BYTE_ORDER(dds_stream_write_arr)
#define write_union_discriminantBO                    NAME_BYTE_ORDER(write_union_discriminant)
#define dds_stream_write_uniBO                        NAME_BYTE_ORDER(dds_stream_write_uni)
#define dds_stream_writeBO                            NAME_BYTE_ORDER(dds_stream_write)
#define dds_stream_write_plBO                         NAME_BYTE_ORDER(dds_stream_write_pl)
#define dds_stream_write_delimitedBO                  NAME_BYTE_ORDER(dds_stream_write_delimited)
#define dds_stream_write_keyBO                        NAME_BYTE_ORDER(dds_stream_write_key)
#define dds_stream_write_keyBO_impl                   NAME2_BYTE_ORDER(dds_stream_write_key, _impl)
#define dds_cdr_alignto_clear_and_resizeBO            NAME_BYTE_ORDER(dds_cdr_alignto_clear_and_resize)
#define dds_stream_swap_if_needed_insituBO            NAME_BYTE_ORDER(dds_stream_swap_if_needed_insitu)
#define dds_stream_to_BO_insitu                       NAME2_BYTE_ORDER(dds_stream_to_, _insitu)
#define dds_stream_extract_keyBO_from_data            NAME2_BYTE_ORDER(dds_stream_extract_key, _from_data)
#define dds_stream_extract_keyBO_from_data1           NAME2_BYTE_ORDER(dds_stream_extract_key, _from_data1)
#define dds_stream_extract_keyBO_from_key_prim_op     NAME2_BYTE_ORDER(dds_stream_extract_key, _from_key_prim_op)
#define dds_stream_extract_keyBO_from_data_delimited  NAME2_BYTE_ORDER(dds_stream_extract_key, _from_data_delimited)
#define dds_stream_extract_keyBO_from_data_pl         NAME2_BYTE_ORDER(dds_stream_extract_key, _from_data_pl)

static const uint32_t *dds_stream_skip_default (char * __restrict data, const uint32_t * __restrict ops);
static const uint32_t *dds_stream_extract_key_from_data1 (dds_istream_t * __restrict is, dds_ostream_t * __restrict os, const uint32_t * __restrict ops, uint32_t * __restrict keys_remaining);
static const uint32_t *dds_stream_extract_keyBE_from_data1 (dds_istream_t * __restrict is, dds_ostreamBE_t * __restrict os, const uint32_t * __restrict ops, uint32_t * __restrict keys_remaining);

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

void dds_istream_init (dds_istream_t * __restrict st, uint32_t size, const void * __restrict input, uint32_t xcdr_version)
{
  st->m_buffer = input;
  st->m_size = size;
  st->m_index = 0;
  st->m_xcdr_version = xcdr_version;
}

void dds_ostream_init (dds_ostream_t * __restrict st, uint32_t size)
{
  st->m_buffer = NULL;
  st->m_size = 0;
  st->m_index = 0;
  dds_cdr_resize (st, size);
}

void dds_ostreamLE_init (dds_ostreamLE_t * __restrict st, uint32_t size)
{
  dds_ostream_init (&st->x, size);
}

void dds_ostreamBE_init (dds_ostreamBE_t * __restrict st, uint32_t size)
{
  dds_ostream_init (&st->x, size);
}

void dds_istream_fini (dds_istream_t * __restrict st)
{
  (void) st;
}

void dds_ostream_fini (dds_ostream_t * __restrict st)
{
  if (st->m_size)
    dds_free (st->m_buffer);
}

void dds_ostreamLE_fini (dds_ostreamLE_t * __restrict st)
{
  dds_ostream_fini (&st->x);
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

static uint32_t dds_cdr_alignto_clear_and_resizeBE (dds_ostreamBE_t * __restrict s, uint32_t a, uint32_t extra)
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

static uint32_t dds_is_peek4 (dds_istream_t * __restrict s)
{
  dds_cdr_alignto (s, 4);
  uint32_t v = * ((uint32_t *) (s->m_buffer + s->m_index));
  return v;
}

static uint64_t dds_is_get8 (dds_istream_t * __restrict s)
{
  dds_cdr_alignto (s, s->m_xcdr_version == CDR_ENC_VERSION_2 ? 4 : 8);
  size_t off_low = (DDSRT_ENDIAN == DDSRT_LITTLE_ENDIAN) ? 0 : 4, off_high = 4 - off_low;
  uint32_t v_low = * ((uint32_t *) (s->m_buffer + s->m_index + off_low)),
    v_high = * ((uint32_t *) (s->m_buffer + s->m_index + off_high));
  uint64_t v = (uint64_t) v_high << 32 | v_low;
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
  uint32_t align = s->m_xcdr_version == CDR_ENC_VERSION_2 ? 4 : 8;
  dds_cdr_alignto_clear_and_resize (s, align, 8);
  size_t off_low = (DDSRT_ENDIAN == DDSRT_LITTLE_ENDIAN) ? 0 : 4, off_high = 4 - off_low;
  *((uint32_t *) (s->m_buffer + s->m_index + off_low)) = (uint32_t) v;
  *((uint32_t *) (s->m_buffer + s->m_index + off_high)) = (uint32_t) (v >> 32);
  s->m_index += 8;
}

static uint32_t dds_os_reserve4 (dds_ostream_t * __restrict s)
{
  dds_cdr_alignto_clear_and_resize (s, 4, 4);
  s->m_index += 4;
  return s->m_index;
}

static uint32_t dds_os_reserve8 (dds_ostream_t * __restrict s)
{
  uint32_t align = s->m_xcdr_version == CDR_ENC_VERSION_2 ? 4 : 8;
  dds_cdr_alignto_clear_and_resize (s, align, 8);
  s->m_index += 8;
  return s->m_index;
}

static void dds_os_put1LE (dds_ostreamLE_t * __restrict s, uint8_t v)  { dds_os_put1 (&s->x, v); }
static void dds_os_put2LE (dds_ostreamLE_t * __restrict s, uint16_t v) { dds_os_put2 (&s->x, ddsrt_toLE2u (v)); }
static void dds_os_put4LE (dds_ostreamLE_t * __restrict s, uint32_t v) { dds_os_put4 (&s->x, ddsrt_toLE4u (v)); }
static void dds_os_put8LE (dds_ostreamLE_t * __restrict s, uint64_t v) { dds_os_put8 (&s->x, ddsrt_toLE8u (v)); }
static uint32_t dds_os_reserve4LE (dds_ostreamLE_t * __restrict s) { return dds_os_reserve4 (&s->x); }
static uint32_t dds_os_reserve8LE (dds_ostreamLE_t * __restrict s) { return dds_os_reserve8 (&s->x); }

static void dds_os_put1BE (dds_ostreamBE_t * __restrict s, uint8_t v)  { dds_os_put1 (&s->x, v); }
static void dds_os_put2BE (dds_ostreamBE_t * __restrict s, uint16_t v) { dds_os_put2 (&s->x, ddsrt_toBE2u (v)); }
static void dds_os_put4BE (dds_ostreamBE_t * __restrict s, uint32_t v) { dds_os_put4 (&s->x, ddsrt_toBE4u (v)); }
static void dds_os_put8BE (dds_ostreamBE_t * __restrict s, uint64_t v) { dds_os_put8 (&s->x, ddsrt_toBE8u (v)); }
static uint32_t dds_os_reserve4BE (dds_ostreamBE_t * __restrict s) { return dds_os_reserve4 (&s->x); }
static uint32_t dds_os_reserve8BE (dds_ostreamBE_t * __restrict s) { return dds_os_reserve8 (&s->x); }

static void dds_stream_swap (void * __restrict vbuf, uint32_t size, uint32_t num)
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

static void dds_os_put_bytes (dds_ostream_t * __restrict s, const void * __restrict b, uint32_t l)
{
  dds_cdr_resize (s, l);
  memcpy (s->m_buffer + s->m_index, b, l);
  s->m_index += l;
}

static void dds_os_put_bytes_aligned (dds_ostream_t * __restrict os, const void * __restrict data, uint32_t num, uint32_t elem_sz, uint32_t align, void **dst)
{
  const uint32_t sz = num * elem_sz;
  dds_cdr_alignto_clear_and_resize (os, align, sz);
  if (dst)
    *dst = os->m_buffer + os->m_index;
  memcpy (os->m_buffer + os->m_index, data, sz);
  os->m_index += sz;
}

static uint32_t get_type_size (enum dds_stream_typecode type)
{
  DDSRT_STATIC_ASSERT (DDS_OP_VAL_1BY == 1 && DDS_OP_VAL_2BY == 2 && DDS_OP_VAL_4BY == 3 && DDS_OP_VAL_8BY == 4);
  assert (type == DDS_OP_VAL_1BY || type == DDS_OP_VAL_2BY || type == DDS_OP_VAL_4BY || type == DDS_OP_VAL_8BY || type == DDS_OP_VAL_ENU);
  if (type == DDS_OP_VAL_ENU)
    return 4;
  return (uint32_t)1 << ((uint32_t) type - 1);
}

static bool type_has_subtype_or_members (enum dds_stream_typecode type)
{
  return type == DDS_OP_VAL_SEQ || type == DDS_OP_VAL_ARR || type == DDS_OP_VAL_UNI || type == DDS_OP_VAL_STU;
}

static size_t dds_stream_check_optimize1 (const struct ddsi_sertype_default_desc * __restrict desc)
{
  const uint32_t *ops = desc->ops.ops;
  size_t off = 0, size;
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
      case DDS_OP_VAL_ENU:
        size = get_type_size (DDS_OP_TYPE (insn));
        if (off % size)
          off += size - (off % size);
        if (ops[1] != off)
          return 0;
        off += size;
        ops += 2;
        if (DDS_OP_TYPE (insn) == DDS_OP_VAL_ENU)
          ops++;
        break;
      case DDS_OP_VAL_ARR:
        switch (DDS_OP_SUBTYPE (insn))
        {
          case DDS_OP_VAL_1BY:
          case DDS_OP_VAL_2BY:
          case DDS_OP_VAL_4BY:
          case DDS_OP_VAL_8BY:
          case DDS_OP_VAL_ENU:
            size = get_type_size (DDS_OP_SUBTYPE (insn));
            if (off % size)
              off += size - (off % size);
            if (ops[1] != off)
              return 0;
            off += size * ops[2];
            ops += 3;
            if (DDS_OP_SUBTYPE (insn) == DDS_OP_VAL_ENU)
              ops++;
            break;
          default:
            return 0;
        }
        break;

      default:
        return 0;
    }
  }

  // off < desc can occur if desc->size includes "trailing" padding
  assert (off <= desc->size);
  return off;
}

size_t dds_stream_check_optimize (const struct ddsi_sertype_default_desc * __restrict desc)
{
  return dds_stream_check_optimize1 (desc);
}

static void dds_stream_countops1 (const uint32_t * __restrict ops, const uint32_t **ops_end, bool *dynamic_type);

static const uint32_t *dds_stream_countops_seq (const uint32_t * __restrict ops, uint32_t insn, const uint32_t **ops_end, bool *dynamic_type)
{
  const enum dds_stream_typecode subtype = DDS_OP_SUBTYPE (insn);
  switch (subtype)
  {
    case DDS_OP_VAL_1BY: case DDS_OP_VAL_2BY: case DDS_OP_VAL_4BY: case DDS_OP_VAL_8BY:
    case DDS_OP_VAL_STR:
      ops += 2;
      break;
    case DDS_OP_VAL_BST: case DDS_OP_VAL_BSP: case DDS_OP_VAL_ENU:
      ops += 3;
      break;
    case DDS_OP_VAL_SEQ: case DDS_OP_VAL_ARR: case DDS_OP_VAL_UNI: case DDS_OP_VAL_STU: {
      const uint32_t jmp = DDS_OP_ADR_JMP (ops[3]);
      uint32_t const * const jsr_ops = ops + DDS_OP_ADR_JSR (ops[3]);
      if (ops + 4 > *ops_end)
        *ops_end = ops + 4;
      if (DDS_OP_ADR_JSR (ops[3]) > 0)
        dds_stream_countops1 (jsr_ops, ops_end, dynamic_type);
      ops += (jmp ? jmp : 4); /* FIXME: why would jmp be 0? */
      break;
    }
    case DDS_OP_VAL_EXT:
      abort (); // not allowed
      break;
  }
  if (ops > *ops_end)
    *ops_end = ops;
  return ops;
}

static const uint32_t *dds_stream_countops_arr (const uint32_t * __restrict ops, uint32_t insn, const uint32_t **ops_end, bool *dynamic_type)
{
  const enum dds_stream_typecode subtype = DDS_OP_SUBTYPE (insn);
  switch (subtype)
  {
    case DDS_OP_VAL_1BY: case DDS_OP_VAL_2BY: case DDS_OP_VAL_4BY: case DDS_OP_VAL_8BY:
    case DDS_OP_VAL_STR:
      ops += 3;
      break;
    case DDS_OP_VAL_BST: case DDS_OP_VAL_BSP: case DDS_OP_VAL_ENU:
      ops += 5;
      break;
    case DDS_OP_VAL_SEQ: case DDS_OP_VAL_ARR: case DDS_OP_VAL_UNI: case DDS_OP_VAL_STU: {
      const uint32_t jmp = DDS_OP_ADR_JMP (ops[3]);
      const uint32_t *jsr_ops = ops + DDS_OP_ADR_JSR (ops[3]);
      if (ops + 5 > *ops_end)
        *ops_end = ops + 5;
      if (DDS_OP_ADR_JSR (ops[3]) > 0)
        dds_stream_countops1 (jsr_ops, ops_end, dynamic_type);
      ops += (jmp ? jmp : 5);
      break;
    }
    case DDS_OP_VAL_EXT:
      abort (); // not allowed
      break;
  }
  if (ops > *ops_end)
    *ops_end = ops;
  return ops;
}

static const uint32_t *dds_stream_countops_uni (const uint32_t * __restrict ops, const uint32_t **ops_end, bool *dynamic_type)
{
  const uint32_t numcases = ops[2];
  const uint32_t *jeq_op = ops + DDS_OP_ADR_JSR (ops[3]);
  for (uint32_t i = 0; i < numcases; i++)
  {
    const enum dds_stream_typecode valtype = DDS_JEQ_TYPE (jeq_op[0]);
    switch (valtype)
    {
      case DDS_OP_VAL_1BY:
      case DDS_OP_VAL_2BY:
      case DDS_OP_VAL_4BY:
      case DDS_OP_VAL_8BY:
      case DDS_OP_VAL_STR:
      case DDS_OP_VAL_ENU:
        break;
      case DDS_OP_VAL_BST: case DDS_OP_VAL_BSP: case DDS_OP_VAL_SEQ: case DDS_OP_VAL_ARR: case DDS_OP_VAL_UNI: case DDS_OP_VAL_STU:
        dds_stream_countops1 (jeq_op + DDS_OP_ADR_JSR (jeq_op[0]), ops_end, dynamic_type);
        break;
      case DDS_OP_VAL_EXT:
        abort (); // not allowed
        break;
    }
    jeq_op += 3 + (valtype == DDS_OP_VAL_ENU);
  }
  if (jeq_op > *ops_end)
    *ops_end = jeq_op;
  ops += DDS_OP_ADR_JMP (ops[3]);
  if (ops > *ops_end)
    *ops_end = ops;
  return ops;
}

static const uint32_t *dds_stream_countops_pl (const uint32_t * __restrict ops, const uint32_t **ops_end, bool *dynamic_type)
{
  uint32_t insn;
  ops++; /* skip PLC op */
  while ((insn = *ops) != DDS_OP_RTS)
  {
    switch (DDS_OP (insn))
    {
      case DDS_OP_PLM:
        dds_stream_countops1 (ops + DDS_OP_ADR_PLM (insn), ops_end, dynamic_type);
        ops += 2;
        break;
      default:
        abort (); /* only list of (PLM, member-id) supported */
        break;
    }
  }
  if (ops > *ops_end)
    *ops_end = ops;
  return ops;
}

static void dds_stream_countops1 (const uint32_t * __restrict ops, const uint32_t **ops_end, bool *dynamic_type)
{
  uint32_t insn;
  while ((insn = *ops) != DDS_OP_RTS)
  {
    switch (DDS_OP (insn))
    {
      case DDS_OP_ADR: {
        switch (DDS_OP_TYPE (insn))
        {
          case DDS_OP_VAL_1BY: case DDS_OP_VAL_2BY: case DDS_OP_VAL_4BY: case DDS_OP_VAL_8BY: case DDS_OP_VAL_STR:
            ops += 2;
            break;
          case DDS_OP_VAL_BST: case DDS_OP_VAL_BSP: case DDS_OP_VAL_ENU:
            ops += 3;
            break;
          case DDS_OP_VAL_SEQ: ops = dds_stream_countops_seq (ops, insn, ops_end, dynamic_type); break;
          case DDS_OP_VAL_ARR: ops = dds_stream_countops_arr (ops, insn, ops_end, dynamic_type); break;
          case DDS_OP_VAL_UNI: ops = dds_stream_countops_uni (ops, ops_end, dynamic_type); break;
          case DDS_OP_VAL_EXT: {
            const uint32_t *jsr_ops = ops + DDS_OP_ADR_JSR (ops[2]);
            const uint32_t jmp = DDS_OP_ADR_JMP (ops[2]);
            if (DDS_OP_ADR_JSR (ops[2]) > 0)
              dds_stream_countops1 (jsr_ops, ops_end, dynamic_type);
            ops += jmp ? jmp : 3;
            break;
          }
          case DDS_OP_VAL_STU:
            abort (); /* op type STU only supported as subtype */
            break;
        }
        break;
      }
      case DDS_OP_JSR: {
        if (DDS_OP_JUMP (insn) > 0)
          dds_stream_countops1 (ops + DDS_OP_JUMP (insn), ops_end, dynamic_type);
        ops++;
        break;
      }
      case DDS_OP_RTS: case DDS_OP_JEQ: case DDS_OP_KOF: case DDS_OP_PLM: {
        abort ();
        break;
      }
      case DDS_OP_DLC: {
        if (dynamic_type)
          *dynamic_type = true;
        ops++;
        break;
      }
      case DDS_OP_PLC: {
        if (dynamic_type)
          *dynamic_type = true;
        ops = dds_stream_countops_pl (ops, ops_end, dynamic_type);
        break;
      }
    }
  }
  ++ops; /* skip RTS op */
  if (ops > *ops_end)
    *ops_end = ops;
}

static void dds_stream_countops_keyoffset (const uint32_t * __restrict ops, const dds_key_descriptor_t * __restrict key, const uint32_t ** __restrict ops_end)
{
  assert (key);
  assert (*ops_end);
  if (key->m_index >= *ops_end - ops)
  {
    assert (DDS_OP (ops[key->m_index]) == DDS_OP_KOF);
    *ops_end = ops + key->m_index + 1 + DDS_OP_LENGTH (ops[key->m_index]);
  }
}

uint32_t dds_stream_countops (const uint32_t * __restrict ops, uint32_t nkeys, const dds_key_descriptor_t * __restrict keys)
{
  const uint32_t *ops_end = ops;
  dds_stream_countops1 (ops, &ops_end, NULL);
  for (uint32_t n = 0; n < nkeys; n++)
    dds_stream_countops_keyoffset (ops, &keys[n], &ops_end);
  return (uint32_t) (ops_end - ops);
}

static char *dds_stream_reuse_string_bound (dds_istream_t * __restrict is, char * __restrict str, const uint32_t size, bool alloc)
{
  const uint32_t length = dds_is_get4 (is);
  const void *src = is->m_buffer + is->m_index;
  /* FIXME: validation now rejects data containing an oversize bounded string,
     so this check is superfluous, but perhaps rejecting such a sample is the
     wrong thing to do */
  if (!alloc)
    assert (str != NULL);
  else if (str == NULL)
    str = dds_alloc (size);
  memcpy (str, src, length > size ? size : length);
  if (length > size)
    str[size - 1] = '\0';
  is->m_index += length;
  return str;
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

static char *dds_stream_reuse_string_empty (char * __restrict str)
{
  if (str == NULL)
    str = dds_realloc (str, 1);
  str[0] = '\0';
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

#ifndef NDEBUG
static bool insn_key_ok_p (uint32_t insn)
{
  return (DDS_OP (insn) == DDS_OP_ADR && (insn & DDS_OP_FLAG_KEY) &&
          (!type_has_subtype_or_members (DDS_OP_TYPE (insn)) // don't allow seq, uni, arr (unless exception below), struct (unless exception below)
            || (DDS_OP_TYPE (insn) == DDS_OP_VAL_ARR && DDS_OP_SUBTYPE (insn) <= DDS_OP_VAL_8BY) // allow prim-array as key
            || (DDS_OP_TYPE (insn) == DDS_OP_VAL_EXT) // allow fields in nested structs as key
          ));
}
#endif

static uint32_t read_union_discriminant (dds_istream_t * __restrict is, enum dds_stream_typecode type)
{
  assert (type == DDS_OP_VAL_1BY || type == DDS_OP_VAL_2BY || type == DDS_OP_VAL_4BY || type == DDS_OP_VAL_ENU);
  switch (type)
  {
    case DDS_OP_VAL_1BY: return dds_is_get1 (is);
    case DDS_OP_VAL_2BY: return dds_is_get2 (is);
    case DDS_OP_VAL_4BY: case DDS_OP_VAL_ENU: return dds_is_get4 (is);
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
  size_t idx = 0;
  for (ci = 0; ci < numcases; ci++)
  {
    assert (DDS_OP (jeq_op[idx]) == DDS_OP_JEQ);
    idx += (size_t) 3 + (DDS_OP_TYPE (jeq_op[idx]) == DDS_OP_VAL_ENU);
  }
#endif
  for (ci = 0; ci < numcases - (has_default ? 1 : 0); ci++)
  {
    if (jeq_op[1] == disc)
      return jeq_op;
    jeq_op += 3 + (DDS_OP_TYPE (jeq_op[0]) == DDS_OP_VAL_ENU);
  }
  return (ci < numcases) ? jeq_op : NULL;
}

static const uint32_t *skip_sequence_insns (uint32_t insn, const uint32_t * __restrict ops)
{
  assert (DDS_OP_TYPE (insn) == DDS_OP_VAL_SEQ);
  switch (DDS_OP_SUBTYPE (insn))
  {
    case DDS_OP_VAL_1BY: case DDS_OP_VAL_2BY: case DDS_OP_VAL_4BY: case DDS_OP_VAL_8BY:
    case DDS_OP_VAL_STR:
      return ops + 2;
    case DDS_OP_VAL_BST: case DDS_OP_VAL_BSP: case DDS_OP_VAL_ENU:
      return ops + 3;
    case DDS_OP_VAL_SEQ: case DDS_OP_VAL_ARR: case DDS_OP_VAL_UNI: case DDS_OP_VAL_STU: {
      const uint32_t jmp = DDS_OP_ADR_JMP (ops[3]);
      return ops + (jmp ? jmp : 4); /* FIXME: why would jmp be 0? */
    }
    case DDS_OP_VAL_EXT: {
      abort (); /* not allowed */
      break;
    }
  }
  return NULL;
}

static const uint32_t *skip_array_insns (uint32_t insn, const uint32_t * __restrict ops)
{
  assert (DDS_OP_TYPE (insn) == DDS_OP_VAL_ARR);
  switch (DDS_OP_SUBTYPE (insn))
  {
    case DDS_OP_VAL_1BY: case DDS_OP_VAL_2BY: case DDS_OP_VAL_4BY: case DDS_OP_VAL_8BY:
    case DDS_OP_VAL_STR:
      return ops + 3;
    case DDS_OP_VAL_ENU:
      return ops + 4;
    case DDS_OP_VAL_BST:
    case DDS_OP_VAL_BSP:
      return ops + 5;
    case DDS_OP_VAL_SEQ: case DDS_OP_VAL_ARR: case DDS_OP_VAL_UNI: case DDS_OP_VAL_STU: {
      const uint32_t jmp = DDS_OP_ADR_JMP (ops[3]);
      return ops + (jmp ? jmp : 5);
    }
    case DDS_OP_VAL_EXT: {
      abort (); /* not supported */
      break;
    }
  }
  return NULL;
}

static const uint32_t *skip_array_default (uint32_t insn, char * __restrict data, const uint32_t * __restrict ops)
{
  const enum dds_stream_typecode subtype = DDS_OP_SUBTYPE (insn);
  const uint32_t num = ops[2];
  switch (subtype)
  {
    case DDS_OP_VAL_ENU:
      ops++;
      /* fall through */
    case DDS_OP_VAL_1BY: case DDS_OP_VAL_2BY: case DDS_OP_VAL_4BY: case DDS_OP_VAL_8BY: {
      const uint32_t elem_size = get_type_size (subtype);
      memset (data, 0, num * elem_size);
      return ops + 3;
    }
    case DDS_OP_VAL_STR: {
      char **ptr = (char **) data;
      for (uint32_t i = 0; i < num; i++)
        ptr[i] = dds_stream_reuse_string_empty (*(char **) ptr[i]);
      return ops + 3;
    }
    case DDS_OP_VAL_BST: {
      char *ptr = (char *) data;
      const uint32_t elem_size = ops[4];
      for (uint32_t i = 0; i < num; i++)
        ((char *) (ptr + i * elem_size))[0] = '\0';
      return ops + 5;
    }
    case DDS_OP_VAL_BSP: {
      char **ptr = (char **) data;
      for (uint32_t i = 0; i < num; i++)
        ptr[i] = dds_stream_reuse_string_empty (*(char **) ptr[i]);
      return ops + 5;
    }
    case DDS_OP_VAL_SEQ: case DDS_OP_VAL_ARR: case DDS_OP_VAL_UNI: case DDS_OP_VAL_STU: {
      const uint32_t *jsr_ops = ops + DDS_OP_ADR_JSR (ops[3]);
      const uint32_t jmp = DDS_OP_ADR_JMP (ops[3]);
      const uint32_t elem_size = ops[4];
      for (uint32_t i = 0; i < num; i++)
        (void) dds_stream_skip_default (data + i * elem_size, jsr_ops);
      return ops + (jmp ? jmp : 5);
    }
    case DDS_OP_VAL_EXT: {
      abort (); /* not supported */
      break;
    }
  }
  return NULL;
}

static const uint32_t *skip_union_default (uint32_t insn, char * __restrict discaddr, char * __restrict baseaddr, const uint32_t * __restrict ops)
{
  switch (DDS_OP_SUBTYPE (insn))
  {
    case DDS_OP_VAL_1BY: *((uint8_t *) discaddr) = 0; break;
    case DDS_OP_VAL_2BY: *((uint16_t *) discaddr) = 0; break;
    case DDS_OP_VAL_4BY: case DDS_OP_VAL_ENU: *((uint32_t *) discaddr) = 0; break;
    default: break;
  }
  uint32_t const * const jeq_op = find_union_case (ops, 0);
  ops += DDS_OP_ADR_JMP (ops[3]);
  if (jeq_op)
  {
    const enum dds_stream_typecode valtype = DDS_JEQ_TYPE (jeq_op[0]);
    void *valaddr = baseaddr + jeq_op[2];
    switch (valtype)
    {
      case DDS_OP_VAL_1BY: *((uint8_t *) valaddr) = 0; break;
      case DDS_OP_VAL_2BY: *((uint16_t *) valaddr) = 0; break;
      case DDS_OP_VAL_4BY: case DDS_OP_VAL_ENU: *((uint32_t *) valaddr) = 0; break;
      case DDS_OP_VAL_8BY: *((uint64_t *) valaddr) = 0; break;
      case DDS_OP_VAL_STR: *(char **) valaddr = dds_stream_reuse_string_empty (*((char **) valaddr)); break;
      case DDS_OP_VAL_BST: case DDS_OP_VAL_BSP: case DDS_OP_VAL_SEQ: case DDS_OP_VAL_ARR: case DDS_OP_VAL_UNI: case DDS_OP_VAL_STU:
        (void) dds_stream_skip_default (valaddr, jeq_op + DDS_OP_ADR_JSR (jeq_op[0]));
        break;
      case DDS_OP_VAL_EXT: {
        abort (); /* not supported */
        break;
      }
    }
  }
  return ops;
}

static const uint32_t *dds_stream_write_delimitedLE (dds_ostreamLE_t * __restrict os, const char * __restrict data, const uint32_t * __restrict ops)
{
  uint32_t offs = dds_os_reserve4LE (os);
  ops = dds_stream_writeLE (os, data, ops + 1);
  *((uint32_t *) (os->x.m_buffer + offs - 4)) = ddsrt_toLE4u (os->x.m_index - offs);
  return ops;
}

static const uint32_t *dds_stream_write_delimitedBE (dds_ostreamBE_t * __restrict os, const char * __restrict data, const uint32_t * __restrict ops)
{
  uint32_t offs = dds_os_reserve4BE (os);
  ops = dds_stream_writeBE (os, data, ops + 1);
  *((uint32_t *) (os->x.m_buffer + offs - 4)) = ddsrt_toBE4u (os->x.m_index - offs);
  return ops;
}

static uint32_t get_length_code_seq (const uint32_t subtype)
{
  switch (subtype)
  {
    /* Sequence length can be used as byte length */
    case DDS_OP_VAL_1BY:
      return LENGTH_CODE_ALSO_NEXTINT;

    /* A sequence with primitive subtype does not include a DHEADER,
       only the seq length, so we have to include a NEXTINT */
    case DDS_OP_VAL_2BY:
      return LENGTH_CODE_NEXTINT;

    /* Sequence length (item count) is used to calculate byte length */
    case DDS_OP_VAL_4BY: case DDS_OP_VAL_ENU:
      return LENGTH_CODE_ALSO_NEXTINT4;
    case DDS_OP_VAL_8BY:
      return LENGTH_CODE_ALSO_NEXTINT8;

    /* Sequences with non-primitive subtype contain a DHEADER */
    case DDS_OP_VAL_STR: case DDS_OP_VAL_BSP: case DDS_OP_VAL_BST:
    case DDS_OP_VAL_SEQ: case DDS_OP_VAL_ARR: case DDS_OP_VAL_UNI: case DDS_OP_VAL_STU:
      return LENGTH_CODE_ALSO_NEXTINT;

    /* not supported */
    case DDS_OP_VAL_EXT:
      abort ();
      break;
  }
  abort ();
}

static uint32_t get_length_code_arr (const uint32_t subtype)
{
  switch (subtype)
  {
    /* An array with primitive subtype does not include a DHEADER,
       so we have to include a NEXTINT */
    case DDS_OP_VAL_1BY: case DDS_OP_VAL_2BY: case DDS_OP_VAL_4BY: case DDS_OP_VAL_ENU: case DDS_OP_VAL_8BY:
      return LENGTH_CODE_NEXTINT;

    /* Arrays with non-primitive subtype contain a DHEADER */
    case DDS_OP_VAL_STR: case DDS_OP_VAL_BSP: case DDS_OP_VAL_BST:
    case DDS_OP_VAL_SEQ: case DDS_OP_VAL_ARR: case DDS_OP_VAL_UNI: case DDS_OP_VAL_STU:
      return LENGTH_CODE_ALSO_NEXTINT;

    /* not supported */
    case DDS_OP_VAL_EXT:
      abort ();
      break;
  }
  abort ();
}

static uint32_t get_length_code (const uint32_t * __restrict ops)
{
  const uint32_t insn = *ops;
  assert (insn != DDS_OP_RTS);
  switch (DDS_OP (insn))
  {
    case DDS_OP_ADR: {
      switch (DDS_OP_TYPE (insn))
      {
        case DDS_OP_VAL_1BY: return LENGTH_CODE_1B;
        case DDS_OP_VAL_2BY: return LENGTH_CODE_2B;
        case DDS_OP_VAL_4BY: case DDS_OP_VAL_ENU: return LENGTH_CODE_4B;
        case DDS_OP_VAL_8BY: return LENGTH_CODE_8B;
        case DDS_OP_VAL_STR: case DDS_OP_VAL_BSP: case DDS_OP_VAL_BST: return LENGTH_CODE_ALSO_NEXTINT; /* nextint overlaps with length from serialized string data */
        case DDS_OP_VAL_SEQ: return get_length_code_seq (DDS_OP_SUBTYPE (insn));
        case DDS_OP_VAL_ARR: return get_length_code_arr (DDS_OP_SUBTYPE (insn));
        case DDS_OP_VAL_UNI: case DDS_OP_VAL_EXT: {
          return LENGTH_CODE_NEXTINT; /* FIXME: may be optimized for specific cases, e.g. when EXT type is appendable */
        }
        case DDS_OP_VAL_STU: abort (); break; /* op type STU only supported as subtype */
      }
      break;
    }
    case DDS_OP_JSR:
      return get_length_code (ops + DDS_OP_JUMP (insn));
    case DDS_OP_RTS: case DDS_OP_JEQ: case DDS_OP_KOF: case DDS_OP_PLM: {
      abort ();
      break;
    }
    case DDS_OP_DLC: case DDS_OP_PLC: {
      return LENGTH_CODE_ALSO_NEXTINT; /* nextint overlaps with DHEADER of serialized delimited or mutable type */
    }
  }
  return 0;
}

static void dds_stream_write_pl_memberLE (bool must_understand, uint32_t mid, dds_ostreamLE_t * __restrict os, const char * __restrict data, const uint32_t * __restrict ops)
{
  assert (!(mid & ~EMHEADER_MEMBERID_MASK));
  uint32_t lc = get_length_code (ops);
  assert (lc < LENGTH_CODE_ALSO_NEXTINT8);
  uint32_t data_offs = (lc != LENGTH_CODE_NEXTINT) ? dds_os_reserve4LE (os) : dds_os_reserve8LE (os);
  (void) dds_stream_writeLE (os, data, ops);

  uint32_t em_hdr = 0;
  if (must_understand)
    em_hdr |= EMHEADER_FLAG_MUSTUNDERSTAND;
  em_hdr |= lc << 28;
  em_hdr |= mid & EMHEADER_MEMBERID_MASK;

  uint32_t *em_hdr_ptr = (uint32_t *) (os->x.m_buffer + data_offs - (lc == LENGTH_CODE_NEXTINT ? 8 : 4));
  em_hdr_ptr[0] = ddsrt_toLE4u (em_hdr);
  if (lc == LENGTH_CODE_NEXTINT)
    em_hdr_ptr[1] = ddsrt_toLE4u (os->x.m_index - data_offs);  /* member size in next_int field in emheader */
}

static void dds_stream_write_pl_memberBE (bool must_understand, uint32_t mid, dds_ostreamBE_t * __restrict os, const char * __restrict data, const uint32_t * __restrict ops)
{
  assert (!(mid & ~EMHEADER_MEMBERID_MASK));
  uint32_t lc = get_length_code (ops);
  assert (lc < LENGTH_CODE_ALSO_NEXTINT8);
  uint32_t data_offs = (lc != LENGTH_CODE_NEXTINT) ? dds_os_reserve4BE (os) : dds_os_reserve8BE (os);
  (void) dds_stream_writeBE (os, data, ops);

  uint32_t em_hdr = 0;
  if (must_understand)
    em_hdr |= EMHEADER_FLAG_MUSTUNDERSTAND;
  em_hdr |= lc << 28;
  em_hdr |= mid & EMHEADER_MEMBERID_MASK;

  uint32_t *em_hdr_ptr = (uint32_t *) (os->x.m_buffer + data_offs - (lc == LENGTH_CODE_NEXTINT ? 8 : 4));
  em_hdr_ptr[0] = ddsrt_toBE4u (em_hdr);
  if (lc == LENGTH_CODE_NEXTINT)
    em_hdr_ptr[1] = ddsrt_toBE4u (os->x.m_index - data_offs);  /* member size in next_int field in emheader */
}

static const uint32_t *dds_stream_write_plLE (dds_ostreamLE_t * __restrict os, const char * __restrict data, const uint32_t * __restrict ops)
{
  /* skip PLC op */
  ops++;

  /* alloc space for dheader */
  dds_os_reserve4LE (os);
  uint32_t data_offs = os->x.m_index;

  uint32_t insn;
  while ((insn = *ops) != DDS_OP_RTS)
  {
    switch (DDS_OP (insn))
    {
      case DDS_OP_PLM: {
        uint32_t flags = DDS_PLM_FLAGS (insn);
        uint32_t member_id = ops[1];
        const uint32_t *plm_ops = ops + DDS_OP_ADR_PLM (insn);
        dds_stream_write_pl_memberLE (flags & DDS_OP_FLAG_MU, member_id, os, data, plm_ops);
        ops += 2;
        break;
      }
      default:
        abort (); /* other ops not supported at this point */
        break;
    }
  }
  /* write serialized size in dheader */
  *((uint32_t *) (os->x.m_buffer + data_offs - 4)) = ddsrt_toLE4u (os->x.m_index - data_offs);
  return ops;
}

static const uint32_t *dds_stream_write_plBE (dds_ostreamBE_t * __restrict os, const char * __restrict data, const uint32_t * __restrict ops)
{
  /* skip PLC op */
  ops++;

  /* alloc space for dheader */
  dds_os_reserve4BE (os);
  uint32_t data_offs = os->x.m_index;

  uint32_t insn;
  while ((insn = *ops) != DDS_OP_RTS)
  {
    switch (DDS_OP (insn))
    {
      case DDS_OP_PLM: {
        uint32_t flags = DDS_PLM_FLAGS (insn);
        uint32_t member_id = ops[1];
        const uint32_t *plm_ops = ops + DDS_OP_ADR_PLM (insn);
        dds_stream_write_pl_memberBE (flags & DDS_OP_FLAG_MU, member_id, os, data, plm_ops);
        ops += 2;
        break;
      }
      default:
        abort (); /* other ops not supported at this point */
        break;
    }
  }
  /* write serialized size in dheader */
  *((uint32_t *) (os->x.m_buffer + data_offs - 4)) = ddsrt_toBE4u (os->x.m_index - data_offs);
  return ops;
}

#if DDSRT_ENDIAN == DDSRT_LITTLE_ENDIAN
static inline void dds_stream_to_BE_insitu (void * __restrict vbuf, uint32_t size, uint32_t num)
{
  dds_stream_swap (vbuf, size, num);
}
static inline void dds_stream_to_LE_insitu (void * __restrict vbuf, uint32_t size, uint32_t num)
{
  (void) vbuf;
  (void) size;
  (void) num;
}
#else /* if DDSRT_ENDIAN == DDSRT_LITTLE_ENDIAN */
static inline void dds_stream_to_BE_insitu (void * __restrict vbuf, uint32_t size, uint32_t num)
{
  (void) vbuf;
  (void) size;
  (void) num;
}
static inline void dds_stream_to_LE_insitu (void * __restrict vbuf, uint32_t size, uint32_t num)
{
  dds_stream_swap (vbuf, size, num);
}
#endif /* if DDSRT_ENDIAN == DDSRT_LITTLE_ENDIAN */

// Little-endian
#define NAME_BYTE_ORDER_EXT LE
#include "ddsi_cdrstream_write.part.c"
#undef NAME_BYTE_ORDER_EXT

// Big-endian
#define NAME_BYTE_ORDER_EXT BE
#include "ddsi_cdrstream_write.part.c"
#undef NAME_BYTE_ORDER_EXT

// Map some write-native functions to their little-endian or big-endian equivalent
#if DDSRT_ENDIAN == DDSRT_LITTLE_ENDIAN

static inline void dds_stream_write_string (dds_ostream_t * __restrict os, const char * __restrict val)
{
  dds_stream_write_stringLE ((dds_ostreamLE_t *) os, val);
}

inline const uint32_t *dds_stream_write (dds_ostream_t * __restrict os, const char * __restrict data, const uint32_t * __restrict ops)
{
  return dds_stream_writeLE ((dds_ostreamLE_t *) os, data, ops);
}

inline void dds_stream_write_sample (dds_ostream_t * __restrict os, const void * __restrict data, const struct ddsi_sertype_default * __restrict type)
{
  dds_stream_write_sampleLE ((dds_ostreamLE_t *) os, data, type);
}

void dds_stream_write_sampleLE (dds_ostreamLE_t * __restrict os, const void * __restrict data, const struct ddsi_sertype_default * __restrict type)
{
  const struct ddsi_sertype_default_desc *desc = &type->type;
  if (type->opt_size && desc->align && (((struct dds_ostream *)os)->m_index % desc->align) == 0)
    dds_os_put_bytes ((struct dds_ostream *)os, data, (uint32_t) type->opt_size);
  else
    (void) dds_stream_writeLE (os, data, desc->ops.ops);
}

void dds_stream_write_sampleBE (dds_ostreamBE_t * __restrict os, const void * __restrict data, const struct ddsi_sertype_default * __restrict type)
{
  const struct ddsi_sertype_default_desc *desc = &type->type;
  (void) dds_stream_writeBE (os, data, desc->ops.ops);
}

#else /* if DDSRT_ENDIAN == DDSRT_LITTLE_ENDIAN */

static inline void dds_stream_write_string (dds_ostream_t * __restrict os, const char * __restrict val)
{
  dds_stream_write_stringBE ((dds_ostreamBE_t *) os, val);
}

inline const uint32_t *dds_stream_write (dds_ostream_t * __restrict os, const char * __restrict data, const uint32_t * __restrict ops)
{
  return dds_stream_writeBE ((dds_ostreamBE_t *) os, data, ops);
}

inline void dds_stream_write_sample (dds_ostream_t * __restrict os, const void * __restrict data, const struct ddsi_sertype_default * __restrict type)
{
  dds_stream_write_sampleBE ((dds_ostreamBE_t *) os, data, type);
}

void dds_stream_write_sampleLE (dds_ostreamLE_t * __restrict os, const void * __restrict data, const struct ddsi_sertype_default * __restrict type)
{
  const struct ddsi_sertype_default_desc *desc = &type->type;
  (void) dds_stream_writeLE (os, data, desc->ops.ops);
}

void dds_stream_write_sampleBE (dds_ostreamBE_t * __restrict os, const void * __restrict data, const struct ddsi_sertype_default * __restrict type)
{
  const struct ddsi_sertype_default_desc *desc = &type->type;
  if (type->opt_size && desc->align && (((struct dds_ostream *)os)->m_index % desc->align) == 0)
    dds_os_put_bytes ((struct dds_ostream *)os, data, (uint32_t) type->opt_size);
  else
    (void) dds_stream_writeBE (os, data, desc->ops.ops);
}

#endif /* if DDSRT_ENDIAN == DDSRT_LITTLE_ENDIAN */

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
  if (subtype > DDS_OP_VAL_8BY && is->m_xcdr_version == CDR_ENC_VERSION_2)
  {
    /* skip DHEADER */
    dds_is_get4 (is);
  }

  const uint32_t num = dds_is_get4 (is);
  if (num == 0)
  {
    seq->_length = 0;
    return skip_sequence_insns (insn, ops);
  }

  switch (subtype)
  {
    case DDS_OP_VAL_1BY: case DDS_OP_VAL_2BY: case DDS_OP_VAL_4BY: case DDS_OP_VAL_8BY: case DDS_OP_VAL_ENU: {
      const uint32_t elem_size = get_type_size (subtype);
      realloc_sequence_buffer_if_needed (seq, num, elem_size, false);
      seq->_length = (num <= seq->_maximum) ? num : seq->_maximum;
      dds_is_get_bytes (is, seq->_buffer, seq->_length, elem_size);
      if (seq->_length < num)
        dds_stream_skip_forward (is, num - seq->_length, elem_size);
      return ops + (subtype == DDS_OP_VAL_ENU ? 3 : 2);
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
        (void) dds_stream_reuse_string_bound (is, ptr + i * elem_size, elem_size, false);
      for (uint32_t i = seq->_length; i < num; i++)
        dds_stream_skip_string (is);
      return ops + 3;
    }
    case DDS_OP_VAL_BSP: {
      const uint32_t elem_size = ops[2];
      realloc_sequence_buffer_if_needed (seq, num, elem_size, false);
      seq->_length = (num <= seq->_maximum) ? num : seq->_maximum;
      char **ptr = (char **) seq->_buffer;
      for (uint32_t i = 0; i < seq->_length; i++)
        ptr[i] = dds_stream_reuse_string_bound (is, ptr[i], elem_size, true);
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
        (void) dds_stream_read (is, ptr + i * elem_size, jsr_ops);
      return ops + (jmp ? jmp : 4); /* FIXME: why would jmp be 0? */
    }
    case DDS_OP_VAL_EXT: {
      abort (); /* not supported */
      break;
    }
  }
  return NULL;
}

static const uint32_t *dds_stream_read_arr (dds_istream_t * __restrict is, char * __restrict addr, const uint32_t * __restrict ops, uint32_t insn)
{
  const enum dds_stream_typecode subtype = DDS_OP_SUBTYPE (insn);
  if (subtype > DDS_OP_VAL_8BY && is->m_xcdr_version == CDR_ENC_VERSION_2)
  {
    /* skip DHEADER */
    dds_is_get4 (is);
  }
  const uint32_t num = ops[2];
  switch (subtype)
  {
    case DDS_OP_VAL_ENU:
      ops++;
      /* fall through */
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
        (void) dds_stream_reuse_string_bound (is, ptr + i * elem_size, elem_size, false);
      return ops + 5;
    }
    case DDS_OP_VAL_BSP: {
      char **ptr = (char **) addr;
      const uint32_t elem_size = ops[4];
      for (uint32_t i = 0; i < num; i++)
        ptr[i] = dds_stream_reuse_string_bound (is, ptr[i], elem_size, true);
      return ops + 5;
    }
    case DDS_OP_VAL_SEQ: case DDS_OP_VAL_ARR: case DDS_OP_VAL_UNI: case DDS_OP_VAL_STU: {
      const uint32_t *jsr_ops = ops + DDS_OP_ADR_JSR (ops[3]);
      const uint32_t jmp = DDS_OP_ADR_JMP (ops[3]);
      const uint32_t elem_size = ops[4];
      for (uint32_t i = 0; i < num; i++)
        (void) dds_stream_read (is, addr + i * elem_size, jsr_ops);
      return ops + (jmp ? jmp : 5);
    }
    case DDS_OP_VAL_EXT: {
      abort (); /* not supported */
      break;
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
    case DDS_OP_VAL_4BY: case DDS_OP_VAL_ENU: *((uint32_t *) discaddr) = disc; break;
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
      case DDS_OP_VAL_4BY: case DDS_OP_VAL_ENU: *((uint32_t *) valaddr) = dds_is_get4 (is); break;
      case DDS_OP_VAL_8BY: *((uint64_t *) valaddr) = dds_is_get8 (is); break;
      case DDS_OP_VAL_STR: *(char **) valaddr = dds_stream_reuse_string (is, *((char **) valaddr)); break;
      case DDS_OP_VAL_BST: case DDS_OP_VAL_BSP: case DDS_OP_VAL_SEQ: case DDS_OP_VAL_ARR: case DDS_OP_VAL_UNI: case DDS_OP_VAL_STU:
        (void) dds_stream_read (is, valaddr, jeq_op + DDS_OP_ADR_JSR (jeq_op[0]));
        break;
      case DDS_OP_VAL_EXT: {
        abort (); /* not supported */
        break;
      }
    }
  }
  return ops;
}

static inline const uint32_t *dds_stream_read_adr (uint32_t insn, dds_istream_t * __restrict is, char * __restrict data, const uint32_t * __restrict ops)
{
  void *addr = data + ops[1];
  switch (DDS_OP_TYPE (insn))
  {
    case DDS_OP_VAL_1BY: *((uint8_t *) addr) = dds_is_get1 (is); ops += 2; break;
    case DDS_OP_VAL_2BY: *((uint16_t *) addr) = dds_is_get2 (is); ops += 2; break;
    case DDS_OP_VAL_4BY: *((uint32_t *) addr) = dds_is_get4 (is); ops += 2; break;
    case DDS_OP_VAL_8BY: *((uint64_t *) addr) = dds_is_get8 (is); ops += 2; break;
    case DDS_OP_VAL_STR: *((char **) addr) = dds_stream_reuse_string (is, *((char **) addr)); ops += 2; break;
    case DDS_OP_VAL_BST: (void) dds_stream_reuse_string_bound (is, (char *) addr, ops[2], false); ops += 3; break;
    case DDS_OP_VAL_BSP: *((char **) addr) = dds_stream_reuse_string_bound (is, *((char **) addr), ops[2], true); ops += 3; break;
    case DDS_OP_VAL_SEQ: ops = dds_stream_read_seq (is, addr, ops, insn); break;
    case DDS_OP_VAL_ARR: ops = dds_stream_read_arr (is, addr, ops, insn); break;
    case DDS_OP_VAL_UNI: ops = dds_stream_read_uni (is, addr, data, ops, insn); break;
    case DDS_OP_VAL_ENU: *((uint32_t *) addr) = dds_is_get4 (is); ops += 3; break;
    case DDS_OP_VAL_EXT: {
      const uint32_t *jsr_ops = ops + DDS_OP_ADR_JSR (ops[2]);
      const uint32_t jmp = DDS_OP_ADR_JMP (ops[2]);
      uint32_t flags = DDS_OP_FLAGS (insn);
      if (flags & DDS_OP_FLAG_EXT)
      {
        uint32_t sz = ops[3];
        *((char **) addr) = ddsrt_malloc (sz);
        (void) dds_stream_read (is, *((char **) addr), jsr_ops);
      }
      else
        (void) dds_stream_read (is, addr, jsr_ops);
      ops += jmp ? jmp : 3;
      break;
    }
    case DDS_OP_VAL_STU: abort(); break; /* op type STU only supported as subtype */
  }
  return ops;
}

static const uint32_t *dds_stream_skip_adr (uint32_t insn, const uint32_t * __restrict ops)
{
  switch (DDS_OP_TYPE (insn))
  {
    case DDS_OP_VAL_1BY:
    case DDS_OP_VAL_2BY:
    case DDS_OP_VAL_4BY:
    case DDS_OP_VAL_8BY:
    case DDS_OP_VAL_STR:
      return ops + 2;
    case DDS_OP_VAL_BST:
    case DDS_OP_VAL_BSP:
    case DDS_OP_VAL_ENU:
      return ops + 3;
    case DDS_OP_VAL_SEQ:
      return skip_sequence_insns (insn, ops);
    case DDS_OP_VAL_ARR:
      return skip_array_insns (insn, ops);
    case DDS_OP_VAL_UNI: {
      const uint32_t jmp = DDS_OP_ADR_JMP (ops[3]);
      return ops + (jmp ? jmp : 4); /* FIXME: jmp cannot be 0? */
    }
    case DDS_OP_VAL_EXT: {
      const uint32_t jmp = DDS_OP_ADR_JMP (ops[2]);
      return ops + (jmp ? jmp : 3);
    }
    case DDS_OP_VAL_STU: {
      abort(); /* op type STU only supported as subtype */
      break;
    }
  }
  return NULL;
}

static const uint32_t *dds_stream_skip_adr_default (uint32_t insn, char * __restrict data, const uint32_t * __restrict ops)
{
  void *addr = data + ops[1];
  /* FIXME: currently only implicit default values are used, this code should be
     using default values that are specified in the type definition */
  switch (DDS_OP_TYPE (insn))
  {
    case DDS_OP_VAL_1BY: *(uint8_t *) addr = 0; return ops + 2;
    case DDS_OP_VAL_2BY: *(uint16_t *) addr = 0; return ops + 2;
    case DDS_OP_VAL_4BY: *(uint32_t *) addr = 0; return ops + 2;
    case DDS_OP_VAL_8BY: *(uint64_t *) addr = 0; return ops + 2;

    case DDS_OP_VAL_STR: *(char **) addr = dds_stream_reuse_string_empty (*(char **) addr); return ops + 2;
    case DDS_OP_VAL_BST: ((char *) addr)[0] = '\0'; return ops + 3;
    case DDS_OP_VAL_BSP: *(char **) addr = dds_stream_reuse_string_empty (*(char **) addr); return ops + 3;
    case DDS_OP_VAL_ENU: *(uint32_t *) addr = 0; return ops + 3;
    case DDS_OP_VAL_SEQ: {
      dds_sequence_t * const seq = (dds_sequence_t *) addr;
      seq->_length = 0;
      return skip_sequence_insns (insn, ops);
    }
    case DDS_OP_VAL_ARR: {
      return skip_array_default (insn, addr, ops);
    }
    case DDS_OP_VAL_UNI: {
      return skip_union_default (insn, addr, data, ops);
    }
    case DDS_OP_VAL_EXT: {
      const uint32_t *jsr_ops = ops + DDS_OP_ADR_JSR (ops[2]);
      const uint32_t jmp = DDS_OP_ADR_JMP (ops[2]);
      (void) dds_stream_skip_default (addr, jsr_ops);
      return ops + (jmp ? jmp : 3);
    }
    case DDS_OP_VAL_STU: {
      abort(); /* op type STU only supported as subtype */
      break;
    }
  }
  return NULL;
}

static const uint32_t *dds_stream_skip_default (char * __restrict data, const uint32_t * __restrict ops)
{
  uint32_t insn;
  while ((insn = *ops) != DDS_OP_RTS)
  {
    switch (DDS_OP (insn))
    {
      case DDS_OP_ADR: {
        ops = dds_stream_skip_adr_default (insn, data, ops);
        break;
      }
      case DDS_OP_JSR: {
        (void) dds_stream_skip_default (data, ops + DDS_OP_JUMP (insn));
        ops++;
        break;
      }
      case DDS_OP_RTS: case DDS_OP_JEQ: case DDS_OP_KOF: case DDS_OP_DLC: case DDS_OP_PLC: case DDS_OP_PLM: {
        abort ();
        break;
      }
    }
  }
  return ops;
}

static const uint32_t *dds_stream_read_delimited (dds_istream_t * __restrict is, char * __restrict data, const uint32_t * __restrict ops)
{
  uint32_t delimited_sz = dds_is_get4 (is), delimited_offs = is->m_index, insn;
  ops++;
  while ((insn = *ops) != DDS_OP_RTS)
  {
    switch (DDS_OP (insn))
    {
      case DDS_OP_ADR: {
        /* skip fields that are not in serialized data for appendable type */
        ops = (is->m_index - delimited_offs < delimited_sz) ? dds_stream_read_adr (insn, is, data, ops) : dds_stream_skip_adr_default (insn, data, ops);
        break;
      }
      case DDS_OP_JSR: {
        (void) dds_stream_read (is, data, ops + DDS_OP_JUMP (insn));
        ops++;
        break;
      }
      case DDS_OP_RTS: case DDS_OP_JEQ: case DDS_OP_KOF: case DDS_OP_DLC: case DDS_OP_PLC: case DDS_OP_PLM: {
        abort ();
        break;
      }
    }
  }
  /* Skip remainder of serialized data for this appendable type */
  if (delimited_sz > is->m_index - delimited_offs)
    is->m_index += delimited_sz - (is->m_index - delimited_offs);
  return ops;
}

static const uint32_t *dds_stream_read_pl (dds_istream_t * __restrict is, char * __restrict data, const uint32_t * __restrict ops)
{
  /* skip PLC op */
  ops++;

  /* read DHEADER */
  uint32_t pl_sz = dds_is_get4 (is), pl_offs = is->m_index;
  while (is->m_index - pl_offs < pl_sz)
  {
    /* read EMHEADER and next_int */
    uint32_t em_hdr = dds_is_get4 (is);
    uint32_t lc = EMHEADER_LENGTH_CODE (em_hdr), mid = EMHEADER_MEMBERID (em_hdr), msz;
    switch (lc)
    {
      case LENGTH_CODE_1B: case LENGTH_CODE_2B: case LENGTH_CODE_4B: case LENGTH_CODE_8B:
        msz = 1u << lc;
        break;
      case LENGTH_CODE_NEXTINT:
        /* read NEXTINT */
        msz = dds_is_get4 (is);
        break;
      case LENGTH_CODE_ALSO_NEXTINT: case LENGTH_CODE_ALSO_NEXTINT4: case LENGTH_CODE_ALSO_NEXTINT8:
        /* length is part of serialized data */
        msz = dds_is_peek4 (is);
        if (lc > LENGTH_CODE_ALSO_NEXTINT)
          msz <<= (lc - 4);
        break;
      default:
        abort ();
        break;
    }

    /* find member and deserialize */
    uint32_t insn, ops_csr = 0;
    bool found = false;

    /* FIXME: continue finding the member in the ops member list starting from the last
       found one, because in many cases the members will be in the data sequentially */
    while (!found && (insn = ops[ops_csr]) != DDS_OP_RTS)
    {
      assert (DDS_OP (insn) == DDS_OP_PLM);
      if (ops[ops_csr + 1] == mid)
      {
        const uint32_t *plm_ops = ops + ops_csr + DDS_OP_ADR_PLM (insn);
        (void) dds_stream_read (is, data, plm_ops);
        found = true;
        break;
      }
      ops_csr += 2;
    }

    if (!found)
    {
      is->m_index += msz;
      if (lc >= LENGTH_CODE_ALSO_NEXTINT)
        is->m_index += 4; /* length embedded in member does not include it's own 4 bytes */
    }
  }

  /* skip all PLM-memberid pairs */
  while (ops[0] != DDS_OP_RTS)
    ops += 2;

  return ops;
}

const uint32_t *dds_stream_read (dds_istream_t * __restrict is, char * __restrict data, const uint32_t * __restrict ops)
{
  uint32_t insn;
  while ((insn = *ops) != DDS_OP_RTS)
  {
    switch (DDS_OP (insn))
    {
      case DDS_OP_ADR: {
        ops = dds_stream_read_adr (insn, is, data, ops);
        break;
      }
      case DDS_OP_JSR: {
        (void) dds_stream_read (is, data, ops + DDS_OP_JUMP (insn));
        ops++;
        break;
      }
      case DDS_OP_RTS: case DDS_OP_JEQ: case DDS_OP_KOF: case DDS_OP_PLM: {
        abort ();
        break;
      }
      case DDS_OP_DLC: {
        assert (is->m_xcdr_version == CDR_ENC_VERSION_2);
        ops = dds_stream_read_delimited (is, data, ops);
        break;
      }
      case DDS_OP_PLC: {
        assert (is->m_xcdr_version == CDR_ENC_VERSION_2);
        ops = dds_stream_read_pl (is, data, ops);
        break;
      }
    }
  }
  return ops;
}

/*******************************************************************************************
 **
 **  Validation and conversion to native endian.
 **
 *******************************************************************************************/

/* Limit the size of the input buffer so we don't need to worry about adding
   padding and a primitive type overflowing our offset */
#define CDR_SIZE_MAX ((uint32_t) 0xfffffff0)

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

static bool peek_and_normalize_uint32 (uint32_t * __restrict val, char * __restrict data, uint32_t * __restrict off, uint32_t size, bool bswap)
{
  if ((*off = check_align_prim (*off, size, 2)) == UINT32_MAX)
    return false;
  if (bswap)
    *val = ddsrt_bswap4u (*((uint32_t *) (data + *off)));
  else
    *val = *((uint32_t *) (data + *off));
  return true;
}

static bool normalize_enum (char * __restrict data, uint32_t * __restrict off, uint32_t size, bool bswap, uint32_t max)
{
  uint32_t val;
  if (!read_and_normalize_uint32 (&val, data, off, size, bswap))
    return false;
  return val <= max;
}

static bool normalize_uint64 (char * __restrict data, uint32_t * __restrict off, uint32_t size, bool bswap, uint32_t xcdr_version)
{
  if ((*off = check_align_prim (*off, size, xcdr_version == CDR_ENC_VERSION_2 ? 2 : 3)) == UINT32_MAX)
    return false;
  if (bswap)
  {
    uint32_t x = ddsrt_bswap4u (* (uint32_t *) data + *off);
    *((uint32_t *) data + *off) = ddsrt_bswap4u (* (((uint32_t *) data + *off) + 1));
    *(((uint32_t *) data + *off) + 1) = x;
  }
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

static bool normalize_primarray (char * __restrict data, uint32_t * __restrict off, uint32_t size, bool bswap, uint32_t num, enum dds_stream_typecode type, uint32_t xcdr_version)
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
    case DDS_OP_VAL_ENU:
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
      if ((*off = check_align_prim_many (*off, size, xcdr_version == CDR_ENC_VERSION_2 ? 2 : 3, num)) == UINT32_MAX)
        return false;
      if (bswap)
      {
        uint64_t *xs = (uint64_t *) (data + *off);
        for (uint32_t i = 0; i < num; i++)
        {
          uint32_t x = ddsrt_bswap4u (* (uint32_t *) &xs[i]);
          *(uint32_t *) &xs[i] = ddsrt_bswap4u (* (((uint32_t *) &xs[i]) + 1));
          *(((uint32_t *) &xs[i]) + 1) = x;
        }
      }
      *off += 8 * num;
      return true;
    default:
      abort ();
      break;
  }
  return false;
}

static const uint32_t *normalize_seq (char * __restrict data, uint32_t * __restrict off, uint32_t size, bool bswap, uint32_t xcdr_version, const uint32_t * __restrict ops, uint32_t insn)
{
  const enum dds_stream_typecode subtype = DDS_OP_SUBTYPE (insn);
  if (subtype > DDS_OP_VAL_8BY && xcdr_version == CDR_ENC_VERSION_2)
  {
    /* DHEADER */
    if (!normalize_uint32 (data, off, size, bswap))
      return NULL;
  }
  uint32_t num;
  if (!read_and_normalize_uint32 (&num, data, off, size, bswap))
    return NULL;
  if (num == 0)
    return skip_sequence_insns (insn, ops);
  switch (subtype)
  {
    case DDS_OP_VAL_ENU:
      ops++;
      /* fall through */
    case DDS_OP_VAL_1BY: case DDS_OP_VAL_2BY: case DDS_OP_VAL_4BY: case DDS_OP_VAL_8BY: {
      if (!normalize_primarray (data, off, size, bswap, num, subtype, xcdr_version))
        return NULL;
      return ops + 2;
    }
    case DDS_OP_VAL_STR: case DDS_OP_VAL_BST: case DDS_OP_VAL_BSP: {
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
        if (dds_stream_normalize1 (data, off, size, bswap, xcdr_version, jsr_ops) == NULL)
          return NULL;
      return ops + (jmp ? jmp : 4); /* FIXME: why would jmp be 0? */
    }
    case DDS_OP_VAL_EXT: {
      abort (); /* not supported */
      break;
    }
  }
  return NULL;
}

static const uint32_t *normalize_arr (char * __restrict data, uint32_t * __restrict off, uint32_t size, bool bswap, uint32_t xcdr_version, const uint32_t * __restrict ops, uint32_t insn)
{
  const enum dds_stream_typecode subtype = DDS_OP_SUBTYPE (insn);
  if (subtype > DDS_OP_VAL_8BY && xcdr_version == CDR_ENC_VERSION_2)
  {
    /* DHEADER */
    if (!normalize_uint32 (data, off, size, bswap))
      return NULL;
  }
  const uint32_t num = ops[2];
  switch (subtype)
  {
    case DDS_OP_VAL_ENU:
      ops++;
      /* fall through */
    case DDS_OP_VAL_1BY: case DDS_OP_VAL_2BY: case DDS_OP_VAL_4BY: case DDS_OP_VAL_8BY: {
      if (!normalize_primarray (data, off, size, bswap, num, subtype, xcdr_version))
        return NULL;
      return ops + 3;
    }
    case DDS_OP_VAL_STR: case DDS_OP_VAL_BST: case DDS_OP_VAL_BSP: {
      const size_t maxsz = (subtype == DDS_OP_VAL_STR) ? SIZE_MAX : ops[4];
      for (uint32_t i = 0; i < num; i++)
        if (!normalize_string (data, off, size, bswap, maxsz))
          return NULL;
      return ops + (subtype == DDS_OP_VAL_STR ? 3 : 5);
    }
    case DDS_OP_VAL_SEQ: case DDS_OP_VAL_ARR: case DDS_OP_VAL_UNI: case DDS_OP_VAL_STU: {
      const uint32_t jmp = DDS_OP_ADR_JMP (ops[3]);
      uint32_t const * const jsr_ops = ops + DDS_OP_ADR_JSR (ops[3]);
      for (uint32_t i = 0; i < num; i++)
        if (dds_stream_normalize1 (data, off, size, bswap, xcdr_version, jsr_ops) == NULL)
          return NULL;
      return ops + (jmp ? jmp : 5);
    }
    case DDS_OP_VAL_EXT: {
      abort (); /* not supported */
      break;
    }
  }
  return NULL;
}

static bool normalize_uni_disc (uint32_t * __restrict val, char * __restrict data, uint32_t * __restrict off, uint32_t size, bool bswap, enum dds_stream_typecode disctype, const uint32_t * __restrict ops)
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
    case DDS_OP_VAL_ENU:
      if (!read_and_normalize_uint32 (val, data, off, size, bswap))
        return false;
      return *val <= ops[4];
    default:
      abort ();
  }
  return false;
}

static const uint32_t *normalize_uni (char * __restrict data, uint32_t * __restrict off, uint32_t size, bool bswap, uint32_t xcdr_version, const uint32_t * __restrict ops, uint32_t insn)
{
  uint32_t disc;
  if (!normalize_uni_disc (&disc, data, off, size, bswap, DDS_OP_SUBTYPE (insn), ops))
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
      case DDS_OP_VAL_ENU: if (!normalize_enum (data, off, size, bswap, jeq_op[3])) return NULL; break;
      case DDS_OP_VAL_8BY: if (!normalize_uint64 (data, off, size, bswap, xcdr_version)) return NULL; break;
      case DDS_OP_VAL_STR: if (!normalize_string (data, off, size, bswap, SIZE_MAX)) return NULL; break;
      case DDS_OP_VAL_BST: case DDS_OP_VAL_BSP: case DDS_OP_VAL_SEQ: case DDS_OP_VAL_ARR: case DDS_OP_VAL_UNI: case DDS_OP_VAL_STU:
        if (dds_stream_normalize1 (data, off, size, bswap, xcdr_version, jeq_op + DDS_OP_ADR_JSR (jeq_op[0])) == NULL)
          return NULL;
        break;
      case DDS_OP_VAL_EXT: {
        abort (); /* not supported */
        break;
      }
    }
  }
  return ops;
}

static const uint32_t *stream_normalize_adr (uint32_t insn, char * __restrict data, uint32_t * __restrict off, uint32_t size, bool bswap, uint32_t xcdr_version, const uint32_t * __restrict ops)
{
  switch (DDS_OP_TYPE (insn))
  {
    case DDS_OP_VAL_1BY: if (!normalize_uint8 (off, size)) return NULL; ops += 2; break;
    case DDS_OP_VAL_2BY: if (!normalize_uint16 (data, off, size, bswap)) return NULL; ops += 2; break;
    case DDS_OP_VAL_4BY: if (!normalize_uint32 (data, off, size, bswap)) return NULL; ops += 2; break;
    case DDS_OP_VAL_8BY: if (!normalize_uint64 (data, off, size, bswap, xcdr_version)) return NULL; ops += 2; break;
    case DDS_OP_VAL_STR: if (!normalize_string (data, off, size, bswap, SIZE_MAX)) return NULL; ops += 2; break;
    case DDS_OP_VAL_BST: case DDS_OP_VAL_BSP: if (!normalize_string (data, off, size, bswap, ops[2])) return NULL; ops += 3; break;
    case DDS_OP_VAL_SEQ: ops = normalize_seq (data, off, size, bswap, xcdr_version, ops, insn); if (!ops) return NULL; break;
    case DDS_OP_VAL_ARR: ops = normalize_arr (data, off, size, bswap, xcdr_version, ops, insn); if (!ops) return NULL; break;
    case DDS_OP_VAL_UNI: ops = normalize_uni (data, off, size, bswap, xcdr_version, ops, insn); if (!ops) return NULL; break;
    case DDS_OP_VAL_ENU: if (!normalize_enum (data, off, size, bswap, ops[2])) return NULL; ops += 3; break;
    case DDS_OP_VAL_EXT: {
      const uint32_t *jsr_ops = ops + DDS_OP_ADR_JSR (ops[2]);
      const uint32_t jmp = DDS_OP_ADR_JMP (ops[2]);
      if (dds_stream_normalize1 (data, off, size, bswap, xcdr_version, jsr_ops) == NULL)
        return NULL;
      ops += jmp ? jmp : 3;
      break;
    }
    case DDS_OP_VAL_STU:
      abort (); /* op type STU only supported as subtype */
      break;
  }
  return ops;
}

static const uint32_t *stream_normalize_delimited (char * __restrict data, uint32_t * __restrict off, uint32_t size, bool bswap, uint32_t xcdr_version, const uint32_t * __restrict ops)
{
  uint32_t delimited_sz;
  if (!read_and_normalize_uint32 (&delimited_sz, data, off, size, bswap))
    return NULL;
  uint32_t delimited_offs = *off, insn;

  ops++; /* skip DLC op */
  while ((insn = *ops) != DDS_OP_RTS)
  {
    switch (DDS_OP (insn))
    {
      case DDS_OP_ADR: {
        if (*off - delimited_offs < delimited_sz)
        {
          if ((ops = stream_normalize_adr (insn, data, off, size, bswap, xcdr_version, ops)) == NULL)
            return NULL;
        }
        else
        {
          /* skip fields that are not in serialized data for appendable type */
          ops = dds_stream_skip_adr (insn, ops);
        }
        break;
      }
      case DDS_OP_JSR: {
        if (dds_stream_normalize1 (data, off, size, bswap, xcdr_version, ops + DDS_OP_JUMP (insn)) == NULL)
          return NULL;
        ops++;
        break;
      }
      case DDS_OP_RTS: case DDS_OP_JEQ: case DDS_OP_KOF: case DDS_OP_DLC: case DDS_OP_PLC: case DDS_OP_PLM: {
        abort ();
        break;
      }
    }
  }
  /* Skip remainder of serialized data for this appendable type */
  if (delimited_sz > *off - delimited_offs)
    *off += delimited_sz - (*off - delimited_offs);
  return ops;
}

static const uint32_t *stream_normalize_pl (char * __restrict data, uint32_t * __restrict off, uint32_t size, bool bswap, uint32_t xcdr_version, const uint32_t * __restrict ops)
{
  /* skip PLC op */
  ops++;

  /* normalize DHEADER */
  uint32_t pl_sz;
  if (!read_and_normalize_uint32 (&pl_sz, data, off, size, bswap))
    return NULL;
  uint32_t pl_offs = *off;
  while (*off - pl_offs < pl_sz)
  {
    /* normalize EMHEADER */
    uint32_t em_hdr;
    if (!read_and_normalize_uint32 (&em_hdr, data, off, size, bswap))
      return NULL;
    uint32_t lc = EMHEADER_LENGTH_CODE (em_hdr), mid = EMHEADER_MEMBERID (em_hdr), msz;
    switch (lc)
    {
      case LENGTH_CODE_1B: case LENGTH_CODE_2B: case LENGTH_CODE_4B: case LENGTH_CODE_8B:
        msz = 1u << lc;
        break;
      case LENGTH_CODE_NEXTINT:
        /* NEXTINT */
        if (!read_and_normalize_uint32 (&msz, data, off, size, bswap))
          return NULL;
        break;
      case LENGTH_CODE_ALSO_NEXTINT: case LENGTH_CODE_ALSO_NEXTINT4: case LENGTH_CODE_ALSO_NEXTINT8:
        /* length is part of serialized data */
        if (!peek_and_normalize_uint32 (&msz, data, off, size, bswap))
          return NULL;
        if (lc > LENGTH_CODE_ALSO_NEXTINT)
          msz <<= (lc - 4);
        break;
      default:
        abort ();
        break;
    }

    uint32_t insn, ops_csr = 0;
    bool found = false;
    while (!found && (insn = ops[ops_csr]) != DDS_OP_RTS)
    {
      assert (DDS_OP (insn) == DDS_OP_PLM);
      if (ops[ops_csr + 1] == mid)
      {
        const uint32_t *plm_ops = ops + ops_csr + DDS_OP_ADR_PLM (insn);
        dds_stream_normalize1 (data, off, size, bswap, xcdr_version, plm_ops);
        found = true;
        break;
      }
      ops_csr += 2;
    }

    if (!found)
    {
      *off += msz;
      if (lc >= LENGTH_CODE_ALSO_NEXTINT)
        *off += 4; /* length embedded in member does not include it's own 4 bytes */
    }
  }

  /* skip all PLM-memberid pairs */
  while (ops[0] != DDS_OP_RTS)
    ops += 2;

  return ops;
}

const uint32_t *dds_stream_normalize1 (char * __restrict data, uint32_t * __restrict off, uint32_t size, bool bswap, uint32_t xcdr_version, const uint32_t * __restrict ops)
{
  uint32_t insn;
  while ((insn = *ops) != DDS_OP_RTS)
  {
    switch (DDS_OP (insn))
    {
      case DDS_OP_ADR: {
        if ((ops = stream_normalize_adr (insn, data, off, size, bswap, xcdr_version, ops)) == NULL)
          return NULL;
        break;
      }
      case DDS_OP_JSR: {
        if (dds_stream_normalize1 (data, off, size, bswap, xcdr_version, ops + DDS_OP_JUMP (insn)) == NULL)
          return NULL;
        ops++;
        break;
      }
      case DDS_OP_RTS: case DDS_OP_JEQ: case DDS_OP_KOF: case DDS_OP_PLM: {
        abort ();
        break;
      }
      case DDS_OP_DLC: {
        assert (xcdr_version == CDR_ENC_VERSION_2);
        if ((ops = stream_normalize_delimited (data, off, size, bswap, xcdr_version, ops)) == NULL)
          return NULL;
        break;
      }
      case DDS_OP_PLC: {
        assert (xcdr_version == CDR_ENC_VERSION_2);
        if ((ops = stream_normalize_pl (data, off, size, bswap, xcdr_version, ops)) == NULL)
          return NULL;
        break;
      }
    }
  }
  return ops;
}

static bool stream_normalize_key_impl (void * __restrict data, uint32_t size, uint32_t *offs, bool bswap, uint32_t xcdr_version, const uint32_t *insnp, uint16_t key_offset_count, const uint32_t * key_offset_insn)
{
  assert (insn_key_ok_p (*insnp));
  switch (DDS_OP_TYPE (*insnp))
  {
    case DDS_OP_VAL_1BY: if (!normalize_uint8 (offs, size)) return false; break;
    case DDS_OP_VAL_2BY: if (!normalize_uint16 (data, offs, size, bswap)) return false; break;
    case DDS_OP_VAL_4BY: case DDS_OP_VAL_ENU: if (!normalize_uint32 (data, offs, size, bswap)) return false; break;
    case DDS_OP_VAL_8BY: if (!normalize_uint64 (data, offs, size, bswap, xcdr_version)) return false; break;
    case DDS_OP_VAL_STR: if (!normalize_string (data, offs, size, bswap, SIZE_MAX)) return false; break;
    case DDS_OP_VAL_BST: case DDS_OP_VAL_BSP: if (!normalize_string (data, offs, size, bswap, insnp[2])) return false; break;
    case DDS_OP_VAL_ARR: if (!normalize_arr (data, offs, size, bswap, xcdr_version, insnp, *insnp)) return false; break;
    case DDS_OP_VAL_EXT: {
      assert (key_offset_count > 0);
      const uint32_t *jsr_ops = insnp + DDS_OP_ADR_JSR (insnp[2]) + *key_offset_insn;
      stream_normalize_key_impl (data, size, offs, bswap, xcdr_version, jsr_ops, --key_offset_count, ++key_offset_insn);
      break;
    }
    case DDS_OP_VAL_SEQ: case DDS_OP_VAL_UNI: case DDS_OP_VAL_STU:
      abort ();
      break;
  }
  return true;
}

static bool stream_normalize_key (void * __restrict data, uint32_t size, bool bswap, uint32_t xcdr_version, const struct ddsi_sertype_default_desc * __restrict desc, uint32_t *actual_size)
{
  uint32_t offs = 0;
  for (uint32_t i = 0; i < desc->keys.nkeys; i++)
  {
    const uint32_t *op = desc->ops.ops + desc->keys.keys[i];
    switch (DDS_OP (*op))
    {
      case DDS_OP_KOF: {
        uint16_t n_offs = DDS_OP_LENGTH (*op);
        stream_normalize_key_impl (data, size, &offs, bswap, xcdr_version, desc->ops.ops + op[1], --n_offs, op + 2);
        break;
      }
      case DDS_OP_ADR: {
        stream_normalize_key_impl (data, size, &offs, bswap, xcdr_version, op, 0, NULL);
        break;
      }
      default:
        abort ();
        break;
    }
  }
  *actual_size = offs;
  return true;
}

bool dds_stream_normalize (void * __restrict data, uint32_t size, bool bswap, uint32_t xcdr_version, const struct ddsi_sertype_default * __restrict topic, bool just_key, uint32_t * __restrict actual_size)
{
  uint32_t off = 0;
  if (size > CDR_SIZE_MAX)
    return false;
  else if (just_key)
    return stream_normalize_key (data, size, bswap, xcdr_version, &topic->type, actual_size);
  else if (!dds_stream_normalize1 (data, &off, size, bswap, xcdr_version, topic->type.ops.ops))
    return false;
  else
  {
    *actual_size = off;
    return true;
  }
}

/*******************************************************************************************
 **
 **  Freeing samples
 **
 *******************************************************************************************/

static const uint32_t *dds_stream_free_sample_seq (char * __restrict addr, const uint32_t * __restrict ops, uint32_t insn)
{
  dds_sequence_t * const seq = (dds_sequence_t *) addr;
  uint32_t num = (seq->_maximum > seq->_length) ? seq->_maximum : seq->_length;
  const enum dds_stream_typecode subtype = DDS_OP_SUBTYPE (insn);
  ops += 2;
  if ((seq->_release && num) || subtype > DDS_OP_VAL_STR)
  {
    switch (subtype)
    {
      case DDS_OP_VAL_1BY: case DDS_OP_VAL_2BY: case DDS_OP_VAL_4BY: case DDS_OP_VAL_8BY: break;
      case DDS_OP_VAL_BST: case DDS_OP_VAL_ENU: ops++; break;
      case DDS_OP_VAL_BSP: ops++; /* fall through */
      case DDS_OP_VAL_STR: {
        char **ptr = (char **) seq->_buffer;
        while (num--)
          dds_free (*ptr++);
        break;
      }
      case DDS_OP_VAL_SEQ: case DDS_OP_VAL_ARR: case DDS_OP_VAL_UNI: case DDS_OP_VAL_STU: {
        const uint32_t elem_size = *ops++;
        const uint32_t *jsr_ops = ops + DDS_OP_ADR_JSR (*ops) - 3;
        const uint32_t jmp = DDS_OP_ADR_JMP (*ops);
        char *ptr = (char *) seq->_buffer;
        while (num--)
        {
          dds_stream_free_sample (ptr, jsr_ops);
          ptr += elem_size;
        }
        ops += jmp ? (jmp - 3) : 1;
        break;
      }
      case DDS_OP_VAL_EXT: {
        abort (); /* not supported */
        break;
      }
    }
  }
  if (seq->_release)
  {
    dds_free (seq->_buffer);
    seq->_maximum = 0;
    seq->_length = 0;
    seq->_buffer = NULL;
  }
  return ops;
}

static const uint32_t *dds_stream_free_sample_arr (char * __restrict addr, const uint32_t * __restrict ops, uint32_t insn)
{
  ops += 2;
  const enum dds_stream_typecode subtype = DDS_OP_SUBTYPE (insn);
  uint32_t num = *ops++;
  switch (subtype)
  {
    case DDS_OP_VAL_1BY: case DDS_OP_VAL_2BY: case DDS_OP_VAL_4BY: case DDS_OP_VAL_8BY: break;
    case DDS_OP_VAL_ENU: ops++; break;
    case DDS_OP_VAL_BST: ops += 2; break;
    case DDS_OP_VAL_BSP: ops += 2; /* fall through */
    case DDS_OP_VAL_STR: {
      char **ptr = (char **) addr;
      while (num--)
        dds_free (*ptr++);
      break;
    }
    case DDS_OP_VAL_SEQ: case DDS_OP_VAL_ARR: case DDS_OP_VAL_UNI: case DDS_OP_VAL_STU: {
      const uint32_t *jsr_ops = ops + DDS_OP_ADR_JSR (*ops) - 3;
      const uint32_t jmp = DDS_OP_ADR_JMP (*ops);
      const uint32_t elem_size = ops[1];
      while (num--)
      {
        dds_stream_free_sample (addr, jsr_ops);
        addr += elem_size;
      }
      ops += jmp ? (jmp - 3) : 2;
      break;
    }
    case DDS_OP_VAL_EXT: {
      abort (); /* not supported */
      break;
    }
  }
  return ops;
}

static const uint32_t *dds_stream_free_sample_uni (char * __restrict discaddr, char * __restrict baseaddr, const uint32_t * __restrict ops, uint32_t insn)
{
  uint32_t disc = 0;
  switch (DDS_OP_SUBTYPE (insn))
  {
    case DDS_OP_VAL_1BY: disc = *((uint8_t *) discaddr); break;
    case DDS_OP_VAL_2BY: disc = *((uint16_t *) discaddr); break;
    case DDS_OP_VAL_4BY: case DDS_OP_VAL_ENU: disc = *((uint32_t *) discaddr); break;
    default: abort(); break;
  }
  uint32_t const * const jeq_op = find_union_case (ops, disc);
  ops += DDS_OP_ADR_JMP (ops[3]);
  if (jeq_op)
  {
    const enum dds_stream_typecode subtype = DDS_JEQ_TYPE (jeq_op[0]);
    void *valaddr = baseaddr + jeq_op[2];
    switch (subtype)
    {
      case DDS_OP_VAL_1BY: case DDS_OP_VAL_2BY: case DDS_OP_VAL_4BY: case DDS_OP_VAL_8BY: case DDS_OP_VAL_BST: case DDS_OP_VAL_ENU: break;
      case DDS_OP_VAL_BSP: case DDS_OP_VAL_STR: {
        dds_free (*((char **) valaddr));
        *((char **) valaddr) = NULL;
        break;
      }
      case DDS_OP_VAL_SEQ: case DDS_OP_VAL_ARR: case DDS_OP_VAL_UNI: case DDS_OP_VAL_STU: {
        dds_stream_free_sample (valaddr, jeq_op + DDS_OP_ADR_JSR (jeq_op[0]));
        break;
      }
      case DDS_OP_VAL_EXT: {
        abort (); /* not supported */
        break;
      }
    }
  }
  return ops;
}

static const uint32_t *dds_stream_free_sample_pl (char * __restrict addr, const uint32_t * __restrict ops)
{
  uint32_t insn;
  ops++; /* skip PLC op */
  while ((insn = *ops) != DDS_OP_RTS)
  {
    switch (DDS_OP (insn))
    {
      case DDS_OP_PLM: {
        const uint32_t *plm_ops = ops + DDS_OP_ADR_PLM (insn);
        dds_stream_free_sample (addr, plm_ops);
        ops += 2;
        break;
      }
      default:
        abort (); /* other ops not supported at this point */
        break;
    }
  }
  return ops;
}

void dds_stream_free_sample (void * __restrict data, const uint32_t * __restrict ops)
{
  uint32_t insn;
  while ((insn = *ops) != DDS_OP_RTS)
  {
    switch (DDS_OP (insn))
    {
      case DDS_OP_ADR: {
        void *addr = (char *) data + ops[1];
        switch (DDS_OP_TYPE (insn))
        {
          case DDS_OP_VAL_1BY: case DDS_OP_VAL_2BY: case DDS_OP_VAL_4BY: case DDS_OP_VAL_8BY: ops += 2; break;
          case DDS_OP_VAL_BSP: ops++; /* fall through */
          case DDS_OP_VAL_STR: {
            dds_free (*((char **) addr));
            *((char **) addr) = NULL;
            ops += 2;
            break;
          }
          case DDS_OP_VAL_BST: case DDS_OP_VAL_ENU: ops += 3; break;
          case DDS_OP_VAL_SEQ: ops = dds_stream_free_sample_seq (addr, ops, insn); break;
          case DDS_OP_VAL_ARR: ops = dds_stream_free_sample_arr (addr, ops, insn); break;
          case DDS_OP_VAL_UNI: ops = dds_stream_free_sample_uni (addr, data, ops, insn); break;
          case DDS_OP_VAL_EXT: {
            const uint32_t *jsr_ops = ops + DDS_OP_ADR_JSR (ops[2]);
            const uint32_t jmp = DDS_OP_ADR_JMP (ops[2]);
            uint32_t flags = DDS_OP_FLAGS (insn);
            if (flags & DDS_OP_FLAG_EXT)
            {
              dds_stream_free_sample (*((char **) addr), jsr_ops);
              dds_free (*((char **) addr));
              *((char **) addr) = NULL;
            }
            else
              dds_stream_free_sample (addr, jsr_ops);
            ops += jmp ? jmp : 3;
            break;
          }
          case DDS_OP_VAL_STU:
            abort (); /* op type STU only supported as subtype */
            break;
        }
        break;
      }
      case DDS_OP_JSR: {
        dds_stream_free_sample (data, ops + DDS_OP_JUMP (insn));
        ops++;
        break;
      }
      case DDS_OP_RTS: case DDS_OP_JEQ: case DDS_OP_KOF: case DDS_OP_PLM: {
        abort ();
        break;
      }
      case DDS_OP_DLC: {
        ops++;
        break;
      }
      case DDS_OP_PLC: {
        ops = dds_stream_free_sample_pl (data, ops);
        break;
      }
    }
  }
}

/*******************************************************************************************
 **
 **  Extracting key/keyhash (the only difference that a keyhash MUST be big-endian,
 **  padding MUST be cleared, and that it may be necessary to run the value through
 **  MD5.
 **
 *******************************************************************************************/

static void dds_stream_extract_key_from_key_prim_op (dds_istream_t * __restrict is, dds_ostream_t * __restrict os, const uint32_t * __restrict op, uint16_t key_offset_count, const uint32_t * key_offset_insn)
{
  assert ((*op & DDS_OP_FLAG_KEY) && ((DDS_OP (*op)) == DDS_OP_ADR));
  switch (DDS_OP_TYPE (*op))
  {
    case DDS_OP_VAL_1BY: dds_os_put1 (os, dds_is_get1 (is)); break;
    case DDS_OP_VAL_2BY: dds_os_put2 (os, dds_is_get2 (is)); break;
    case DDS_OP_VAL_4BY: case DDS_OP_VAL_ENU: dds_os_put4 (os, dds_is_get4 (is)); break;
    case DDS_OP_VAL_8BY: dds_os_put8 (os, dds_is_get8 (is)); break;
    case DDS_OP_VAL_STR: case DDS_OP_VAL_BST: case DDS_OP_VAL_BSP: {
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
      break;
    }
    case DDS_OP_VAL_EXT: {
      assert (key_offset_count > 0);
      const uint32_t *jsr_ops = op + DDS_OP_ADR_JSR (op[2]) + *key_offset_insn;
      dds_stream_extract_key_from_key_prim_op (is, os, jsr_ops, --key_offset_count, ++key_offset_insn);
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
      {
        *(uint32_t *) &dst[i] = ddsrt_bswap4u (* (((uint32_t *) &src[i]) + 1));
        *(((uint32_t *) &dst[i]) + 1) = ddsrt_bswap4u (* (uint32_t *) &src[i]);
      }
      break;
    }
  }
}
#endif

static void dds_stream_extract_keyBE_from_key_prim_op (dds_istream_t * __restrict is, dds_ostreamBE_t * __restrict os, const uint32_t * __restrict op, uint16_t key_offset_count, const uint32_t * key_offset_insn)
{
  assert ((*op & DDS_OP_FLAG_KEY) && ((DDS_OP (*op)) == DDS_OP_ADR));
  switch (DDS_OP_TYPE (*op))
  {
    case DDS_OP_VAL_1BY: dds_os_put1BE (os, dds_is_get1 (is)); break;
    case DDS_OP_VAL_2BY: dds_os_put2BE (os, dds_is_get2 (is)); break;
    case DDS_OP_VAL_4BY: case DDS_OP_VAL_ENU: dds_os_put4BE (os, dds_is_get4 (is)); break;
    case DDS_OP_VAL_8BY: dds_os_put8BE (os, dds_is_get8 (is)); break;
    case DDS_OP_VAL_STR: case DDS_OP_VAL_BST: case DDS_OP_VAL_BSP: {
      uint32_t sz = dds_is_get4 (is);
      dds_os_put4BE (os, sz);
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
      dds_cdr_alignto_clear_and_resizeBE (os, align, num * align);
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
    case DDS_OP_VAL_EXT: {
      assert (key_offset_count > 0);
      const uint32_t *jsr_ops = op + DDS_OP_ADR_JSR (op[2]) + *key_offset_insn;
      dds_stream_extract_keyBE_from_key_prim_op (is, os, jsr_ops, --key_offset_count, ++key_offset_insn);
      break;
    }
    case DDS_OP_VAL_SEQ: case DDS_OP_VAL_UNI: case DDS_OP_VAL_STU: {
      abort ();
      break;
    }
  }
}

void dds_stream_extract_keyBE_from_key (dds_istream_t * __restrict is, dds_ostreamBE_t * __restrict os, const struct ddsi_sertype_default * __restrict type)
{
  const struct ddsi_sertype_default_desc *desc = &type->type;
  for (uint32_t i = 0; i < desc->keys.nkeys; i++)
  {
    uint32_t const * const op = desc->ops.ops + desc->keys.keys[i];
    switch (DDS_OP (*op))
    {
      case DDS_OP_KOF: {
        uint16_t n_offs = DDS_OP_LENGTH (*op);
        dds_stream_extract_keyBE_from_key_prim_op (is, os, desc->ops.ops + op[1], --n_offs, op + 2);
        break;
      }
      case DDS_OP_ADR: {
        dds_stream_extract_keyBE_from_key_prim_op (is, os, op, 0, NULL);
        break;
      }
      default:
        abort ();
        break;
    }
  }
}

static void dds_stream_extract_key_from_data_skip_subtype (dds_istream_t * __restrict is, uint32_t num, uint32_t subtype, const uint32_t * __restrict subops)
{
  switch (subtype)
  {
    case DDS_OP_VAL_1BY: case DDS_OP_VAL_2BY: case DDS_OP_VAL_4BY: case DDS_OP_VAL_8BY: case DDS_OP_VAL_ENU: {
      const uint32_t elem_size = get_type_size (subtype);
      dds_cdr_alignto (is, elem_size);
      is->m_index += num * elem_size;
      break;
    }
    case DDS_OP_VAL_STR: case DDS_OP_VAL_BST: case DDS_OP_VAL_BSP: {
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
    case DDS_OP_VAL_EXT: {
      abort (); /* not supported */
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
  if (subtype > DDS_OP_VAL_8BY && is->m_xcdr_version == CDR_ENC_VERSION_2)
  {
    const uint32_t sz = dds_is_get4 (is);
    is->m_index += sz;
  }
  else if (type_has_subtype_or_members (subtype))
    dds_stream_extract_key_from_data_skip_subtype (is, num, subtype, ops + DDS_OP_ADR_JSR (ops[3]));
  else
    dds_stream_extract_key_from_data_skip_subtype (is, num, subtype, NULL);
  return skip_array_insns (op, ops);
}

static const uint32_t *dds_stream_extract_key_from_data_skip_sequence (dds_istream_t * __restrict is, const uint32_t * __restrict ops)
{
  const uint32_t op = *ops;
  assert (DDS_OP_TYPE (op) == DDS_OP_VAL_SEQ);
  const uint32_t num = dds_is_get4 (is);
  if (num > 0)
  {
    const uint32_t subtype = DDS_OP_SUBTYPE (op);
    if (subtype > DDS_OP_VAL_8BY && is->m_xcdr_version == CDR_ENC_VERSION_2)
    {
      const uint32_t sz = dds_is_get4 (is);
      is->m_index += sz;
    }
    else if (type_has_subtype_or_members (subtype))
      dds_stream_extract_key_from_data_skip_subtype (is, num, subtype, ops + DDS_OP_ADR_JSR (ops[3]));
    else
      dds_stream_extract_key_from_data_skip_subtype (is, num, subtype, NULL);
  }
  return skip_sequence_insns (op, ops);
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

static const uint32_t *dds_stream_extract_key_from_data_skip_adr (dds_istream_t * __restrict is, const uint32_t * __restrict ops, uint32_t type)
{
  switch (type)
  {
    case DDS_OP_VAL_1BY: case DDS_OP_VAL_2BY: case DDS_OP_VAL_4BY: case DDS_OP_VAL_8BY: case DDS_OP_VAL_STR: case DDS_OP_VAL_BST: case DDS_OP_VAL_BSP: case DDS_OP_VAL_ENU:
      dds_stream_extract_key_from_data_skip_subtype (is, 1, type, NULL);
      ops += 2 + (type == DDS_OP_VAL_BST || type == DDS_OP_VAL_BSP || type == DDS_OP_VAL_ARR || type == DDS_OP_VAL_ENU);
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
      abort (); /* op type STU only supported as subtype */
      break;
  }
  return ops;
}

/*******************************************************************************************
 **
 **  Read/write of samples and keys -- i.e., DDSI payloads.
 **
 *******************************************************************************************/

void dds_stream_read_sample (dds_istream_t * __restrict is, void * __restrict data, const struct ddsi_sertype_default * __restrict type)
{
  const struct ddsi_sertype_default_desc *desc = &type->type;
  if (type->opt_size)
  {
    /* Layout of struct & CDR is the same, but sizeof(struct) may include padding at
       the end that is not present in CDR, so we must use type->opt_size to avoid a
       potential out-of-bounds read */
    dds_is_get_bytes (is, data, (uint32_t) type->opt_size, 1);
  }
  else
  {
    if (desc->flagset & DDS_TOPIC_CONTAINS_UNION)
    {
      /* Switching union cases causes big trouble if some cases have sequences or strings,
         and other cases have other things mapped to those addresses.  So, pretend to be
         nice by freeing whatever was allocated, then clearing all memory.  This will
         make any preallocated buffers go to waste, but it does allow reusing the message
         from read-to-read, at the somewhat reasonable price of a slower deserialization
         and not being able to use preallocated sequences in topics containing unions. */
      dds_stream_free_sample (data, desc->ops.ops);
      memset (data, 0, desc->size);
    }
    (void) dds_stream_read (is, data, desc->ops.ops);
  }
}

static void dds_stream_read_key_impl (dds_istream_t * __restrict is, char * __restrict sample, const uint32_t *insnp, uint16_t key_offset_count, const uint32_t * key_offset_insn)
{
  char *dst = sample + insnp[1];
  assert (insn_key_ok_p (*insnp));
  switch (DDS_OP_TYPE (*insnp))
  {
    case DDS_OP_VAL_1BY: *((uint8_t *) dst) = dds_is_get1 (is); break;
    case DDS_OP_VAL_2BY: *((uint16_t *) dst) = dds_is_get2 (is); break;
    case DDS_OP_VAL_4BY: case DDS_OP_VAL_ENU: *((uint32_t *) dst) = dds_is_get4 (is); break;
    case DDS_OP_VAL_8BY: *((uint64_t *) dst) = dds_is_get8 (is); break;
    case DDS_OP_VAL_STR: *((char **) dst) = dds_stream_reuse_string (is, *((char **) dst)); break;
    case DDS_OP_VAL_BST: (void) dds_stream_reuse_string_bound (is, dst, insnp[2], false); break;
    case DDS_OP_VAL_BSP: *((char **) dst) = dds_stream_reuse_string_bound (is, dst, insnp[2], true); break;
    case DDS_OP_VAL_ARR: dds_is_get_bytes (is, dst, insnp[2], get_type_size (DDS_OP_SUBTYPE (*insnp))); break;
    case DDS_OP_VAL_SEQ: case DDS_OP_VAL_UNI: case DDS_OP_VAL_STU: abort (); break;
    case DDS_OP_VAL_EXT:
    {
      assert (key_offset_count > 0);
      const uint32_t *jsr_ops = insnp + DDS_OP_ADR_JSR (insnp[2]) + *key_offset_insn;
      dds_stream_read_key_impl (is, dst, jsr_ops, --key_offset_count, ++key_offset_insn);
      break;
    }
  }
}

void dds_stream_read_key (dds_istream_t * __restrict is, char * __restrict sample, const struct ddsi_sertype_default * __restrict type)
{
  const struct ddsi_sertype_default_desc *desc = &type->type;
  for (uint32_t i = 0; i < desc->keys.nkeys; i++)
  {
    const uint32_t *op = desc->ops.ops + desc->keys.keys[i];
    switch (DDS_OP (*op))
    {
      case DDS_OP_KOF: {
        uint16_t n_offs = DDS_OP_LENGTH (*op);
        dds_stream_read_key_impl (is, sample, desc->ops.ops + op[1], --n_offs, op + 2);
        break;
      }
      case DDS_OP_ADR: {
        dds_stream_read_key_impl (is, sample, op, 0, NULL);
        break;
      }
      default:
        abort ();
        break;
    }
  }
}

/* Used in dds_stream_write_key for writing keys in native endianness, so no
   swap is needed in that case and this function is a no-op */
static inline void dds_stream_swap_if_needed_insitu (void * __restrict vbuf, uint32_t size, uint32_t num)
{
  (void) vbuf;
  (void) size;
  (void) num;
}

// Native endianness
#define NAME_BYTE_ORDER_EXT
#include "ddsi_cdrstream_keys.part.c"
#undef NAME_BYTE_ORDER_EXT

#if DDSRT_ENDIAN == DDSRT_LITTLE_ENDIAN

static void dds_stream_swap_if_needed_insituBE (void * __restrict vbuf, uint32_t size, uint32_t num)
{
  dds_stream_swap (vbuf, size, num);
}

// Big-endian implementation
#define NAME_BYTE_ORDER_EXT BE
#include "ddsi_cdrstream_keys.part.c"
#undef NAME_BYTE_ORDER_EXT

#else /* if DDSRT_ENDIAN == DDSRT_LITTLE_ENDIAN */

void dds_stream_write_keyBE (dds_ostreamBE_t * __restrict os, const char * __restrict sample, const struct ddsi_sertype_default * __restrict type)
{
  dds_stream_write_key (&os->x, sample, type);
}

#endif /* if DDSRT_ENDIAN == DDSRT_LITTLE_ENDIAN */

/*******************************************************************************************
 **
 **  Pretty-printing
 **
 *******************************************************************************************/

/* Returns true if buffer not yet exhausted, false otherwise */
static bool prtf (char * __restrict *buf, size_t * __restrict bufsize, const char *fmt, ...)
  ddsrt_attribute_format_printf(3, 4);

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

static bool prtf_simple (char * __restrict *buf, size_t * __restrict bufsize, dds_istream_t * __restrict is, enum dds_stream_typecode type, unsigned flags)
{
  switch (type)
  {
    case DDS_OP_VAL_1BY: {
      const union { int8_t s; uint8_t u; } x = { .u = dds_is_get1 (is) };
      if (flags & DDS_OP_FLAG_SGN)
        return prtf (buf, bufsize, "%"PRId8, x.s);
      else
        return prtf (buf, bufsize, "%"PRIu8, x.u);
    }
    case DDS_OP_VAL_2BY: {
      const union { int16_t s; uint16_t u; } x = { .u = dds_is_get2 (is) };
      if (flags & DDS_OP_FLAG_SGN)
        return prtf (buf, bufsize, "%"PRId16, x.s);
      else
        return prtf (buf, bufsize, "%"PRIu16, x.u);
    }
    case DDS_OP_VAL_4BY: case DDS_OP_VAL_ENU: {
      const union { int32_t s; uint32_t u; float f; } x = { .u = dds_is_get4 (is) };
      if (flags & DDS_OP_FLAG_FP)
        return prtf (buf, bufsize, "%g", x.f);
      else if (flags & DDS_OP_FLAG_SGN)
        return prtf (buf, bufsize, "%"PRId32, x.s);
      else
        return prtf (buf, bufsize, "%"PRIu32, x.u);
    }
    case DDS_OP_VAL_8BY: {
      const union { int64_t s; uint64_t u; double f; } x = { .u = dds_is_get8 (is) };
      if (flags & DDS_OP_FLAG_FP)
        return prtf (buf, bufsize, "%g", x.f);
      else if (flags & DDS_OP_FLAG_SGN)
        return prtf (buf, bufsize, "%"PRId64, x.s);
      else
        return prtf (buf, bufsize, "%"PRIu64, x.u);
    }
    case DDS_OP_VAL_STR: case DDS_OP_VAL_BST: case DDS_OP_VAL_BSP: return prtf_str (buf, bufsize, is);
    case DDS_OP_VAL_ARR: case DDS_OP_VAL_SEQ: case DDS_OP_VAL_UNI: case DDS_OP_VAL_STU: case DDS_OP_VAL_EXT:
      abort ();
  }
  return false;
}

static bool prtf_simple_array (char * __restrict *buf, size_t * __restrict bufsize, dds_istream_t * __restrict is, uint32_t num, enum dds_stream_typecode type, unsigned flags)
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
          cont = prtf_simple (buf, bufsize, is, type, flags);
          i++;
        }
      }
      break;
    }
    case DDS_OP_VAL_2BY: case DDS_OP_VAL_4BY: case DDS_OP_VAL_8BY: case DDS_OP_VAL_ENU:
    case DDS_OP_VAL_STR: case DDS_OP_VAL_BST: case DDS_OP_VAL_BSP:
      for (size_t i = 0; cont && i < num; i++)
      {
        if (i != 0)
          (void) prtf (buf, bufsize, ",");
        cont = prtf_simple (buf, bufsize, is, type, flags);
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
  if (subtype > DDS_OP_VAL_8BY && is->m_xcdr_version == CDR_ENC_VERSION_2)
  {
    /* skip DHEADER */
    dds_is_get4 (is);
  }

  const uint32_t num = dds_is_get4 (is);
  if (num == 0)
  {
    (void) prtf (buf, bufsize, "{}");
    return skip_sequence_insns (insn, ops);
  }
  switch (subtype)
  {
    case DDS_OP_VAL_1BY: case DDS_OP_VAL_2BY: case DDS_OP_VAL_4BY: case DDS_OP_VAL_8BY:
      (void) prtf_simple_array (buf, bufsize, is, num, subtype, DDS_OP_FLAGS (insn));
      return ops + 2;
    case DDS_OP_VAL_STR: case DDS_OP_VAL_BST: case DDS_OP_VAL_BSP: case DDS_OP_VAL_ENU:
      (void) prtf_simple_array (buf, bufsize, is, num, subtype, DDS_OP_FLAGS (insn));
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
    case DDS_OP_VAL_EXT: {
      abort (); /* not supported */
      break;
    }
  }
  return NULL;
}

static const uint32_t *prtf_arr (char * __restrict *buf, size_t *bufsize, dds_istream_t * __restrict is, const uint32_t * __restrict ops, uint32_t insn)
{
  const enum dds_stream_typecode subtype = DDS_OP_SUBTYPE (insn);
  if (subtype > DDS_OP_VAL_8BY && is->m_xcdr_version == CDR_ENC_VERSION_2)
  {
    /* skip DHEADER */
    dds_is_get4 (is);
  }
  const uint32_t num = ops[2];
  switch (subtype)
  {
    case DDS_OP_VAL_1BY: case DDS_OP_VAL_2BY: case DDS_OP_VAL_4BY: case DDS_OP_VAL_8BY:
      (void) prtf_simple_array (buf, bufsize, is, num, subtype, DDS_OP_FLAGS (insn));
      return ops + 3;
    case DDS_OP_VAL_STR: case DDS_OP_VAL_BST: case DDS_OP_VAL_BSP: case DDS_OP_VAL_ENU:
      (void) prtf_simple_array (buf, bufsize, is, num, subtype, DDS_OP_FLAGS (insn));
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
    case DDS_OP_VAL_EXT: {
      abort (); /* not supported */
      break;
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
      case DDS_OP_VAL_1BY: case DDS_OP_VAL_2BY: case DDS_OP_VAL_4BY: case DDS_OP_VAL_8BY: case DDS_OP_VAL_ENU:
      case DDS_OP_VAL_STR: case DDS_OP_VAL_BST: case DDS_OP_VAL_BSP:
        (void) prtf_simple (buf, bufsize, is, valtype, DDS_OP_FLAGS (jeq_op[0]));
        break;
      case DDS_OP_VAL_SEQ: case DDS_OP_VAL_ARR: case DDS_OP_VAL_UNI: case DDS_OP_VAL_STU:
        (void) dds_stream_print_sample1 (buf, bufsize, is, jeq_op + DDS_OP_ADR_JSR (jeq_op[0]), valtype == DDS_OP_VAL_STU);
        break;
      case DDS_OP_VAL_EXT: {
        abort (); /* not supported, use UNI instead */
        break;
      }
    }
  }
  return ops;
}

static const uint32_t *prtf_pl (char * __restrict *buf, size_t *bufsize, dds_istream_t * __restrict is, const uint32_t * __restrict ops)
{
  /* skip PLC op */
  ops++;

  uint32_t pl_sz = dds_is_get4 (is), pl_offs = is->m_index;
  if (!prtf (buf, bufsize, "pl:%d", pl_sz))
    return NULL;

  while (is->m_index - pl_offs < pl_sz)
  {
    /* read emheader and next_int */
    uint32_t em_hdr = dds_is_get4 (is);
    uint32_t lc = EMHEADER_LENGTH_CODE (em_hdr), mid = EMHEADER_MEMBERID (em_hdr), msz;
    if (!prtf (buf, bufsize, ",lc:%d,m:%d,", lc, mid))
      return NULL;
    switch (lc)
    {
      case LENGTH_CODE_1B: case LENGTH_CODE_2B: case LENGTH_CODE_4B: case LENGTH_CODE_8B:
        msz = 1u << lc;
        break;
      case LENGTH_CODE_NEXTINT:
        msz = dds_is_get4 (is); /* next-int */
        break;
      case LENGTH_CODE_ALSO_NEXTINT: case LENGTH_CODE_ALSO_NEXTINT4: case LENGTH_CODE_ALSO_NEXTINT8:
        msz = dds_is_peek4 (is); /* length is part of serialized data */
        if (lc > LENGTH_CODE_ALSO_NEXTINT)
          msz <<= (lc - 4);
        break;
      default:
        abort ();
        break;
    }

    /* find member and deserialize */
    uint32_t insn, ops_csr = 0;
    bool found = false;
    while (!found && (insn = ops[ops_csr]) != DDS_OP_RTS)
    {
      assert (DDS_OP (insn) == DDS_OP_PLM);
      if (ops[ops_csr + 1] == mid)
      {
        const uint32_t *plm_ops = ops + ops_csr + DDS_OP_ADR_PLM (insn);
        (void) dds_stream_print_sample1 (buf, bufsize, is, plm_ops, true);
        found = true;
        break;
      }
      ops_csr += 2;
    }

    if (!found)
    {
      is->m_index += msz;
      if (lc >= LENGTH_CODE_ALSO_NEXTINT)
        is->m_index += 4; /* length embedded in member does not include it's own 4 bytes */
    }
  }

  /* skip all PLM-memberid pairs */
  while (ops[0] != DDS_OP_RTS)
    ops += 2;

  return ops;
}

static bool dds_stream_print_sample1 (char * __restrict *buf, size_t * __restrict bufsize, dds_istream_t * __restrict is, const uint32_t * __restrict ops, bool add_braces)
{
  uint32_t insn, delimited_sz = 0, delimited_offs = 0;
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
        if (!delimited_sz || (is->m_index - delimited_offs < delimited_sz))
        {
          switch (DDS_OP_TYPE (insn))
          {
            case DDS_OP_VAL_1BY: case DDS_OP_VAL_2BY: case DDS_OP_VAL_4BY: case DDS_OP_VAL_8BY:
            case DDS_OP_VAL_STR:
              cont = prtf_simple (buf, bufsize, is, DDS_OP_TYPE (insn), DDS_OP_FLAGS (insn));
              ops += 2;
              break;
            case DDS_OP_VAL_BST: case DDS_OP_VAL_BSP: case DDS_OP_VAL_ENU:
              cont = prtf_simple (buf, bufsize, is, DDS_OP_TYPE (insn), DDS_OP_FLAGS (insn));
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
            case DDS_OP_VAL_EXT: {
              const uint32_t *jsr_ops = ops + DDS_OP_ADR_JSR (ops[2]);
              const uint32_t jmp = DDS_OP_ADR_JMP (ops[2]);
              cont = dds_stream_print_sample1 (buf, bufsize, is, jsr_ops, true);
              ops += jmp ? jmp : 3;
              break;
            }
            case DDS_OP_VAL_STU:
              abort (); /* op type STU only supported as subtype */
              break;
          }
        }
        else
        {
          /* skip fields that are not in serialized data for appendable type */
          ops = dds_stream_skip_adr (insn, ops);
          cont = prtf (buf, bufsize, "_");
        }
        break;
      }
      case DDS_OP_JSR: {
        cont = dds_stream_print_sample1 (buf, bufsize, is, ops + DDS_OP_JUMP (insn), true);
        ops++;
        break;
      }
      case DDS_OP_RTS: case DDS_OP_JEQ: case DDS_OP_KOF: case DDS_OP_PLM: {
        abort ();
        break;
      }
      case DDS_OP_DLC: {
        assert (is->m_xcdr_version == CDR_ENC_VERSION_2);
        cont = prtf (buf, bufsize, "dlh:");
        if (cont)
        {
          dds_cdr_alignto (is, 4);
          delimited_sz = * ((uint32_t *) (is->m_buffer + is->m_index));
          cont = prtf_simple (buf, bufsize, is, DDS_OP_VAL_4BY, 0);
          delimited_offs = is->m_index;
          ops++;
        }
        break;
      }
      case DDS_OP_PLC: {
        assert (is->m_xcdr_version == CDR_ENC_VERSION_2);
        ops = prtf_pl (buf, bufsize, is, ops);
        break;
      }
    }
  }
  /* Skip remainder of serialized data for this appendable type */
  if (delimited_sz && (delimited_sz > is->m_index - delimited_offs))
    is->m_index += delimited_sz - (is->m_index - delimited_offs);
  if (add_braces)
    (void) prtf (buf, bufsize, "}");
  return cont;
}

size_t dds_stream_print_sample (dds_istream_t * __restrict is, const struct ddsi_sertype_default * __restrict type, char * __restrict buf, size_t bufsize)
{
  (void) dds_stream_print_sample1 (&buf, &bufsize, is, type->type.ops.ops, true);
  return bufsize;
}

static size_t dds_stream_print_key_impl (dds_istream_t * __restrict is, const uint32_t *op, uint16_t key_offset_count, const uint32_t * key_offset_insn,
  char * __restrict buf, size_t bufsize, bool *cont)
{
  assert (insn_key_ok_p (*op));
  assert (cont);
  switch (DDS_OP_TYPE (*op))
  {
    case DDS_OP_VAL_1BY: case DDS_OP_VAL_2BY: case DDS_OP_VAL_4BY: case DDS_OP_VAL_8BY: case DDS_OP_VAL_ENU:
    case DDS_OP_VAL_STR: case DDS_OP_VAL_BST: case DDS_OP_VAL_BSP:
      *cont = prtf_simple (&buf, &bufsize, is, DDS_OP_TYPE (*op), DDS_OP_FLAGS (*op));
      break;
    case DDS_OP_VAL_ARR:
      *cont = prtf_simple_array (&buf, &bufsize, is, op[2], DDS_OP_SUBTYPE (*op), DDS_OP_FLAGS (*op));
      break;
    case DDS_OP_VAL_SEQ: case DDS_OP_VAL_UNI: case DDS_OP_VAL_STU:
      abort ();
      break;
    case DDS_OP_VAL_EXT:
    {
      assert (key_offset_count > 0);
      const uint32_t *jsr_ops = op + DDS_OP_ADR_JSR (op[2]) + *key_offset_insn;
      dds_stream_print_key_impl (is, jsr_ops, --key_offset_count, ++key_offset_insn, buf, bufsize, cont);
      break;
    }
  }
  return bufsize;
}

size_t dds_stream_print_key (dds_istream_t * __restrict is, const struct ddsi_sertype_default * __restrict type, char * __restrict buf, size_t bufsize)
{
  const struct ddsi_sertype_default_desc *desc = &type->type;
  bool cont = prtf (&buf, &bufsize, ":k:{");
  for (uint32_t i = 0; cont && i < desc->keys.nkeys; i++)
  {
    const uint32_t *op = desc->ops.ops + desc->keys.keys[i];
    switch (DDS_OP (*op))
    {
      case DDS_OP_KOF: {
        uint16_t n_offs = DDS_OP_LENGTH (*op);
        dds_stream_print_key_impl (is, desc->ops.ops + op[1], --n_offs, op + 2, buf, bufsize, &cont);
        break;
      }
      case DDS_OP_ADR: {
        dds_stream_print_key_impl (is, op, 0, NULL, buf, bufsize, &cont);
        break;
      }
      default:
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
  assert (CDR_ENC_LE (d->hdr.identifier));
#elif DDSRT_ENDIAN == DDSRT_BIG_ENDIAN
  assert (!CDR_ENC_LE (d->hdr.identifier));
#endif
  s->m_xcdr_version = get_xcdr_version (d->hdr.identifier);
}

void dds_ostream_from_serdata_default (dds_ostream_t * __restrict s, const struct ddsi_serdata_default * __restrict d)
{
  s->m_buffer = (unsigned char *) d;
  s->m_index = (uint32_t) offsetof (struct ddsi_serdata_default, data);
  s->m_size = d->size + s->m_index;
#if DDSRT_ENDIAN == DDSRT_LITTLE_ENDIAN
  assert (CDR_ENC_LE (d->hdr.identifier));
#elif DDSRT_ENDIAN == DDSRT_BIG_ENDIAN
  assert (!CDR_ENC_LE (d->hdr.identifier));
#endif
  s->m_xcdr_version = get_xcdr_version (d->hdr.identifier);
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

/* Gets the (minimum) extensibility of the types used for this topic, and returns if XCDR
   is required for (de)serializing the type for this topic descriptor (maybe move this
   check to idl-compile time?) */
bool dds_stream_has_dynamic_type (const uint32_t * __restrict ops)
{
  bool has_dynamic_type = false;
  const uint32_t *ops_end = ops;
  dds_stream_countops1 (ops, &ops_end, &has_dynamic_type);
  return has_dynamic_type;
}
