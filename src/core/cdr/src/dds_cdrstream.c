// Copyright(c) 2006 to 2022 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <assert.h>
#include <string.h>
#include <ctype.h>

#include "dds/ddsrt/endian.h"
#include "dds/ddsrt/md5.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/string.h"
#include "dds/ddsrt/static_assert.h"
#include "dds/cdr/dds_cdrstream.h"

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

#define ddsrt_to2u(n) (n)
#define ddsrt_to4u(n) (n)
#define ddsrt_to8u(n) (n)
#define to_BO4u NAME2_BYTE_ORDER(ddsrt_to, 4u)

#define dds_os_put1BO                                 NAME_BYTE_ORDER(dds_os_put1)
#define dds_os_put2BO                                 NAME_BYTE_ORDER(dds_os_put2)
#define dds_os_put4BO                                 NAME_BYTE_ORDER(dds_os_put4)
#define dds_os_put8BO                                 NAME_BYTE_ORDER(dds_os_put8)
#define dds_os_reserve4BO                             NAME_BYTE_ORDER(dds_os_reserve4)
#define dds_os_reserve8BO                             NAME_BYTE_ORDER(dds_os_reserve8)
#define dds_ostreamBO_fini                            NAME2_BYTE_ORDER(dds_ostream, _fini)
#define dds_stream_write_stringBO                     NAME_BYTE_ORDER(dds_stream_write_string)
#define dds_stream_write_seqBO                        NAME_BYTE_ORDER(dds_stream_write_seq)
#define dds_stream_write_arrBO                        NAME_BYTE_ORDER(dds_stream_write_arr)
#define dds_stream_write_bool_valueBO                 NAME_BYTE_ORDER(dds_stream_write_bool_value)
#define dds_stream_write_bool_arrBO                   NAME_BYTE_ORDER(dds_stream_write_bool_arr)
#define dds_stream_write_enum_valueBO                 NAME_BYTE_ORDER(dds_stream_write_enum_value)
#define dds_stream_write_enum_arrBO                   NAME_BYTE_ORDER(dds_stream_write_enum_arr)
#define dds_stream_write_bitmask_valueBO              NAME_BYTE_ORDER(dds_stream_write_bitmask_value)
#define dds_stream_write_bitmask_arrBO                NAME_BYTE_ORDER(dds_stream_write_bitmask_arr)
#define dds_stream_write_union_discriminantBO         NAME_BYTE_ORDER(dds_stream_write_union_discriminant)
#define dds_stream_write_uniBO                        NAME_BYTE_ORDER(dds_stream_write_uni)
#define dds_stream_writeBO                            NAME_BYTE_ORDER(dds_stream_write)
#define dds_stream_write_implBO                       NAME_BYTE_ORDER(dds_stream_write_impl)
#define dds_stream_write_adrBO                        NAME_BYTE_ORDER(dds_stream_write_adr)
#define dds_stream_write_plBO                         NAME_BYTE_ORDER(dds_stream_write_pl)
#define dds_stream_write_pl_memberlistBO              NAME_BYTE_ORDER(dds_stream_write_pl_memberlist)
#define dds_stream_write_pl_memberBO                  NAME_BYTE_ORDER(dds_stream_write_pl_member)
#define dds_stream_write_delimitedBO                  NAME_BYTE_ORDER(dds_stream_write_delimited)
#define dds_stream_write_keyBO                        NAME_BYTE_ORDER(dds_stream_write_key)
#define dds_stream_write_keyBO_impl                   NAME2_BYTE_ORDER(dds_stream_write_key, _impl)
#define dds_cdr_alignto_clear_and_resizeBO            NAME_BYTE_ORDER(dds_cdr_alignto_clear_and_resize)
#define dds_stream_swap_if_needed_insituBO            NAME_BYTE_ORDER(dds_stream_swap_if_needed_insitu)
#define dds_stream_to_BO_insitu                       NAME2_BYTE_ORDER(dds_stream_to_, _insitu)
#define dds_stream_extract_keyBO_from_data            NAME2_BYTE_ORDER(dds_stream_extract_key, _from_data)
#define dds_stream_extract_keyBO_from_data1           NAME2_BYTE_ORDER(dds_stream_extract_key, _from_data1)
#define dds_stream_extract_keyBO_from_data_adr        NAME2_BYTE_ORDER(dds_stream_extract_key, _from_data_adr)
#define dds_stream_extract_keyBO_from_key_prim_op     NAME2_BYTE_ORDER(dds_stream_extract_key, _from_key_prim_op)
#define dds_stream_extract_keyBO_from_data_delimited  NAME2_BYTE_ORDER(dds_stream_extract_key, _from_data_delimited)
#define dds_stream_extract_keyBO_from_data_pl         NAME2_BYTE_ORDER(dds_stream_extract_key, _from_data_pl)
#define dds_stream_extract_keyBO_from_data_pl_member  NAME2_BYTE_ORDER(dds_stream_extract_key, _from_data_pl_member)
#define dds_stream_extract_keyBO_from_key             NAME2_BYTE_ORDER(dds_stream_extract_key, _from_key)

// Type used by dds_cdrstream_keys.part.c to temporarily store key field positions in CDR
// and the instructions needed for handling it
struct key_off_info {
  uint32_t src_off;
  const uint32_t *op_off;
};

static const uint32_t *dds_stream_skip_adr (uint32_t insn, const uint32_t * __restrict ops);
static const uint32_t *dds_stream_skip_default (char * __restrict data, const struct dds_cdrstream_allocator * __restrict allocator, const uint32_t * __restrict ops);
static const uint32_t *dds_stream_extract_key_from_data1 (dds_istream_t * __restrict is, dds_ostream_t * __restrict os, const struct dds_cdrstream_allocator * __restrict allocator,
  uint32_t ops_offs_idx, uint32_t * __restrict ops_offs, const uint32_t * const __restrict op0, const uint32_t * const __restrict op0_type, const uint32_t * __restrict ops, bool mutable_member, bool mutable_member_or_parent,
  uint32_t n_keys, uint32_t * __restrict keys_remaining, const dds_cdrstream_desc_key_t * __restrict key, struct key_off_info * __restrict key_offs);
static const uint32_t *dds_stream_extract_keyBE_from_data1 (dds_istream_t * __restrict is, dds_ostreamBE_t * __restrict os, const struct dds_cdrstream_allocator * __restrict allocator,
  uint32_t ops_offs_idx, uint32_t * __restrict ops_offs, const uint32_t * const __restrict op0, const uint32_t * const __restrict op0_type, const uint32_t * __restrict ops, bool mutable_member, bool mutable_member_or_parent,
  uint32_t n_keys, uint32_t * __restrict keys_remaining, const dds_cdrstream_desc_key_t * __restrict key, struct key_off_info * __restrict key_offs);
static const uint32_t *stream_normalize_data_impl (char * __restrict data, uint32_t * __restrict off, uint32_t size, bool bswap, uint32_t xcdr_version, const uint32_t * __restrict ops, bool is_mutable_member) ddsrt_attribute_warn_unused_result ddsrt_nonnull_all;
static const uint32_t *dds_stream_read_impl (dds_istream_t * __restrict is, char * __restrict data, const struct dds_cdrstream_allocator * __restrict allocator, const uint32_t * __restrict ops, bool is_mutable_member);
static const uint32_t *stream_free_sample_adr (uint32_t insn, void * __restrict data, const struct dds_cdrstream_allocator * __restrict allocator, const uint32_t * __restrict ops);

#ifndef NDEBUG
typedef struct align { uint32_t a; } align_t;
#define ALIGN(n) ((n).a)
#else
typedef uint32_t align_t;
#define ALIGN(n) (n)
#endif

static inline align_t dds_cdr_get_align (uint32_t xcdr_version, uint32_t size)
{
#ifndef NDEBUG
#define MK_ALIGN(n) (struct align){(n)}
#else
#define MK_ALIGN(n) (n)
#endif
  if (size > 4)
    return xcdr_version == DDSI_RTPS_CDR_ENC_VERSION_2 ? MK_ALIGN(4) : MK_ALIGN(8);
  return MK_ALIGN(size);
#undef MK_ALIGN
}

static void dds_ostream_grow (dds_ostream_t * __restrict os, const struct dds_cdrstream_allocator * __restrict allocator, uint32_t size)
{
  uint32_t needed = size + os->m_index;

  /* Reallocate on 4k boundry */

  uint32_t new_size = (needed & ~(uint32_t)0xfff) + 0x1000;
  uint8_t *old = os->m_buffer;

  os->m_buffer = allocator->realloc (old, new_size);
  os->m_size = new_size;
}

dds_ostream_t dds_ostream_from_buffer(void *buffer, size_t size, uint16_t write_encoding_version)
{
  dds_ostream_t os;
  os.m_buffer = buffer;
  os.m_size = (uint32_t) size;
  os.m_index = 0;
  os.m_xcdr_version = write_encoding_version;
  return os;
}

static void dds_cdr_resize (dds_ostream_t * __restrict os, const struct dds_cdrstream_allocator * __restrict allocator, uint32_t l)
{
  if (os->m_size < l + os->m_index)
    dds_ostream_grow (os, allocator, l);
}

void dds_istream_init (dds_istream_t * __restrict is, uint32_t size, const void * __restrict input, uint32_t xcdr_version)
{
  is->m_buffer = input;
  is->m_size = size;
  is->m_index = 0;
  is->m_xcdr_version = xcdr_version;
}

void dds_ostream_init (dds_ostream_t * __restrict os, const struct dds_cdrstream_allocator * __restrict allocator, uint32_t size, uint32_t xcdr_version)
{
  os->m_buffer = NULL;
  os->m_size = 0;
  os->m_index = 0;
  os->m_xcdr_version = xcdr_version;
  dds_cdr_resize (os, allocator, size);
}

void dds_ostreamLE_init (dds_ostreamLE_t * __restrict os, const struct dds_cdrstream_allocator * __restrict allocator, uint32_t size, uint32_t xcdr_version)
{
  dds_ostream_init (&os->x, allocator, size, xcdr_version);
}

void dds_ostreamBE_init (dds_ostreamBE_t * __restrict os, const struct dds_cdrstream_allocator * __restrict allocator, uint32_t size, uint32_t xcdr_version)
{
  dds_ostream_init (&os->x, allocator, size, xcdr_version);
}

void dds_istream_fini (dds_istream_t * __restrict is)
{
  (void) is;
}

void dds_ostream_fini (dds_ostream_t * __restrict os, const struct dds_cdrstream_allocator * __restrict allocator)
{
  if (os->m_size)
    allocator->free (os->m_buffer);
}

void dds_ostreamLE_fini (dds_ostreamLE_t * __restrict os, const struct dds_cdrstream_allocator * __restrict allocator)
{
  dds_ostream_fini (&os->x, allocator);
}

void dds_ostreamBE_fini (dds_ostreamBE_t * __restrict os, const struct dds_cdrstream_allocator * __restrict allocator)
{
  dds_ostream_fini (&os->x, allocator);
}

static void dds_cdr_alignto (dds_istream_t * __restrict is, align_t a)
{
  is->m_index = (is->m_index + ALIGN(a) - 1) & ~(ALIGN(a) - 1);
  assert (is->m_index < is->m_size);
}

static uint32_t dds_cdr_alignto_clear_and_resize (dds_ostream_t * __restrict os, const struct dds_cdrstream_allocator * __restrict allocator, align_t a, uint32_t extra)
{
  const uint32_t m = os->m_index % ALIGN(a);
  if (m == 0)
  {
    dds_cdr_resize (os, allocator, extra);
    return 0;
  }
  else
  {
    const uint32_t pad = ALIGN(a) - m;
    dds_cdr_resize (os, allocator, pad + extra);
    for (uint32_t i = 0; i < pad; i++)
      os->m_buffer[os->m_index++] = 0;
    return pad;
  }
}

static uint32_t dds_cdr_alignto_clear_and_resizeBE (dds_ostreamBE_t * __restrict os, const struct dds_cdrstream_allocator * __restrict allocator, align_t a, uint32_t extra)
{
  return dds_cdr_alignto_clear_and_resize (&os->x, allocator, a, extra);
}

uint32_t dds_cdr_alignto4_clear_and_resize (dds_ostream_t * __restrict os, const struct dds_cdrstream_allocator * __restrict allocator, uint32_t xcdr_version)
{
  return dds_cdr_alignto_clear_and_resize (os, allocator, dds_cdr_get_align (xcdr_version, 4), 0);
}

static uint8_t dds_is_get1 (dds_istream_t * __restrict is)
{
  assert (is->m_index < is->m_size);
  uint8_t v = *(is->m_buffer + is->m_index);
  is->m_index++;
  return v;
}

static uint16_t dds_is_get2 (dds_istream_t * __restrict is)
{
  dds_cdr_alignto (is, dds_cdr_get_align (is->m_xcdr_version, 2));
  uint16_t v = * ((uint16_t *) (is->m_buffer + is->m_index));
  is->m_index += 2;
  return v;
}

static uint32_t dds_is_get4 (dds_istream_t * __restrict is)
{
  dds_cdr_alignto (is, dds_cdr_get_align (is->m_xcdr_version, 4));
  uint32_t v = * ((uint32_t *) (is->m_buffer + is->m_index));
  is->m_index += 4;
  return v;
}

static uint32_t dds_is_peek4 (dds_istream_t * __restrict is)
{
  dds_cdr_alignto (is, dds_cdr_get_align (is->m_xcdr_version, 4));
  uint32_t v = * ((uint32_t *) (is->m_buffer + is->m_index));
  return v;
}

static uint64_t dds_is_get8 (dds_istream_t * __restrict is)
{
  dds_cdr_alignto (is, dds_cdr_get_align (is->m_xcdr_version, 8));
  size_t off_low = (DDSRT_ENDIAN == DDSRT_LITTLE_ENDIAN) ? 0 : 4, off_high = 4 - off_low;
  uint32_t v_low = * ((uint32_t *) (is->m_buffer + is->m_index + off_low)),
    v_high = * ((uint32_t *) (is->m_buffer + is->m_index + off_high));
  uint64_t v = (uint64_t) v_high << 32 | v_low;
  is->m_index += 8;
  return v;
}

static void dds_is_get_bytes (dds_istream_t * __restrict is, void * __restrict b, uint32_t num, uint32_t elem_size)
{
  dds_cdr_alignto (is, dds_cdr_get_align (is->m_xcdr_version, elem_size));
  memcpy (b, is->m_buffer + is->m_index, num * elem_size);
  is->m_index += num * elem_size;
}

static void dds_os_put1 (dds_ostream_t * __restrict os, const struct dds_cdrstream_allocator * __restrict allocator, uint8_t v)
{
  dds_cdr_resize (os, allocator, 1);
  *((uint8_t *) (os->m_buffer + os->m_index)) = v;
  os->m_index += 1;
}

static void dds_os_put2 (dds_ostream_t * __restrict os, const struct dds_cdrstream_allocator * __restrict allocator, uint16_t v)
{
  dds_cdr_alignto_clear_and_resize (os, allocator, dds_cdr_get_align (os->m_xcdr_version, 2), 2);
  *((uint16_t *) (os->m_buffer + os->m_index)) = v;
  os->m_index += 2;
}

static void dds_os_put4 (dds_ostream_t * __restrict os, const struct dds_cdrstream_allocator * __restrict allocator, uint32_t v)
{
  dds_cdr_alignto_clear_and_resize (os, allocator, dds_cdr_get_align (os->m_xcdr_version, 4), 4);
  *((uint32_t *) (os->m_buffer + os->m_index)) = v;
  os->m_index += 4;
}

static void dds_os_put8 (dds_ostream_t * __restrict os, const struct dds_cdrstream_allocator * __restrict allocator, uint64_t v)
{
  dds_cdr_alignto_clear_and_resize (os, allocator, dds_cdr_get_align (os->m_xcdr_version, 8), 8);
  size_t off_low = (DDSRT_ENDIAN == DDSRT_LITTLE_ENDIAN) ? 0 : 4, off_high = 4 - off_low;
  *((uint32_t *) (os->m_buffer + os->m_index + off_low)) = (uint32_t) v;
  *((uint32_t *) (os->m_buffer + os->m_index + off_high)) = (uint32_t) (v >> 32);
  os->m_index += 8;
}

static uint32_t dds_os_reserve4 (dds_ostream_t * __restrict os, const struct dds_cdrstream_allocator * __restrict allocator)
{
  dds_cdr_alignto_clear_and_resize (os, allocator, dds_cdr_get_align (os->m_xcdr_version, 4), 4);
  os->m_index += 4;
  return os->m_index;
}

static uint32_t dds_os_reserve8 (dds_ostream_t * __restrict os, const struct dds_cdrstream_allocator * __restrict allocator)
{
  dds_cdr_alignto_clear_and_resize (os, allocator, dds_cdr_get_align (os->m_xcdr_version, 8), 8);
  os->m_index += 8;
  return os->m_index;
}

static void dds_os_put1LE (dds_ostreamLE_t * __restrict os, const struct dds_cdrstream_allocator * __restrict allocator, uint8_t v)  { dds_os_put1 (&os->x, allocator, v); }
static void dds_os_put2LE (dds_ostreamLE_t * __restrict os, const struct dds_cdrstream_allocator * __restrict allocator, uint16_t v) { dds_os_put2 (&os->x, allocator, ddsrt_toLE2u (v)); }
static void dds_os_put4LE (dds_ostreamLE_t * __restrict os, const struct dds_cdrstream_allocator * __restrict allocator, uint32_t v) { dds_os_put4 (&os->x, allocator, ddsrt_toLE4u (v)); }
static void dds_os_put8LE (dds_ostreamLE_t * __restrict os, const struct dds_cdrstream_allocator * __restrict allocator, uint64_t v) { dds_os_put8 (&os->x, allocator, ddsrt_toLE8u (v)); }
static uint32_t dds_os_reserve4LE (dds_ostreamLE_t * __restrict os, const struct dds_cdrstream_allocator * __restrict allocator) { return dds_os_reserve4 (&os->x, allocator); }
static uint32_t dds_os_reserve8LE (dds_ostreamLE_t * __restrict os, const struct dds_cdrstream_allocator * __restrict allocator) { return dds_os_reserve8 (&os->x, allocator); }

static void dds_os_put1BE (dds_ostreamBE_t * __restrict os, const struct dds_cdrstream_allocator * __restrict allocator, uint8_t v)  { dds_os_put1 (&os->x, allocator, v); }
static void dds_os_put2BE (dds_ostreamBE_t * __restrict os, const struct dds_cdrstream_allocator * __restrict allocator, uint16_t v) { dds_os_put2 (&os->x, allocator, ddsrt_toBE2u (v)); }
static void dds_os_put4BE (dds_ostreamBE_t * __restrict os, const struct dds_cdrstream_allocator * __restrict allocator, uint32_t v) { dds_os_put4 (&os->x, allocator, ddsrt_toBE4u (v)); }
static void dds_os_put8BE (dds_ostreamBE_t * __restrict os, const struct dds_cdrstream_allocator * __restrict allocator, uint64_t v) { dds_os_put8 (&os->x, allocator, ddsrt_toBE8u (v)); }
static uint32_t dds_os_reserve4BE (dds_ostreamBE_t * __restrict os, const struct dds_cdrstream_allocator * __restrict allocator) { return dds_os_reserve4 (&os->x, allocator); }
static uint32_t dds_os_reserve8BE (dds_ostreamBE_t * __restrict os, const struct dds_cdrstream_allocator * __restrict allocator) { return dds_os_reserve8 (&os->x, allocator); }

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
      uint32_t *buf = vbuf;
      // max size of sample is 4GB or thereabouts, so a 64-bit int or double
      // array or sequence can never have more than 0.5G elements
      //
      // need to byte-swap using 32-bit elements because of XCDR2 droping the
      // natural alignment requirement
      for (uint32_t i = 0; i < num; i++) {
        uint32_t a = ddsrt_bswap4u (buf[2*i]);
        uint32_t b = ddsrt_bswap4u (buf[2*i+1]);
        buf[2*i] = b;
        buf[2*i+1] = a;
      }
      break;
    }
  }
}

static void dds_os_put_bytes (dds_ostream_t * __restrict os, const struct dds_cdrstream_allocator * __restrict allocator, const void * __restrict b, uint32_t l)
{
  dds_cdr_resize (os, allocator, l);
  memcpy (os->m_buffer + os->m_index, b, l);
  os->m_index += l;
}

static void dds_os_put_bytes_aligned (dds_ostream_t * __restrict os, const struct dds_cdrstream_allocator * __restrict allocator, const void * __restrict data, uint32_t num, uint32_t elem_sz, align_t align, void **dst)
{
  const uint32_t sz = num * elem_sz;
  dds_cdr_alignto_clear_and_resize (os, allocator, align, sz);
  if (dst)
    *dst = os->m_buffer + os->m_index;
  memcpy (os->m_buffer + os->m_index, data, sz);
  os->m_index += sz;
}

static inline bool is_primitive_type (enum dds_stream_typecode type)
{
  return type <= DDS_OP_VAL_8BY || type == DDS_OP_VAL_BLN;
}

#ifndef NDEBUG
static inline bool is_primitive_or_enum_type (enum dds_stream_typecode type)
{
  return is_primitive_type (type) || type == DDS_OP_VAL_ENU;
}
#endif

static inline bool is_dheader_needed (enum dds_stream_typecode type, uint32_t xcdrv)
{
  return !is_primitive_type (type) && xcdrv == DDSI_RTPS_CDR_ENC_VERSION_2;
}

static uint32_t get_primitive_size (enum dds_stream_typecode type)
{
  DDSRT_STATIC_ASSERT (DDS_OP_VAL_1BY == 1 && DDS_OP_VAL_2BY == 2 && DDS_OP_VAL_4BY == 3 && DDS_OP_VAL_8BY == 4);
  assert (is_primitive_type (type));
  return type == DDS_OP_VAL_BLN ? 1 : (uint32_t)1 << ((uint32_t) type - 1);
}

static uint32_t get_collection_elem_size (uint32_t insn, const uint32_t * __restrict ops)
{
  switch (DDS_OP_SUBTYPE (insn))
  {
    case DDS_OP_VAL_BLN: case DDS_OP_VAL_1BY: case DDS_OP_VAL_2BY: case DDS_OP_VAL_4BY: case DDS_OP_VAL_8BY:
      return get_primitive_size (DDS_OP_SUBTYPE (insn));
    case DDS_OP_VAL_ENU:
      return sizeof (uint32_t);
    case DDS_OP_VAL_BMK:
      return DDS_OP_TYPE_SZ (insn);
    case DDS_OP_VAL_STR:
      return sizeof (char *);
    case DDS_OP_VAL_BST: case DDS_OP_VAL_SEQ: case DDS_OP_VAL_BSQ: case DDS_OP_VAL_ARR: case DDS_OP_VAL_UNI: case DDS_OP_VAL_STU:
      if (DDS_OP_TYPE (insn) == DDS_OP_VAL_ARR)
        return ops[4];
      break;
    case DDS_OP_VAL_EXT:
      break;
  }
  abort ();
}

static uint32_t get_adr_type_size (uint32_t insn, const uint32_t * __restrict ops)
{
  uint32_t sz = 0;
  switch (DDS_OP_TYPE (insn))
  {
    case DDS_OP_VAL_BLN: case DDS_OP_VAL_1BY: case DDS_OP_VAL_2BY: case DDS_OP_VAL_4BY: case DDS_OP_VAL_8BY:
      sz = get_primitive_size (DDS_OP_TYPE (insn));
      break;
    case DDS_OP_VAL_ENU:
      sz = sizeof (uint32_t);
      break;
    case DDS_OP_VAL_BMK:
      sz = DDS_OP_TYPE_SZ (insn);
      break;
    case DDS_OP_VAL_STR:
      sz = sizeof (char *);
      break;
    case DDS_OP_VAL_BST:
      sz = ops[2];
      break;
    case DDS_OP_VAL_ARR:
    {
      uint32_t num = ops[2];
      uint32_t elem_sz = get_collection_elem_size (ops[0], ops);
      sz = num * elem_sz;
      break;
    }
    case DDS_OP_VAL_SEQ:
    case DDS_OP_VAL_BSQ:
      /* external sequence member is a pointer to a dds_sequence_t, so element size and
         sequence length are not relevant for the allocation size for the member */
      sz = sizeof (struct dds_sequence);
      break;
    case DDS_OP_VAL_EXT:
      sz = ops[3];
      break;
    case DDS_OP_VAL_UNI:
    case DDS_OP_VAL_STU:
      /* for UNI and STU members are externally defined, so are using EXT type */
      abort ();
      break;
  }
  return sz;
}

static uint32_t get_jeq4_type_size (const enum dds_stream_typecode valtype, const uint32_t * __restrict jeq_op)
{
  uint32_t sz = 0;
  switch (valtype)
  {
    case DDS_OP_VAL_BLN: case DDS_OP_VAL_1BY: case DDS_OP_VAL_2BY: case DDS_OP_VAL_4BY: case DDS_OP_VAL_8BY:
      sz = get_primitive_size (valtype);
      break;
    case DDS_OP_VAL_ENU:
      sz = sizeof (uint32_t);
      break;
    case DDS_OP_VAL_STR:
      sz = sizeof (char *);
      break;
    case DDS_OP_VAL_BMK:
    case DDS_OP_VAL_BST:
    case DDS_OP_VAL_ARR: {
      const uint32_t *jsr_ops = jeq_op + DDS_OP_ADR_JSR (jeq_op[0]);
      sz = get_adr_type_size (jsr_ops[0], jsr_ops);
      break;
    }
    case DDS_OP_VAL_SEQ: case DDS_OP_VAL_BSQ: case DDS_OP_VAL_STU: case DDS_OP_VAL_UNI:
      sz = jeq_op[3];
      break;
    case DDS_OP_VAL_EXT:
      abort ();
      break;
  }
  return sz;
}

static bool type_has_subtype_or_members (enum dds_stream_typecode type)
{
  return type == DDS_OP_VAL_SEQ || type == DDS_OP_VAL_BSQ || type == DDS_OP_VAL_ARR || type == DDS_OP_VAL_UNI || type == DDS_OP_VAL_STU;
}

static bool seq_is_bounded (enum dds_stream_typecode type)
{
  assert (type == DDS_OP_VAL_SEQ || type == DDS_OP_VAL_BSQ);
  return type == DDS_OP_VAL_BSQ;
}

static inline bool bitmask_value_valid (uint64_t val, uint32_t bits_h, uint32_t bits_l)
{
  return (val >> 32 & ~bits_h) == 0 && ((uint32_t) val & ~bits_l) == 0;
}

static inline bool op_type_external (const uint32_t insn)
{
  uint32_t typeflags = DDS_OP_TYPE_FLAGS (insn);
  return (typeflags & DDS_OP_FLAG_EXT);
}

static inline bool op_type_optional (const uint32_t insn)
{
  uint32_t flags = DDS_OP_FLAGS (insn);
  return (flags & DDS_OP_FLAG_OPT);
}

static inline bool op_type_base (const uint32_t insn)
{
  uint32_t opflags = DDS_OP_FLAGS (insn);
  return (opflags & DDS_OP_FLAG_BASE);
}

static inline bool check_optimize_impl (uint32_t xcdr_version, const uint32_t *ops, uint32_t size, uint32_t num, uint32_t *off, uint32_t member_offs)
{
  align_t align = dds_cdr_get_align (xcdr_version, size);
  if (*off % ALIGN(align))
    *off += ALIGN(align) - (*off % ALIGN(align));
  if (member_offs + ops[1] != *off)
    return false;
  *off += num * size;
  return true;
}

static uint32_t dds_stream_check_optimize1 (const struct dds_cdrstream_desc * __restrict desc, uint32_t xcdr_version, const uint32_t *ops, uint32_t off, uint32_t member_offs)
{
  uint32_t insn;
  while ((insn = *ops) != DDS_OP_RTS)
  {
    if (DDS_OP (insn) != DDS_OP_ADR)
      return 0;

    if (op_type_external (insn))
      return 0;

    switch (DDS_OP_TYPE (insn))
    {
      case DDS_OP_VAL_BLN: case DDS_OP_VAL_1BY: case DDS_OP_VAL_2BY: case DDS_OP_VAL_4BY: case DDS_OP_VAL_8BY:
        if (!check_optimize_impl (xcdr_version, ops, get_primitive_size (DDS_OP_TYPE (insn)), 1, &off, member_offs))
          return 0;
        ops += 2;
        break;
      case DDS_OP_VAL_ENU:
        if (DDS_OP_TYPE_SZ (insn) != 4 || !check_optimize_impl (xcdr_version, ops, sizeof (uint32_t), 1, &off, member_offs))
          return 0;
        ops += 3;
        break;
      case DDS_OP_VAL_BMK:
        if (!check_optimize_impl (xcdr_version, ops, DDS_OP_TYPE_SZ (insn), 1, &off, member_offs))
          return 0;
        ops += 4;
        break;
      case DDS_OP_VAL_ARR:
        switch (DDS_OP_SUBTYPE (insn))
        {
          case DDS_OP_VAL_BLN: case DDS_OP_VAL_1BY: case DDS_OP_VAL_2BY: case DDS_OP_VAL_4BY: case DDS_OP_VAL_8BY:
            if (!check_optimize_impl (xcdr_version, ops, get_primitive_size (DDS_OP_SUBTYPE (insn)), ops[2], &off, member_offs))
              return 0;
            ops += 3;
            break;
          case DDS_OP_VAL_ENU:
            if (xcdr_version == DDSI_RTPS_CDR_ENC_VERSION_2) /* xcdr2 arrays have a dheader for non-primitive types */
              return 0;
            if (DDS_OP_TYPE_SZ (insn) != 4 || !check_optimize_impl (xcdr_version, ops, sizeof (uint32_t), ops[2], &off, member_offs))
              return 0;
            ops += 4;
            break;
          case DDS_OP_VAL_BMK:
            if (xcdr_version == DDSI_RTPS_CDR_ENC_VERSION_2) /* xcdr2 arrays have a dheader for non-primitive types */
              return 0;
            if (!check_optimize_impl (xcdr_version, ops, DDS_OP_TYPE_SZ (insn), ops[2], &off, member_offs))
              return 0;
            ops += 5;
            break;
          default:
            return 0;
        }
        break;
      case DDS_OP_VAL_EXT: {
        const uint32_t *jsr_ops = ops + DDS_OP_ADR_JSR (ops[2]);
        const uint32_t jmp = DDS_OP_ADR_JMP (ops[2]);
        if (DDS_OP_ADR_JSR (ops[2]) > 0)
          off = dds_stream_check_optimize1 (desc, xcdr_version, jsr_ops, off, member_offs + ops[1]);
        ops += jmp ? jmp : 3;
        break;
      }
      case DDS_OP_VAL_SEQ:
      case DDS_OP_VAL_BSQ:
      case DDS_OP_VAL_STR:
      case DDS_OP_VAL_BST:
      case DDS_OP_VAL_STU:
      case DDS_OP_VAL_UNI:
        return 0;
    }
  }
  return off;
#undef ALLOW_ENUM
}

size_t dds_stream_check_optimize (const struct dds_cdrstream_desc * __restrict desc, uint32_t xcdr_version)
{
  size_t opt_size = dds_stream_check_optimize1 (desc, xcdr_version, desc->ops.ops, 0, 0);
  // off < desc can occur if desc->size includes "trailing" padding
  assert (opt_size <= desc->size);
  return opt_size;
}

static void dds_stream_countops1 (const uint32_t * __restrict ops, const uint32_t **ops_end, uint16_t *min_xcdrv, uint32_t nestc, uint32_t *nestm);

static const uint32_t *dds_stream_countops_seq (const uint32_t * __restrict ops, uint32_t insn, const uint32_t **ops_end, uint16_t *min_xcdrv, uint32_t nestc, uint32_t *nestm)
{
  uint32_t bound_op = seq_is_bounded (DDS_OP_TYPE (insn)) ? 1 : 0;
  const enum dds_stream_typecode subtype = DDS_OP_SUBTYPE (insn);
  switch (subtype)
  {
    case DDS_OP_VAL_BLN: case DDS_OP_VAL_1BY: case DDS_OP_VAL_2BY: case DDS_OP_VAL_4BY: case DDS_OP_VAL_8BY:
    case DDS_OP_VAL_STR:
      ops += 2 + bound_op;
      break;
    case DDS_OP_VAL_BST: case DDS_OP_VAL_ENU:
      ops += 3 + bound_op;
      break;
    case DDS_OP_VAL_BMK:
      ops += 4 + bound_op;
      break;
    case DDS_OP_VAL_SEQ: case DDS_OP_VAL_BSQ: case DDS_OP_VAL_ARR: case DDS_OP_VAL_UNI: case DDS_OP_VAL_STU: {
      const uint32_t jmp = DDS_OP_ADR_JMP (ops[3 + bound_op]);
      uint32_t const * const jsr_ops = ops + DDS_OP_ADR_JSR (ops[3 + bound_op]);
      if (ops + 4 + bound_op > *ops_end)
        *ops_end = ops + 4 + bound_op;
      if (DDS_OP_ADR_JSR (ops[3 + bound_op]) > 0)
        dds_stream_countops1 (jsr_ops, ops_end, min_xcdrv, nestc + (subtype == DDS_OP_VAL_UNI || subtype == DDS_OP_VAL_STU ? 1 : 0), nestm);
      ops += (jmp ? jmp : (4 + bound_op)); /* FIXME: why would jmp be 0? */
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

static const uint32_t *dds_stream_countops_arr (const uint32_t * __restrict ops, uint32_t insn, const uint32_t **ops_end, uint16_t *min_xcdrv, uint32_t nestc, uint32_t *nestm)
{
  const enum dds_stream_typecode subtype = DDS_OP_SUBTYPE (insn);
  switch (subtype)
  {
    case DDS_OP_VAL_BLN: case DDS_OP_VAL_1BY: case DDS_OP_VAL_2BY: case DDS_OP_VAL_4BY: case DDS_OP_VAL_8BY:
    case DDS_OP_VAL_STR:
      ops += 3;
      break;
    case DDS_OP_VAL_ENU:
      ops += 4;
      break;
    case DDS_OP_VAL_BST: case DDS_OP_VAL_BMK:
      ops += 5;
      break;
    case DDS_OP_VAL_SEQ: case DDS_OP_VAL_BSQ: case DDS_OP_VAL_ARR: case DDS_OP_VAL_UNI: case DDS_OP_VAL_STU: {
      const uint32_t jmp = DDS_OP_ADR_JMP (ops[3]);
      const uint32_t *jsr_ops = ops + DDS_OP_ADR_JSR (ops[3]);
      if (ops + 5 > *ops_end)
        *ops_end = ops + 5;
      if (DDS_OP_ADR_JSR (ops[3]) > 0)
        dds_stream_countops1 (jsr_ops, ops_end, min_xcdrv, nestc + (subtype == DDS_OP_VAL_UNI || subtype == DDS_OP_VAL_STU ? 1 : 0), nestm);
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

static const uint32_t *dds_stream_countops_uni (const uint32_t * __restrict ops, const uint32_t **ops_end, uint16_t *min_xcdrv, uint32_t nestc, uint32_t *nestm)
{
  const uint32_t numcases = ops[2];
  const uint32_t *jeq_op = ops + DDS_OP_ADR_JSR (ops[3]);
  for (uint32_t i = 0; i < numcases; i++)
  {
    const enum dds_stream_typecode valtype = DDS_JEQ_TYPE (jeq_op[0]);
    switch (valtype)
    {
      case DDS_OP_VAL_BLN: case DDS_OP_VAL_1BY: case DDS_OP_VAL_2BY: case DDS_OP_VAL_4BY: case DDS_OP_VAL_8BY:
      case DDS_OP_VAL_STR: case DDS_OP_VAL_ENU:
        break;
      case DDS_OP_VAL_BST: case DDS_OP_VAL_SEQ: case DDS_OP_VAL_BSQ: case DDS_OP_VAL_ARR: case DDS_OP_VAL_UNI: case DDS_OP_VAL_STU: case DDS_OP_VAL_BMK:
        if (DDS_OP_ADR_JSR (jeq_op[0]) > 0)
          dds_stream_countops1 (jeq_op + DDS_OP_ADR_JSR (jeq_op[0]), ops_end, min_xcdrv, nestc + (valtype == DDS_OP_VAL_UNI || valtype == DDS_OP_VAL_STU ? 1 : 0), nestm);
        break;
      case DDS_OP_VAL_EXT:
        abort (); // not allowed
        break;
    }
    jeq_op += (DDS_OP (jeq_op[0]) == DDS_OP_JEQ) ? 3 : 4;
  }
  if (jeq_op > *ops_end)
    *ops_end = jeq_op;
  ops += DDS_OP_ADR_JMP (ops[3]);
  if (ops > *ops_end)
    *ops_end = ops;
  return ops;
}

static const uint32_t *dds_stream_countops_pl (const uint32_t * __restrict ops, const uint32_t **ops_end, uint16_t *min_xcdrv, uint32_t nestc, uint32_t *nestm)
{
  uint32_t insn;
  assert (ops[0] == DDS_OP_PLC);
  ops++; /* skip PLC op */
  while ((insn = *ops) != DDS_OP_RTS)
  {
    switch (DDS_OP (insn))
    {
      case DDS_OP_PLM: {
        uint32_t flags = DDS_PLM_FLAGS (insn);
        const uint32_t *plm_ops = ops + DDS_OP_ADR_PLM (insn);
        if (flags & DDS_OP_FLAG_BASE)
          (void) dds_stream_countops_pl (plm_ops, ops_end, min_xcdrv, nestc, nestm);
        else
          dds_stream_countops1 (plm_ops, ops_end, min_xcdrv, nestc, nestm);
        ops += 2;
        break;
      }
      default:
        abort (); /* only list of (PLM, member-id) supported */
        break;
    }
  }
  if (ops > *ops_end)
    *ops_end = ops;
  return ops;
}

static void dds_stream_countops1 (const uint32_t * __restrict ops, const uint32_t **ops_end, uint16_t *min_xcdrv, uint32_t nestc, uint32_t *nestm)
{
  uint32_t insn;
  if (nestm && *nestm < nestc)
    *nestm = nestc;
  while ((insn = *ops) != DDS_OP_RTS)
  {
    switch (DDS_OP (insn))
    {
      case DDS_OP_ADR: {
        if (op_type_optional (insn) && min_xcdrv)
          *min_xcdrv = DDSI_RTPS_CDR_ENC_VERSION_2;
        switch (DDS_OP_TYPE (insn))
        {
          case DDS_OP_VAL_BLN: case DDS_OP_VAL_1BY: case DDS_OP_VAL_2BY: case DDS_OP_VAL_4BY: case DDS_OP_VAL_8BY: case DDS_OP_VAL_STR:
            ops += 2;
            break;
          case DDS_OP_VAL_BST: case DDS_OP_VAL_ENU:
            ops += 3;
            break;
          case DDS_OP_VAL_BMK:
            ops += 4;
            break;
          case DDS_OP_VAL_SEQ: case DDS_OP_VAL_BSQ: ops = dds_stream_countops_seq (ops, insn, ops_end, min_xcdrv, nestc, nestm); break;
          case DDS_OP_VAL_ARR: ops = dds_stream_countops_arr (ops, insn, ops_end, min_xcdrv, nestc, nestm); break;
          case DDS_OP_VAL_UNI: ops = dds_stream_countops_uni (ops, ops_end, min_xcdrv, nestc, nestm); break;
          case DDS_OP_VAL_EXT: {
            const uint32_t *jsr_ops = ops + DDS_OP_ADR_JSR (ops[2]);
            const uint32_t jmp = DDS_OP_ADR_JMP (ops[2]);
            if (DDS_OP_ADR_JSR (ops[2]) > 0)
              dds_stream_countops1 (jsr_ops, ops_end, min_xcdrv, nestc + 1, nestm);
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
          dds_stream_countops1 (ops + DDS_OP_JUMP (insn), ops_end, min_xcdrv, nestc, nestm);
        ops++;
        break;
      }
      case DDS_OP_RTS: case DDS_OP_JEQ: case DDS_OP_JEQ4: case DDS_OP_KOF: case DDS_OP_PLM: {
        abort ();
        break;
      }
      case DDS_OP_DLC: {
        if (min_xcdrv)
          *min_xcdrv = DDSI_RTPS_CDR_ENC_VERSION_2;
        ops++;
        break;
      }
      case DDS_OP_PLC: {
        if (min_xcdrv)
          *min_xcdrv = DDSI_RTPS_CDR_ENC_VERSION_2;
        ops = dds_stream_countops_pl (ops, ops_end, min_xcdrv, nestc, nestm);
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
  if (key->m_offset >= (uint32_t) (*ops_end - ops))
  {
    assert (DDS_OP (ops[key->m_offset]) == DDS_OP_KOF);
    *ops_end = ops + key->m_offset + 1 + DDS_OP_LENGTH (ops[key->m_offset]);
  }
}

uint32_t dds_stream_countops (const uint32_t * __restrict ops, uint32_t nkeys, const dds_key_descriptor_t * __restrict keys)
{
  const uint32_t *ops_end = ops;
  dds_stream_countops1 (ops, &ops_end, NULL, 0, NULL);
  for (uint32_t n = 0; n < nkeys; n++)
    dds_stream_countops_keyoffset (ops, &keys[n], &ops_end);
  return (uint32_t) (ops_end - ops);
}

static char *dds_stream_reuse_string_bound (dds_istream_t * __restrict is, char * __restrict str, const struct dds_cdrstream_allocator * __restrict allocator, const uint32_t size, bool alloc)
{
  const uint32_t length = dds_is_get4 (is);
  const void *src = is->m_buffer + is->m_index;
  /* FIXME: validation now rejects data containing an oversize bounded string,
     so this check is superfluous, but perhaps rejecting such a sample is the
     wrong thing to do */
  if (!alloc)
    assert (str != NULL);
  else if (str == NULL)
    str = allocator->malloc (size);
  memcpy (str, src, length > size ? size : length);
  if (length > size)
    str[size - 1] = '\0';
  is->m_index += length;
  return str;
}

static char *dds_stream_reuse_string (dds_istream_t * __restrict is, char * __restrict str, const struct dds_cdrstream_allocator * __restrict allocator)
{
  const uint32_t length = dds_is_get4 (is);
  const void *src = is->m_buffer + is->m_index;
  if (str == NULL || strlen (str) + 1 < length)
    str = allocator->realloc (str, length);
  memcpy (str, src, length);
  is->m_index += length;
  return str;
}

static char *dds_stream_reuse_string_empty (char * __restrict str, const struct dds_cdrstream_allocator * __restrict allocator)
{
  if (str == NULL)
    str = allocator->realloc (str, 1);
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
            || (DDS_OP_TYPE (insn) == DDS_OP_VAL_ARR && (is_primitive_or_enum_type (DDS_OP_SUBTYPE (insn)) || DDS_OP_SUBTYPE (insn) == DDS_OP_VAL_BMK)) // allow prim-array, enum-array and bitmask-array as key
            || DDS_OP_TYPE (insn) == DDS_OP_VAL_EXT // allow fields in nested structs as key
          ));
}
#endif

static uint32_t read_union_discriminant (dds_istream_t * __restrict is, uint32_t insn)
{
  enum dds_stream_typecode type = DDS_OP_SUBTYPE (insn);
  assert (is_primitive_or_enum_type (type));
  switch (type)
  {
    case DDS_OP_VAL_BLN: case DDS_OP_VAL_1BY: return dds_is_get1 (is);
    case DDS_OP_VAL_2BY: return dds_is_get2 (is);
    case DDS_OP_VAL_4BY: return dds_is_get4 (is);
    case DDS_OP_VAL_ENU:
      switch (DDS_OP_TYPE_SZ (insn))
      {
        case 1: return dds_is_get1 (is);
        case 2: return dds_is_get2 (is);
        case 4: return dds_is_get4 (is);
        default: abort ();
      }
      break;
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
    if (DDS_OP (jeq_op[idx]) == DDS_OP_JEQ)
      idx += 3;
    else
    {
      assert (DDS_OP (jeq_op[idx]) == DDS_OP_JEQ4);
      idx += 4;
    }
  }
#endif
  for (ci = 0; ci < numcases - (has_default ? 1 : 0); ci++)
  {
    if (jeq_op[1] == disc)
      return jeq_op;
    jeq_op += (DDS_OP (jeq_op[0]) == DDS_OP_JEQ) ? 3 : 4;
  }
  return (ci < numcases) ? jeq_op : NULL;
}

static const uint32_t *skip_sequence_insns (uint32_t insn, const uint32_t * __restrict ops)
{
  uint32_t bound_op = seq_is_bounded (DDS_OP_TYPE (insn)) ? 1 : 0;
  switch (DDS_OP_SUBTYPE (insn))
  {
    case DDS_OP_VAL_BLN: case DDS_OP_VAL_1BY: case DDS_OP_VAL_2BY: case DDS_OP_VAL_4BY: case DDS_OP_VAL_8BY:
    case DDS_OP_VAL_STR:
      return ops + 2 + bound_op;
    case DDS_OP_VAL_BST: case DDS_OP_VAL_ENU:
      return ops + 3 + bound_op;
    case DDS_OP_VAL_BMK:
      return ops + 4 + bound_op;
    case DDS_OP_VAL_SEQ: case DDS_OP_VAL_BSQ: case DDS_OP_VAL_ARR: case DDS_OP_VAL_UNI: case DDS_OP_VAL_STU: {
      const uint32_t jmp = DDS_OP_ADR_JMP (ops[3 + bound_op]);
      return ops + (jmp ? jmp : 4 + bound_op); /* FIXME: why would jmp be 0? */
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
    case DDS_OP_VAL_BLN: case DDS_OP_VAL_1BY: case DDS_OP_VAL_2BY: case DDS_OP_VAL_4BY: case DDS_OP_VAL_8BY:
    case DDS_OP_VAL_STR:
      return ops + 3;
    case DDS_OP_VAL_ENU:
      return ops + 4;
    case DDS_OP_VAL_BST: case DDS_OP_VAL_BMK:
      return ops + 5;
    case DDS_OP_VAL_SEQ: case DDS_OP_VAL_BSQ: case DDS_OP_VAL_ARR: case DDS_OP_VAL_UNI: case DDS_OP_VAL_STU: {
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

static const uint32_t *skip_array_default (uint32_t insn, char * __restrict data, const struct dds_cdrstream_allocator * __restrict allocator, const uint32_t * __restrict ops)
{
  const enum dds_stream_typecode subtype = DDS_OP_SUBTYPE (insn);
  const uint32_t num = ops[2];
  switch (subtype)
  {
    case DDS_OP_VAL_BLN: case DDS_OP_VAL_1BY: case DDS_OP_VAL_2BY: case DDS_OP_VAL_4BY: case DDS_OP_VAL_8BY: {
      const uint32_t elem_size = get_primitive_size (subtype);
      memset (data, 0, num * elem_size);
      return ops + 3;
    }
    case DDS_OP_VAL_ENU: case DDS_OP_VAL_BMK: {
      const uint32_t elem_size = DDS_OP_TYPE_SZ (insn);
      memset (data, 0, num * elem_size);
      return ops + 4 + (subtype == DDS_OP_VAL_BMK ? 1 : 0);
    }
    case DDS_OP_VAL_STR: {
      char **ptr = (char **) data;
      for (uint32_t i = 0; i < num; i++)
        ptr[i] = dds_stream_reuse_string_empty (*(char **) ptr[i], allocator);
      return ops + 3;
    }
    case DDS_OP_VAL_BST: {
      char *ptr = (char *) data;
      const uint32_t elem_size = ops[4];
      for (uint32_t i = 0; i < num; i++)
        ((char *) (ptr + i * elem_size))[0] = '\0';
      return ops + 5;
    }
    case DDS_OP_VAL_SEQ: case DDS_OP_VAL_BSQ: case DDS_OP_VAL_ARR: case DDS_OP_VAL_UNI: case DDS_OP_VAL_STU: {
      const uint32_t *jsr_ops = ops + DDS_OP_ADR_JSR (ops[3]);
      const uint32_t jmp = DDS_OP_ADR_JMP (ops[3]);
      const uint32_t elem_size = ops[4];
      for (uint32_t i = 0; i < num; i++)
        (void) dds_stream_skip_default (data + i * elem_size, allocator, jsr_ops);
      return ops + (jmp ? jmp : 5);
    }
    case DDS_OP_VAL_EXT: {
      abort (); /* not supported */
      break;
    }
  }
  return NULL;
}

static const uint32_t *skip_union_default (uint32_t insn, char * __restrict discaddr, char * __restrict baseaddr, const struct dds_cdrstream_allocator * __restrict allocator, const uint32_t * __restrict ops)
{
  switch (DDS_OP_SUBTYPE (insn))
  {
    case DDS_OP_VAL_BLN: case DDS_OP_VAL_1BY: *((uint8_t *) discaddr) = 0; break;
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
      case DDS_OP_VAL_BLN: case DDS_OP_VAL_1BY: *((uint8_t *) valaddr) = 0; break;
      case DDS_OP_VAL_2BY: *((uint16_t *) valaddr) = 0; break;
      case DDS_OP_VAL_4BY: case DDS_OP_VAL_ENU: *((uint32_t *) valaddr) = 0; break;
      case DDS_OP_VAL_8BY: *((uint64_t *) valaddr) = 0; break;
      case DDS_OP_VAL_STR: *(char **) valaddr = dds_stream_reuse_string_empty (*((char **) valaddr), allocator); break;
      case DDS_OP_VAL_BST: case DDS_OP_VAL_SEQ: case DDS_OP_VAL_BSQ: case DDS_OP_VAL_ARR: case DDS_OP_VAL_UNI: case DDS_OP_VAL_STU: case DDS_OP_VAL_BMK:
        (void) dds_stream_skip_default (valaddr, allocator, jeq_op + DDS_OP_ADR_JSR (jeq_op[0]));
        break;
      case DDS_OP_VAL_EXT: {
        abort (); /* not supported */
        break;
      }
    }
  }
  return ops;
}

static uint32_t get_length_code_seq (const enum dds_stream_typecode subtype)
{
  switch (subtype)
  {
    /* Sequence length can be used as byte length */
    case DDS_OP_VAL_BLN: case DDS_OP_VAL_1BY:
      return LENGTH_CODE_ALSO_NEXTINT;

    /* A sequence with primitive subtype does not include a DHEADER,
       only the seq length, so we have to include a NEXTINT */
    case DDS_OP_VAL_2BY:
      return LENGTH_CODE_NEXTINT;

    /* Sequence length (item count) is used to calculate byte length */
    case DDS_OP_VAL_4BY:
      return LENGTH_CODE_ALSO_NEXTINT4;
    case DDS_OP_VAL_8BY:
      return LENGTH_CODE_ALSO_NEXTINT8;

    /* Sequences with non-primitive subtype contain a DHEADER */
    case DDS_OP_VAL_STR: case DDS_OP_VAL_BST: case DDS_OP_VAL_ENU: case DDS_OP_VAL_BMK:
    case DDS_OP_VAL_SEQ: case DDS_OP_VAL_BSQ: case DDS_OP_VAL_ARR: case DDS_OP_VAL_UNI: case DDS_OP_VAL_STU:
      return LENGTH_CODE_ALSO_NEXTINT;

    /* not supported */
    case DDS_OP_VAL_EXT:
      abort ();
      break;
  }
  abort ();
}

static uint32_t get_length_code_arr (const enum dds_stream_typecode subtype)
{
  switch (subtype)
  {
    /* An array with primitive subtype does not include a DHEADER,
       so we have to include a NEXTINT */
    case DDS_OP_VAL_BLN: case DDS_OP_VAL_1BY: case DDS_OP_VAL_2BY: case DDS_OP_VAL_4BY: case DDS_OP_VAL_8BY:
      return LENGTH_CODE_NEXTINT;

    /* Arrays with non-primitive subtype contain a DHEADER */
    case DDS_OP_VAL_STR: case DDS_OP_VAL_BST: case DDS_OP_VAL_ENU: case DDS_OP_VAL_BMK:
    case DDS_OP_VAL_SEQ: case DDS_OP_VAL_BSQ: case DDS_OP_VAL_ARR: case DDS_OP_VAL_UNI: case DDS_OP_VAL_STU:
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
        case DDS_OP_VAL_BLN: case DDS_OP_VAL_1BY: return LENGTH_CODE_1B;
        case DDS_OP_VAL_2BY: return LENGTH_CODE_2B;
        case DDS_OP_VAL_4BY: return LENGTH_CODE_4B;
        case DDS_OP_VAL_8BY: return LENGTH_CODE_8B;
        case DDS_OP_VAL_ENU: case DDS_OP_VAL_BMK:
          switch (DDS_OP_TYPE_SZ (insn))
          {
            case 1: return LENGTH_CODE_1B;
            case 2: return LENGTH_CODE_2B;
            case 4: return LENGTH_CODE_4B;
            case 8: return LENGTH_CODE_8B;
          }
          break;
        case DDS_OP_VAL_STR: case DDS_OP_VAL_BST: return LENGTH_CODE_ALSO_NEXTINT; /* nextint overlaps with length from serialized string data */
        case DDS_OP_VAL_SEQ: case DDS_OP_VAL_BSQ: return get_length_code_seq (DDS_OP_SUBTYPE (insn));
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
    case DDS_OP_RTS: case DDS_OP_JEQ: case DDS_OP_JEQ4: case DDS_OP_KOF: case DDS_OP_PLM:
      abort ();
      break;
    case DDS_OP_DLC: case DDS_OP_PLC:
      /* members of (final/appendable/mutable) aggregated types are included using ADR | EXT */
      abort();
      break;
  }
  return 0;
}

static bool is_member_present (const char * __restrict data, const uint32_t * __restrict ops)
{
  uint32_t insn;
  while ((insn = *ops) != DDS_OP_RTS)
  {
    switch (DDS_OP (insn))
    {
      case DDS_OP_ADR: {
        if (op_type_optional (insn))
        {
          const void *addr = data + ops[1];
          addr = *(char **) addr; /* de-reference also for type STR */
          return addr != NULL;
        }
        /* assume non-optional members always present */
        return true;
      }
      case DDS_OP_JSR:
        return is_member_present (data, ops + DDS_OP_JUMP (insn));
      case DDS_OP_RTS: case DDS_OP_JEQ: case DDS_OP_JEQ4: case DDS_OP_KOF:
      case DDS_OP_DLC: case DDS_OP_PLC: case DDS_OP_PLM:
        abort ();
        break;
    }
  }
  abort ();
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
#include "dds_cdrstream_write.part.c"
#undef NAME_BYTE_ORDER_EXT

// Big-endian
#define NAME_BYTE_ORDER_EXT BE
#include "dds_cdrstream_write.part.c"
#undef NAME_BYTE_ORDER_EXT

// Map some write-native functions to their little-endian or big-endian equivalent
#if DDSRT_ENDIAN == DDSRT_LITTLE_ENDIAN

static inline void dds_stream_write_string (dds_ostream_t * __restrict os, const struct dds_cdrstream_allocator * __restrict allocator, const char * __restrict val)
{
  dds_stream_write_stringLE ((dds_ostreamLE_t *) os, allocator, val);
}

static inline bool dds_stream_write_enum_value (dds_ostream_t * __restrict os, const struct dds_cdrstream_allocator * __restrict allocator, uint32_t insn, uint32_t val, uint32_t max)
{
  return dds_stream_write_enum_valueLE ((dds_ostreamLE_t *) os, allocator, insn, val, max);
}

static inline bool dds_stream_write_enum_arr (dds_ostream_t * __restrict os, const struct dds_cdrstream_allocator * __restrict allocator, uint32_t insn, const uint32_t * __restrict addr, uint32_t num, uint32_t max)
{
  return dds_stream_write_enum_arrLE ((dds_ostreamLE_t *) os, allocator, insn, addr, num, max);
}

static inline bool dds_stream_write_bitmask_value (dds_ostream_t * __restrict os, const struct dds_cdrstream_allocator * __restrict allocator, uint32_t insn, const void * __restrict addr, uint32_t bits_h, uint32_t bits_l)
{
  return dds_stream_write_bitmask_valueLE ((dds_ostreamLE_t *) os, allocator, insn, addr, bits_h, bits_l);
}

static inline bool dds_stream_write_bitmask_arr (dds_ostream_t * __restrict os, const struct dds_cdrstream_allocator * __restrict allocator, uint32_t insn, const void * __restrict addr, uint32_t num, uint32_t bits_h, uint32_t bits_l)
{
  return dds_stream_write_bitmask_arrLE ((dds_ostreamLE_t *) os, allocator, insn, addr, num, bits_h, bits_l);
}

const uint32_t *dds_stream_write (dds_ostream_t * __restrict os, const struct dds_cdrstream_allocator * __restrict allocator, const char * __restrict data, const uint32_t * __restrict ops)
{
  return dds_stream_writeLE ((dds_ostreamLE_t *) os, allocator, data, ops);
}

bool dds_stream_write_sample (dds_ostream_t * __restrict os, const struct dds_cdrstream_allocator * __restrict allocator, const void * __restrict data, const struct dds_cdrstream_desc * __restrict desc)
{
  return dds_stream_write_sampleLE ((dds_ostreamLE_t *) os, allocator, data, desc);
}

bool dds_stream_write_sampleLE (dds_ostreamLE_t * __restrict os, const struct dds_cdrstream_allocator * __restrict allocator, const void * __restrict data, const struct dds_cdrstream_desc * __restrict desc)
{
  size_t opt_size = os->x.m_xcdr_version == DDSI_RTPS_CDR_ENC_VERSION_1 ? desc->opt_size_xcdr1 : desc->opt_size_xcdr2;
  if (opt_size && desc->align && (((struct dds_ostream *)os)->m_index % desc->align) == 0)
  {
    dds_os_put_bytes ((struct dds_ostream *)os, allocator, data, (uint32_t) opt_size);
    return true;
  }
  else
    return dds_stream_writeLE (os, allocator, data, desc->ops.ops) != NULL;
}

bool dds_stream_write_sampleBE (dds_ostreamBE_t * __restrict os, const struct dds_cdrstream_allocator * __restrict allocator, const void * __restrict data, const struct dds_cdrstream_desc * __restrict desc)
{
  return dds_stream_writeBE (os, allocator, data, desc->ops.ops) != NULL;
}

#else /* if DDSRT_ENDIAN == DDSRT_LITTLE_ENDIAN */

static inline void dds_stream_write_string (dds_ostream_t * __restrict os, const struct dds_cdrstream_allocator * __restrict allocator, const char * __restrict val)
{
  dds_stream_write_stringBE ((dds_ostreamBE_t *) os, allocator, val, allocator);
}

static inline bool dds_stream_write_enum_value (dds_ostream_t * __restrict os, const struct dds_cdrstream_allocator * __restrict allocator, uint32_t insn, uint32_t val, uint32_t max)
{
  return dds_stream_write_enum_valueBE ((dds_ostreamBE_t *) os, allocator, insn, val, max, allocator);
}

static inline bool dds_stream_write_enum_arr (dds_ostream_t * __restrict os, const struct dds_cdrstream_allocator * __restrict allocator, uint32_t insn, const uint32_t * __restrict addr, uint32_t num, uint32_t max)
{
  return dds_stream_write_enum_arrBE ((dds_ostreamBE_t *) os, allocator, insn, addr, num, max);
}

static inline bool dds_stream_write_bitmask_value (dds_ostream_t * __restrict os, const struct dds_cdrstream_allocator * __restrict allocator, uint32_t insn, const void * __restrict addr, uint32_t bits_h, uint32_t bits_l)
{
  return dds_stream_write_bitmask_valueBE ((dds_ostreamBE_t *) os, allocator, insn, addr, bits_h, bits_l);
}

static inline bool dds_stream_write_bitmask_arr (dds_ostream_t * __restrict os, const struct dds_cdrstream_allocator * __restrict allocator, uint32_t insn, const void * __restrict addr, uint32_t num, uint32_t bits_h, uint32_t bits_l)
{
  return dds_stream_write_bitmask_arrBE ((dds_ostreamBE_t *) os, allocator, insn, addr, num, bits_h, bits_l);
}

const uint32_t *dds_stream_write (dds_ostream_t * __restrict os, const struct dds_cdrstream_allocator * __restrict allocator, const char * __restrict data, const uint32_t * __restrict ops)
{
  return dds_stream_writeBE ((dds_ostreamBE_t *) os, allocator, data, ops);
}

bool dds_stream_write_sample (dds_ostream_t * __restrict os, const struct dds_cdrstream_allocator * __restrict allocator, const void * __restrict data, const struct dds_cdrstream_desc * __restrict desc)
{
  return dds_stream_write_sampleBE ((dds_ostreamBE_t *) os, allocator, data, desc);
}

bool dds_stream_write_sampleLE (dds_ostreamLE_t * __restrict os, const struct dds_cdrstream_allocator * __restrict allocator, const void * __restrict data, const struct dds_cdrstream_desc * __restrict desc)
{
  return dds_stream_writeLE (os, allocator, data, desc->ops.ops) != NULL;
}

bool dds_stream_write_sampleBE (dds_ostreamBE_t * __restrict os, const struct dds_cdrstream_allocator * __restrict allocator, const void * __restrict data, const struct dds_cdrstream_desc * __restrict desc)
{
  size_t opt_size = os->x.m_xcdr_version == DDSI_RTPS_CDR_ENC_VERSION_1 ? desc->opt_size_xcdr1 : desc->opt_size_xcdr2;
  if (opt_size && desc->align && (((struct dds_ostream *)os)->m_index % desc->align) == 0)
  {
    dds_os_put_bytes ((struct dds_ostream *)os, data, (uint32_t) opt_size);
    return true;
  }
  else
    return dds_stream_writeBE (os, allocator, data, desc->ops.ops) != NULL;
}

#endif /* if DDSRT_ENDIAN == DDSRT_LITTLE_ENDIAN */

const uint32_t * dds_stream_write_with_byte_order (dds_ostream_t * __restrict os, const struct dds_cdrstream_allocator * __restrict allocator, const char * __restrict data, const uint32_t * __restrict ops, enum ddsrt_byte_order_selector bo)
{
  if (bo == DDSRT_BOSEL_LE)
    return dds_stream_writeLE ((dds_ostreamLE_t *) os, allocator, data, ops);
  else if (bo == DDSRT_BOSEL_BE)
    return dds_stream_writeBE ((dds_ostreamBE_t *) os, allocator, data, ops);
  else
    return dds_stream_write (os, allocator, data, ops);
}

static void realloc_sequence_buffer_if_needed (dds_sequence_t * __restrict seq, const struct dds_cdrstream_allocator * __restrict allocator, uint32_t num, uint32_t elem_size, bool init)
{
  const uint32_t size = num * elem_size;

  /* maintain max sequence length (may not have been set by caller) */
  if (seq->_length > seq->_maximum)
    seq->_maximum = seq->_length;

  if (num > seq->_maximum && seq->_release)
  {
    seq->_buffer = allocator->realloc (seq->_buffer, size);
    if (init)
    {
      const uint32_t off = seq->_maximum * elem_size;
      memset (seq->_buffer + off, 0, size - off);
    }
    seq->_maximum = num;
  }
  else if (num > 0 && seq->_maximum == 0)
  {
    seq->_buffer = allocator->malloc (size);
    if (init)
      memset (seq->_buffer, 0, size);
    seq->_release = true;
    seq->_maximum = num;
  }
}

static bool stream_is_member_present (uint32_t insn, dds_istream_t * __restrict is, bool is_mutable_member)
{
  return !op_type_optional (insn) || is_mutable_member || dds_is_get1 (is);
}

static const uint32_t *dds_stream_read_seq (dds_istream_t * __restrict is, char * __restrict addr, const struct dds_cdrstream_allocator * __restrict allocator, const uint32_t * __restrict ops, uint32_t insn)
{
  dds_sequence_t * const seq = (dds_sequence_t *) addr;
  const enum dds_stream_typecode subtype = DDS_OP_SUBTYPE (insn);
  uint32_t bound_op = seq_is_bounded (DDS_OP_TYPE (insn)) ? 1 : 0;
  if (is_dheader_needed (subtype, is->m_xcdr_version))
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
    case DDS_OP_VAL_BLN: case DDS_OP_VAL_1BY: case DDS_OP_VAL_2BY: case DDS_OP_VAL_4BY: case DDS_OP_VAL_8BY: {
      const uint32_t elem_size = get_primitive_size (subtype);
      realloc_sequence_buffer_if_needed (seq, allocator, num, elem_size, false);
      seq->_length = (num <= seq->_maximum) ? num : seq->_maximum;
      dds_is_get_bytes (is, seq->_buffer, seq->_length, elem_size);
      if (seq->_length < num)
        dds_stream_skip_forward (is, num - seq->_length, elem_size);
      return ops + 2 + bound_op;
    }
    case DDS_OP_VAL_ENU: {
      const uint32_t elem_size = DDS_OP_TYPE_SZ (insn);
      realloc_sequence_buffer_if_needed (seq, allocator, num, 4, false);
      seq->_length = (num <= seq->_maximum) ? num : seq->_maximum;
      switch (elem_size)
      {
        case 1:
          for (uint32_t i = 0; i < seq->_length; i++)
            ((uint32_t *) seq->_buffer)[i] = dds_is_get1 (is);
          break;
        case 2:
          for (uint32_t i = 0; i < seq->_length; i++)
            ((uint32_t *) seq->_buffer)[i] = dds_is_get2 (is);
          break;
        case 4:
          dds_is_get_bytes (is, seq->_buffer, seq->_length, elem_size);
          break;
      }
      if (seq->_length < num)
        dds_stream_skip_forward (is, num - seq->_length, elem_size);
      return ops + 3 + bound_op;
    }
    case DDS_OP_VAL_BMK: {
      const uint32_t elem_size = DDS_OP_TYPE_SZ (insn);
      realloc_sequence_buffer_if_needed (seq, allocator, num, elem_size, false);
      seq->_length = (num <= seq->_maximum) ? num : seq->_maximum;
      dds_is_get_bytes (is, seq->_buffer, seq->_length, elem_size);
      if (seq->_length < num)
        dds_stream_skip_forward (is, num - seq->_length, elem_size);
      return ops + 4 + bound_op;
    }
    case DDS_OP_VAL_STR: {
      realloc_sequence_buffer_if_needed (seq, allocator, num, sizeof (char *), true);
      seq->_length = (num <= seq->_maximum) ? num : seq->_maximum;
      char **ptr = (char **) seq->_buffer;
      for (uint32_t i = 0; i < seq->_length; i++)
        ptr[i] = dds_stream_reuse_string (is, ptr[i], allocator);
      for (uint32_t i = seq->_length; i < num; i++)
        dds_stream_skip_string (is);
      return ops + 2 + bound_op;
    }
    case DDS_OP_VAL_BST: {
      const uint32_t elem_size = ops[2 + bound_op];
      realloc_sequence_buffer_if_needed (seq, allocator, num, elem_size, false);
      seq->_length = (num <= seq->_maximum) ? num : seq->_maximum;
      char *ptr = (char *) seq->_buffer;
      for (uint32_t i = 0; i < seq->_length; i++)
        (void) dds_stream_reuse_string_bound (is, ptr + i * elem_size, allocator, elem_size, false);
      for (uint32_t i = seq->_length; i < num; i++)
        dds_stream_skip_string (is);
      return ops + 3 + bound_op;
    }
    case DDS_OP_VAL_SEQ: case DDS_OP_VAL_BSQ: case DDS_OP_VAL_ARR: case DDS_OP_VAL_UNI: case DDS_OP_VAL_STU: {
      const uint32_t elem_size = ops[2 + bound_op];
      const uint32_t jmp = DDS_OP_ADR_JMP (ops[3 + bound_op]);
      uint32_t const * const jsr_ops = ops + DDS_OP_ADR_JSR (ops[3 + bound_op]);
      realloc_sequence_buffer_if_needed (seq, allocator, num, elem_size, true);
      seq->_length = (num <= seq->_maximum) ? num : seq->_maximum;
      char *ptr = (char *) seq->_buffer;
      for (uint32_t i = 0; i < num; i++)
        (void) dds_stream_read_impl (is, ptr + i * elem_size, allocator, jsr_ops, false);
      return ops + (jmp ? jmp : (4 + bound_op)); /* FIXME: why would jmp be 0? */
    }
    case DDS_OP_VAL_EXT: {
      abort (); /* not supported */
      break;
    }
  }
  return NULL;
}

static const uint32_t *dds_stream_read_arr (dds_istream_t * __restrict is, char * __restrict addr, const struct dds_cdrstream_allocator * __restrict allocator, const uint32_t * __restrict ops, uint32_t insn)
{
  const enum dds_stream_typecode subtype = DDS_OP_SUBTYPE (insn);
  if (is_dheader_needed (subtype, is->m_xcdr_version))
  {
    /* skip DHEADER */
    dds_is_get4 (is);
  }
  const uint32_t num = ops[2];
  switch (subtype)
  {
    case DDS_OP_VAL_BLN: case DDS_OP_VAL_1BY: case DDS_OP_VAL_2BY: case DDS_OP_VAL_4BY: case DDS_OP_VAL_8BY: {
      const uint32_t elem_size = get_primitive_size (subtype);
      dds_is_get_bytes (is, addr, num, elem_size);
      return ops + 3;
    }
    case DDS_OP_VAL_ENU: {
      switch (DDS_OP_TYPE_SZ (insn))
      {
        case 1:
          for (uint32_t i = 0; i < num; i++)
             ((uint32_t *) addr)[i] = dds_is_get1 (is);
          break;
        case 2:
          for (uint32_t i = 0; i < num; i++)
             ((uint32_t *) addr)[i] = dds_is_get2 (is);
          break;
        case 4:
          dds_is_get_bytes (is, addr, num, 4);
          break;
        default:
          abort ();
      }
      return ops + 4;
    }
    case DDS_OP_VAL_BMK: {
      const uint32_t elem_size = DDS_OP_TYPE_SZ (insn);
      dds_is_get_bytes (is, addr, num, elem_size);
      return ops + 5;
    }
    case DDS_OP_VAL_STR: {
      char **ptr = (char **) addr;
      for (uint32_t i = 0; i < num; i++)
        ptr[i] = dds_stream_reuse_string (is, ptr[i], allocator);
      return ops + 3;
    }
    case DDS_OP_VAL_BST: {
      char *ptr = (char *) addr;
      const uint32_t elem_size = ops[4];
      for (uint32_t i = 0; i < num; i++)
        (void) dds_stream_reuse_string_bound (is, ptr + i * elem_size, allocator, elem_size, false);
      return ops + 5;
    }
    case DDS_OP_VAL_SEQ: case DDS_OP_VAL_BSQ: case DDS_OP_VAL_ARR: case DDS_OP_VAL_UNI: case DDS_OP_VAL_STU: {
      const uint32_t *jsr_ops = ops + DDS_OP_ADR_JSR (ops[3]);
      const uint32_t jmp = DDS_OP_ADR_JMP (ops[3]);
      const uint32_t elem_size = ops[4];
      for (uint32_t i = 0; i < num; i++)
        (void) dds_stream_read_impl (is, addr + i * elem_size, allocator, jsr_ops, false);
      return ops + (jmp ? jmp : 5);
    }
    case DDS_OP_VAL_EXT: {
      abort (); /* not supported */
      break;
    }
  }
  return NULL;
}

static const uint32_t *dds_stream_read_uni (dds_istream_t * __restrict is, char * __restrict discaddr, char * __restrict baseaddr, const struct dds_cdrstream_allocator * __restrict allocator, const uint32_t * __restrict ops, uint32_t insn)
{
  const uint32_t disc = read_union_discriminant (is, insn);
  switch (DDS_OP_SUBTYPE (insn))
  {
    case DDS_OP_VAL_BLN: case DDS_OP_VAL_1BY: *((uint8_t *) discaddr) = (uint8_t) disc; break;
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

    if (op_type_external (jeq_op[0]))
    {
      /* Allocate memory for @external union member. This memory must be initialized
          to 0, because the type may contain sequences that need to have 0 index/size
          or external fields that need to be initialized to null */
      assert (DDS_OP (jeq_op[0]) == DDS_OP_JEQ4);
      uint32_t sz = get_jeq4_type_size (valtype, jeq_op);
      if (*((char **) valaddr) == NULL)
      {
        *((char **) valaddr) = allocator->malloc (sz);
        memset (*((char **) valaddr), 0, sz);
      }
      valaddr = *((char **) valaddr);
    }

    switch (valtype)
    {
      case DDS_OP_VAL_BLN: case DDS_OP_VAL_1BY: *((uint8_t *) valaddr) = dds_is_get1 (is); break;
      case DDS_OP_VAL_2BY: *((uint16_t *) valaddr) = dds_is_get2 (is); break;
      case DDS_OP_VAL_4BY: *((uint32_t *) valaddr) = dds_is_get4 (is); break;
      case DDS_OP_VAL_8BY: *((uint64_t *) valaddr) = dds_is_get8 (is); break;
      case DDS_OP_VAL_ENU:
        switch (DDS_OP_TYPE_SZ (jeq_op[0]))
        {
          case 1: *((uint32_t *) valaddr) = dds_is_get1 (is); break;
          case 2: *((uint32_t *) valaddr) = dds_is_get2 (is); break;
          case 4: *((uint32_t *) valaddr) = dds_is_get4 (is); break;
          default: abort ();
        }
        break;
      case DDS_OP_VAL_STR:
        *(char **) valaddr = dds_stream_reuse_string (is, *((char **) valaddr), allocator);
        break;
      case DDS_OP_VAL_BST: case DDS_OP_VAL_SEQ: case DDS_OP_VAL_BSQ: case DDS_OP_VAL_ARR: case DDS_OP_VAL_BMK:
        (void) dds_stream_read_impl (is, valaddr, allocator, jeq_op + DDS_OP_ADR_JSR (jeq_op[0]), false);
        break;
      case DDS_OP_VAL_UNI: case DDS_OP_VAL_STU: {
        const uint32_t *jsr_ops = jeq_op + DDS_OP_ADR_JSR (jeq_op[0]);
        (void) dds_stream_read_impl (is, valaddr, allocator, jsr_ops, false);
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

static void dds_stream_alloc_external (const uint32_t * __restrict ops, uint32_t insn, void ** addr, const struct dds_cdrstream_allocator * __restrict allocator)
{
  /* Allocate memory for @external member. This memory must be initialized to 0,
      because the type may contain sequences that need to have 0 index/size
      or external fields that need to be initialized to null */
  uint32_t sz = get_adr_type_size (insn, ops);
  if (*((char **) *addr) == NULL)
  {
    *((char **) *addr) = allocator->malloc (sz);
    memset (*((char **) *addr), 0, sz);
  }
  *addr = *((char **) *addr);
}

static inline const uint32_t *dds_stream_read_adr (uint32_t insn, dds_istream_t * __restrict is, char * __restrict data, const struct dds_cdrstream_allocator * __restrict allocator, const uint32_t * __restrict ops, bool is_mutable_member)
{
  void *addr = data + ops[1];
  if (!stream_is_member_present (insn, is, is_mutable_member))
    return stream_free_sample_adr (insn, data, allocator, ops);

  if (op_type_external (insn))
    dds_stream_alloc_external (ops, insn, &addr, allocator);

  switch (DDS_OP_TYPE (insn))
  {
    case DDS_OP_VAL_BLN: case DDS_OP_VAL_1BY: *((uint8_t *) addr) = dds_is_get1 (is); ops += 2; break;
    case DDS_OP_VAL_2BY: *((uint16_t *) addr) = dds_is_get2 (is); ops += 2; break;
    case DDS_OP_VAL_4BY: *((uint32_t *) addr) = dds_is_get4 (is); ops += 2; break;
    case DDS_OP_VAL_8BY: *((uint64_t *) addr) = dds_is_get8 (is); ops += 2; break;
    case DDS_OP_VAL_STR: *((char **) addr) = dds_stream_reuse_string (is, *((char **) addr), allocator); ops += 2; break;
    case DDS_OP_VAL_BST: (void) dds_stream_reuse_string_bound (is, (char *) addr, allocator, ops[2], false); ops += 3; break;
    case DDS_OP_VAL_SEQ: case DDS_OP_VAL_BSQ: ops = dds_stream_read_seq (is, addr, allocator, ops, insn); break;
    case DDS_OP_VAL_ARR: ops = dds_stream_read_arr (is, addr, allocator, ops, insn); break;
    case DDS_OP_VAL_UNI: ops = dds_stream_read_uni (is, addr, data, allocator, ops, insn); break;
    case DDS_OP_VAL_ENU: {
      switch (DDS_OP_TYPE_SZ (insn))
      {
        case 1: *((uint32_t *) addr) = dds_is_get1 (is); break;
        case 2: *((uint32_t *) addr) = dds_is_get2 (is); break;
        case 4: *((uint32_t *) addr) = dds_is_get4 (is); break;
        default: abort ();
      }
      ops += 3;
      break;
    }
    case DDS_OP_VAL_BMK: {
      switch (DDS_OP_TYPE_SZ (insn))
      {
        case 1: *((uint8_t *) addr) = dds_is_get1 (is); break;
        case 2: *((uint16_t *) addr) = dds_is_get2 (is); break;
        case 4: *((uint32_t *) addr) = dds_is_get4 (is); break;
        case 8: *((uint64_t *) addr) = dds_is_get8 (is); break;
        default: abort ();
      }
      ops += 4;
      break;
    }
    case DDS_OP_VAL_EXT: {
      const uint32_t *jsr_ops = ops + DDS_OP_ADR_JSR (ops[2]);
      const uint32_t jmp = DDS_OP_ADR_JMP (ops[2]);

      /* skip DLC instruction for base type, handle as if it is final because the base type's
         members follow the derived types members without an extra DHEADER */
      if (op_type_base (insn) && jsr_ops[0] == DDS_OP_DLC)
        jsr_ops++;

      (void) dds_stream_read_impl (is, addr, allocator, jsr_ops, false);
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
    case DDS_OP_VAL_BLN: case DDS_OP_VAL_1BY: case DDS_OP_VAL_2BY: case DDS_OP_VAL_4BY: case DDS_OP_VAL_8BY:
    case DDS_OP_VAL_STR:
      return ops + 2;
    case DDS_OP_VAL_BST:
    case DDS_OP_VAL_ENU:
      return ops + 3;
    case DDS_OP_VAL_BMK:
      return ops + 4;
    case DDS_OP_VAL_SEQ:
    case DDS_OP_VAL_BSQ:
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

static const uint32_t *dds_stream_skip_adr_default (uint32_t insn, char * __restrict data, const struct dds_cdrstream_allocator * __restrict allocator, const uint32_t * __restrict ops)
{
  void *addr = data + ops[1];
  /* FIXME: currently only implicit default values are used, this code should be
     using default values that are specified in the type definition */

  /* Free memory in sample and set pointer to null in case of optional or external member.
     test for optional (which also gets the external flag) is added because string type
     is the exception for this rule, that does not get the external flag */
  if (op_type_external (insn) || op_type_optional (insn))
    return stream_free_sample_adr(insn, data, allocator, ops);

  switch (DDS_OP_TYPE (insn))
  {
    case DDS_OP_VAL_BLN: case DDS_OP_VAL_1BY: *(uint8_t *) addr = 0; return ops + 2;
    case DDS_OP_VAL_2BY: *(uint16_t *) addr = 0; return ops + 2;
    case DDS_OP_VAL_4BY: *(uint32_t *) addr = 0; return ops + 2;
    case DDS_OP_VAL_8BY: *(uint64_t *) addr = 0; return ops + 2;

    case DDS_OP_VAL_STR: *(char **) addr = dds_stream_reuse_string_empty (*(char **) addr, allocator); return ops + 2;
    case DDS_OP_VAL_BST: ((char *) addr)[0] = '\0'; return ops + 3;
    case DDS_OP_VAL_ENU: *(uint32_t *) addr = 0; return ops + 3;
    case DDS_OP_VAL_BMK:
      switch (DDS_OP_TYPE_SZ (insn))
      {
        case 1: *(uint8_t *) addr = 0; break;
        case 2: *(uint16_t *) addr = 0; break;
        case 4: *(uint32_t *) addr = 0; break;
        case 8: *(uint64_t *) addr = 0; break;
        default: abort ();
      }
      return ops + 4;
    case DDS_OP_VAL_SEQ: case DDS_OP_VAL_BSQ: {
      dds_sequence_t * const seq = (dds_sequence_t *) addr;
      seq->_length = 0;
      return skip_sequence_insns (insn, ops);
    }
    case DDS_OP_VAL_ARR: {
      return skip_array_default (insn, addr, allocator, ops);
    }
    case DDS_OP_VAL_UNI: {
      return skip_union_default (insn, addr, data, allocator, ops);
    }
    case DDS_OP_VAL_EXT: {
      const uint32_t *jsr_ops = ops + DDS_OP_ADR_JSR (ops[2]);
      const uint32_t jmp = DDS_OP_ADR_JMP (ops[2]);
      (void) dds_stream_skip_default (addr, allocator, jsr_ops);
      return ops + (jmp ? jmp : 3);
    }
    case DDS_OP_VAL_STU: {
      abort(); /* op type STU only supported as subtype */
      break;
    }
  }

  return NULL;
}

static const uint32_t *dds_stream_skip_delimited_default (char * __restrict data, const struct dds_cdrstream_allocator * __restrict allocator, const uint32_t * __restrict ops)
{
  return dds_stream_skip_default (data, allocator, ++ops);
}

static void dds_stream_skip_pl_member_default (char * __restrict data, const struct dds_cdrstream_allocator * __restrict allocator, const uint32_t * __restrict ops)
{
  uint32_t insn;
  while ((insn = *ops) != DDS_OP_RTS)
  {
    switch (DDS_OP (insn))
    {
      case DDS_OP_ADR: {
        ops = dds_stream_skip_default (data, allocator, ops);
        break;
      }
      case DDS_OP_JSR:
        dds_stream_skip_pl_member_default (data, allocator, ops + DDS_OP_JUMP (insn));
        ops++;
        break;
      case DDS_OP_RTS: case DDS_OP_JEQ: case DDS_OP_JEQ4: case DDS_OP_KOF:
      case DDS_OP_DLC: case DDS_OP_PLC: case DDS_OP_PLM:
        abort ();
        break;
    }
  }
}

static const uint32_t *dds_stream_skip_pl_memberlist_default (char * __restrict data, const struct dds_cdrstream_allocator * __restrict allocator, const uint32_t * __restrict ops)
{
  uint32_t insn;
  while (ops && (insn = *ops) != DDS_OP_RTS)
  {
    switch (DDS_OP (insn))
    {
      case DDS_OP_PLM: {
        uint32_t flags = DDS_PLM_FLAGS (insn);
        const uint32_t *plm_ops = ops + DDS_OP_ADR_PLM (insn);
        if (flags & DDS_OP_FLAG_BASE)
        {
          assert (plm_ops[0] == DDS_OP_PLC);
          plm_ops++; /* skip PLC op to go to first PLM for the base type */
          (void) dds_stream_skip_pl_memberlist_default (data, allocator, plm_ops);
        }
        else
        {
          dds_stream_skip_pl_member_default (data, allocator, plm_ops);
        }
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

static const uint32_t *dds_stream_skip_pl_default (char * __restrict data, const struct dds_cdrstream_allocator * __restrict allocator, const uint32_t * __restrict ops)
{
  /* skip PLC op */
  return dds_stream_skip_pl_memberlist_default (data, allocator, ++ops);
}

static const uint32_t *dds_stream_skip_default (char * __restrict data, const struct dds_cdrstream_allocator * __restrict allocator, const uint32_t * __restrict ops)
{
  uint32_t insn;
  while ((insn = *ops) != DDS_OP_RTS)
  {
    switch (DDS_OP (insn))
    {
      case DDS_OP_ADR: {
        ops = dds_stream_skip_adr_default (insn, data, allocator, ops);
        break;
      }
      case DDS_OP_JSR: {
        (void) dds_stream_skip_default (data, allocator, ops + DDS_OP_JUMP (insn));
        ops++;
        break;
      }
      case DDS_OP_RTS: case DDS_OP_JEQ: case DDS_OP_JEQ4: case DDS_OP_KOF: case DDS_OP_PLM:
        abort ();
        break;
      case DDS_OP_DLC:
        ops = dds_stream_skip_delimited_default (data, allocator, ops);
        break;
      case DDS_OP_PLC:
        ops = dds_stream_skip_pl_default (data, allocator, ops);
        break;
    }
  }
  return ops;
}

static const uint32_t *dds_stream_read_delimited (dds_istream_t * __restrict is, char * __restrict data, const struct dds_cdrstream_allocator * __restrict allocator, const uint32_t * __restrict ops)
{
  uint32_t delimited_sz = dds_is_get4 (is), delimited_offs = is->m_index, insn;
  ops++;
  while ((insn = *ops) != DDS_OP_RTS)
  {
    switch (DDS_OP (insn))
    {
      case DDS_OP_ADR: {
        /* skip fields that are not in serialized data for appendable type */
        ops = (is->m_index - delimited_offs < delimited_sz) ? dds_stream_read_adr (insn, is, data, allocator, ops, false) : dds_stream_skip_adr_default (insn, data, allocator, ops);
        break;
      }
      case DDS_OP_JSR: {
        (void) dds_stream_read_impl (is, data, allocator, ops + DDS_OP_JUMP (insn), false);
        ops++;
        break;
      }
      case DDS_OP_RTS: case DDS_OP_JEQ: case DDS_OP_JEQ4: case DDS_OP_KOF: case DDS_OP_DLC: case DDS_OP_PLC: case DDS_OP_PLM: {
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

static bool dds_stream_read_pl_member (dds_istream_t * __restrict is, char * __restrict data, const struct dds_cdrstream_allocator * __restrict allocator, uint32_t m_id, const uint32_t * __restrict ops)
{
  uint32_t insn, ops_csr = 0;
  bool found = false;

  /* FIXME: continue finding the member in the ops member list starting from the last
      found one, because in many cases the members will be in the data sequentially */
  while (!found && (insn = ops[ops_csr]) != DDS_OP_RTS)
  {
    assert (DDS_OP (insn) == DDS_OP_PLM);
    uint32_t flags = DDS_PLM_FLAGS (insn);
    const uint32_t *plm_ops = ops + ops_csr + DDS_OP_ADR_PLM (insn);
    if (flags & DDS_OP_FLAG_BASE)
    {
      assert (DDS_OP (plm_ops[0]) == DDS_OP_PLC);
      plm_ops++; /* skip PLC to go to first PLM from base type */
      found = dds_stream_read_pl_member (is, data, allocator, m_id, plm_ops);
    }
    else if (ops[ops_csr + 1] == m_id)
    {
      (void) dds_stream_read_impl (is, data, allocator, plm_ops, true);
      found = true;
      break;
    }
    ops_csr += 2;
  }
  return found;
}

static const uint32_t *dds_stream_read_pl (dds_istream_t * __restrict is, char * __restrict data, const struct dds_cdrstream_allocator * __restrict allocator, const uint32_t * __restrict ops)
{
  /* skip PLC op */
  ops++;

  /* default-initialize all members
      FIXME: optimize so that only members not in received data are initialized */
  dds_stream_skip_pl_memberlist_default (data, allocator, ops);

  /* read DHEADER */
  uint32_t pl_sz = dds_is_get4 (is), pl_offs = is->m_index;
  while (is->m_index - pl_offs < pl_sz)
  {
    /* read EMHEADER and next_int */
    uint32_t em_hdr = dds_is_get4 (is);
    uint32_t lc = EMHEADER_LENGTH_CODE (em_hdr), m_id = EMHEADER_MEMBERID (em_hdr), msz;
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
    if (!dds_stream_read_pl_member (is, data, allocator, m_id, ops))
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

static const uint32_t *dds_stream_read_impl (dds_istream_t * __restrict is, char * __restrict data, const struct dds_cdrstream_allocator * __restrict allocator, const uint32_t * __restrict ops, bool is_mutable_member)
{
  uint32_t insn;
  while ((insn = *ops) != DDS_OP_RTS)
  {
    switch (DDS_OP (insn))
    {
      case DDS_OP_ADR:
        ops = dds_stream_read_adr (insn, is, data, allocator, ops, is_mutable_member);
        break;
      case DDS_OP_JSR:
        (void) dds_stream_read_impl (is, data, allocator, ops + DDS_OP_JUMP (insn), is_mutable_member);
        ops++;
        break;
      case DDS_OP_RTS: case DDS_OP_JEQ: case DDS_OP_JEQ4: case DDS_OP_KOF: case DDS_OP_PLM:
        abort ();
        break;
      case DDS_OP_DLC:
        assert (is->m_xcdr_version == DDSI_RTPS_CDR_ENC_VERSION_2);
        ops = dds_stream_read_delimited (is, data, allocator, ops);
        break;
      case DDS_OP_PLC:
        assert (is->m_xcdr_version == DDSI_RTPS_CDR_ENC_VERSION_2);
        ops = dds_stream_read_pl (is, data, allocator, ops);
        break;
    }
  }
  return ops;
}

const uint32_t *dds_stream_read (dds_istream_t * __restrict is, char * __restrict data, const struct dds_cdrstream_allocator * __restrict allocator, const uint32_t * __restrict ops)
{
  return dds_stream_read_impl (is, data, allocator, ops, false);
}

/*******************************************************************************************
 **
 **  Validation and conversion to native endian.
 **
 *******************************************************************************************/

static inline void normalize_error (void) { }
static inline uint32_t normalize_error_offset (void) { normalize_error (); return UINT32_MAX; }
static inline bool normalize_error_bool (void) { normalize_error (); return false; }
static inline const uint32_t *normalize_error_ops (void) { normalize_error (); return NULL; }

/* Limit the size of the input buffer so we don't need to worry about adding
   padding and a primitive type overflowing our offset */
#define CDR_SIZE_MAX ((uint32_t) 0xfffffff0)

static uint32_t check_align_prim (uint32_t off, uint32_t size, uint32_t a_lg2, uint32_t c_lg2)
{
  assert (a_lg2 <= 3);
  const uint32_t a = 1u << a_lg2;
  assert (c_lg2 <= 3);
  const uint32_t c = 1u << c_lg2;
  assert (size <= CDR_SIZE_MAX);
  assert (off <= size);
  const uint32_t off1 = (off + a - 1) & ~(a - 1);
  assert (off <= off1 && off1 <= CDR_SIZE_MAX);
  if (size < off1 + c)
    return normalize_error_offset ();
  return off1;
}

static uint32_t check_align_prim_many (uint32_t off, uint32_t size, uint32_t a_lg2, uint32_t c_lg2, uint32_t n)
{
  assert (a_lg2 <= 3);
  const uint32_t a = 1u << a_lg2;
  assert (c_lg2 <= 3);
  assert (size <= CDR_SIZE_MAX);
  assert (off <= size);
  const uint32_t off1 = (off + a - 1) & ~(a - 1);
  assert (off <= off1 && off1 <= CDR_SIZE_MAX);
  if (size < off1 || ((size - off1) >> c_lg2) < n)
    return normalize_error_offset ();
  return off1;
}

static bool normalize_uint8 (uint32_t *off, uint32_t size) ddsrt_attribute_warn_unused_result ddsrt_nonnull_all;
static bool normalize_uint8 (uint32_t *off, uint32_t size)
{
  if (*off == size)
    return normalize_error_bool ();
  (*off)++;
  return true;
}

static bool normalize_uint16 (char * __restrict data, uint32_t * __restrict off, uint32_t size, bool bswap) ddsrt_attribute_warn_unused_result ddsrt_nonnull_all;
static bool normalize_uint16 (char * __restrict data, uint32_t * __restrict off, uint32_t size, bool bswap)
{
  if ((*off = check_align_prim (*off, size, 1, 1)) == UINT32_MAX)
    return false;
  if (bswap)
    *((uint16_t *) (data + *off)) = ddsrt_bswap2u (*((uint16_t *) (data + *off)));
  (*off) += 2;
  return true;
}

static bool normalize_uint32 (char * __restrict data, uint32_t * __restrict off, uint32_t size, bool bswap) ddsrt_attribute_warn_unused_result ddsrt_nonnull_all;
static bool normalize_uint32 (char * __restrict data, uint32_t * __restrict off, uint32_t size, bool bswap)
{
  if ((*off = check_align_prim (*off, size, 2, 2)) == UINT32_MAX)
    return false;
  if (bswap)
    *((uint32_t *) (data + *off)) = ddsrt_bswap4u (*((uint32_t *) (data + *off)));
  (*off) += 4;
  return true;
}

static bool normalize_uint64 (char * __restrict data, uint32_t * __restrict off, uint32_t size, bool bswap, uint32_t xcdr_version) ddsrt_attribute_warn_unused_result ddsrt_nonnull_all;
static bool normalize_uint64 (char * __restrict data, uint32_t * __restrict off, uint32_t size, bool bswap, uint32_t xcdr_version)
{
  if ((*off = check_align_prim (*off, size, xcdr_version == DDSI_RTPS_CDR_ENC_VERSION_2 ? 2 : 3, 3)) == UINT32_MAX)
    return false;
  if (bswap)
  {
    uint32_t x = ddsrt_bswap4u (* (uint32_t *) (data + *off));
    *((uint32_t *) (data + *off)) = ddsrt_bswap4u (* ((uint32_t *) (data + *off) + 1));
    *((uint32_t *) (data + *off) + 1) = x;
  }
  (*off) += 8;
  return true;
}

static bool normalize_bool (char * __restrict data, uint32_t * __restrict off, uint32_t size) ddsrt_attribute_warn_unused_result ddsrt_nonnull_all;
static bool normalize_bool (char * __restrict data, uint32_t * __restrict off, uint32_t size)
{
  if (*off == size)
    return normalize_error_bool ();
  uint8_t b = *((uint8_t *) (data + *off));
  if (b > 1)
    return normalize_error_bool ();
  (*off)++;
  return true;
}

static bool read_and_normalize_bool (bool * __restrict val, char * __restrict data, uint32_t * __restrict off, uint32_t size) ddsrt_attribute_warn_unused_result ddsrt_nonnull_all;
static bool read_and_normalize_bool (bool * __restrict val, char * __restrict data, uint32_t * __restrict off, uint32_t size)
{
  if (*off == size)
    return normalize_error_bool ();
  uint8_t b = *((uint8_t *) (data + *off));
  if (b > 1)
    return normalize_error_bool ();
  *val = b;
  (*off)++;
  return true;
}

static inline bool read_and_normalize_uint8 (uint8_t * __restrict val, char * __restrict data, uint32_t * __restrict off, uint32_t size) ddsrt_attribute_warn_unused_result ddsrt_nonnull_all;
static inline bool read_and_normalize_uint8 (uint8_t * __restrict val, char * __restrict data, uint32_t * __restrict off, uint32_t size)
{
  if ((*off = check_align_prim (*off, size, 0, 0)) == UINT32_MAX)
    return false;
  *val = *((uint8_t *) (data + *off));
  (*off)++;
  return true;
}

static inline bool read_and_normalize_uint16 (uint16_t * __restrict val, char * __restrict data, uint32_t * __restrict off, uint32_t size, bool bswap) ddsrt_attribute_warn_unused_result ddsrt_nonnull_all;
static inline bool read_and_normalize_uint16 (uint16_t * __restrict val, char * __restrict data, uint32_t * __restrict off, uint32_t size, bool bswap)
{
  if ((*off = check_align_prim (*off, size, 1, 1)) == UINT32_MAX)
    return false;
  if (bswap)
    *((uint16_t *) (data + *off)) = ddsrt_bswap2u (*((uint16_t *) (data + *off)));
  *val = *((uint16_t *) (data + *off));
  (*off) += 2;
  return true;
}

static inline bool read_and_normalize_uint32 (uint32_t * __restrict val, char * __restrict data, uint32_t * __restrict off, uint32_t size, bool bswap) ddsrt_attribute_warn_unused_result ddsrt_nonnull_all;
static inline bool read_and_normalize_uint32 (uint32_t * __restrict val, char * __restrict data, uint32_t * __restrict off, uint32_t size, bool bswap)
{
  if ((*off = check_align_prim (*off, size, 2, 2)) == UINT32_MAX)
    return false;
  if (bswap)
    *((uint32_t *) (data + *off)) = ddsrt_bswap4u (*((uint32_t *) (data + *off)));
  *val = *((uint32_t *) (data + *off));
  (*off) += 4;
  return true;
}

static inline bool read_and_normalize_uint64 (uint64_t * __restrict val, char * __restrict data, uint32_t * __restrict off, uint32_t size, bool bswap, uint32_t xcdr_version) ddsrt_attribute_warn_unused_result ddsrt_nonnull_all;
static inline bool read_and_normalize_uint64 (uint64_t * __restrict val, char * __restrict data, uint32_t * __restrict off, uint32_t size, bool bswap, uint32_t xcdr_version)
{
  if ((*off = check_align_prim (*off, size, xcdr_version == DDSI_RTPS_CDR_ENC_VERSION_2 ? 2 : 3, 3)) == UINT32_MAX)
    return false;
  union { uint32_t u32[2]; uint64_t u64; } u;
  u.u32[0] = * (uint32_t *) (data + *off);
  u.u32[1] = * ((uint32_t *) (data + *off) + 1);
  if (bswap)
  {
    u.u64 = ddsrt_bswap8u (u.u64);
    *((uint32_t *) (data + *off)) = u.u32[0];
    *((uint32_t *) (data + *off) + 1) = u.u32[1];
  }
  *val = u.u64;
  (*off) += 8;
  return true;
}

static bool peek_and_normalize_uint32 (uint32_t * __restrict val, char * __restrict data, uint32_t * __restrict off, uint32_t size, bool bswap) ddsrt_attribute_warn_unused_result ddsrt_nonnull_all;
static bool peek_and_normalize_uint32 (uint32_t * __restrict val, char * __restrict data, uint32_t * __restrict off, uint32_t size, bool bswap)
{
  if ((*off = check_align_prim (*off, size, 2, 2)) == UINT32_MAX)
    return false;
  if (bswap)
    *val = ddsrt_bswap4u (*((uint32_t *) (data + *off)));
  else
    *val = *((uint32_t *) (data + *off));
  return true;
}

static bool read_normalize_enum (uint32_t * __restrict val, char * __restrict data, uint32_t * __restrict off, uint32_t size, bool bswap, uint32_t insn, uint32_t max) ddsrt_attribute_warn_unused_result ddsrt_nonnull((1,2,3));
static bool read_normalize_enum (uint32_t * __restrict val, char * __restrict data, uint32_t * __restrict off, uint32_t size, bool bswap, uint32_t insn, uint32_t max)
{
  switch (DDS_OP_TYPE_SZ (insn))
  {
    case 1: {
      uint8_t val8;
      if (!read_and_normalize_uint8 (&val8, data, off, size))
        return false;
      *val = val8;
      break;
    }
    case 2: {
      uint16_t val16;
      if (!read_and_normalize_uint16 (&val16, data, off, size, bswap))
        return false;
      *val = val16;
      break;
    }
    case 4:
      if (!read_and_normalize_uint32 (val, data, off, size, bswap))
        return false;
      break;
    default:
      return normalize_error_bool ();
  }
  if (*val > max)
    return normalize_error_bool ();
  return true;
}

static bool normalize_enum (char * __restrict data, uint32_t * __restrict off, uint32_t size, bool bswap, uint32_t insn, uint32_t max) ddsrt_attribute_warn_unused_result ddsrt_nonnull_all;
static bool normalize_enum (char * __restrict data, uint32_t * __restrict off, uint32_t size, bool bswap, uint32_t insn, uint32_t max)
{
  uint32_t val;
  return read_normalize_enum (&val, data, off, size, bswap, insn, max);
}

static bool read_normalize_bitmask (uint64_t * __restrict val, char * __restrict data, uint32_t * __restrict off, uint32_t size, bool bswap, uint32_t xcdr_version, uint32_t insn, uint32_t bits_h, uint32_t bits_l) ddsrt_attribute_warn_unused_result ddsrt_nonnull((1,2,3));
static bool read_normalize_bitmask (uint64_t * __restrict val, char * __restrict data, uint32_t * __restrict off, uint32_t size, bool bswap, uint32_t xcdr_version, uint32_t insn, uint32_t bits_h, uint32_t bits_l)
{
  switch (DDS_OP_TYPE_SZ (insn))
  {
    case 1: {
      uint8_t val8;
      if (!read_and_normalize_uint8 (&val8, data, off, size))
        return false;
      *val = val8;
      break;
    }
    case 2: {
      uint16_t val16;
      if (!read_and_normalize_uint16 (&val16, data, off, size, bswap))
        return false;
      *val = val16;
      break;
    }
    case 4: {
      uint32_t val32;
      if (!read_and_normalize_uint32 (&val32, data, off, size, bswap))
        return false;
      *val = val32;
      break;
    }
    case 8:
      if (!read_and_normalize_uint64 (val, data, off, size, bswap, xcdr_version))
        return false;
      break;
    default:
      abort ();
  }
  if (!bitmask_value_valid (*val, bits_h, bits_l))
    return normalize_error_bool ();
  return true;
}

static bool normalize_bitmask (char * __restrict data, uint32_t * __restrict off, uint32_t size, bool bswap, uint32_t xcdr_version, uint32_t insn, uint32_t bits_h, uint32_t bits_l) ddsrt_attribute_warn_unused_result ddsrt_nonnull_all;
static bool normalize_bitmask (char * __restrict data, uint32_t * __restrict off, uint32_t size, bool bswap, uint32_t xcdr_version, uint32_t insn, uint32_t bits_h, uint32_t bits_l)
{
  uint64_t val;
  return read_normalize_bitmask (&val, data, off, size, bswap, xcdr_version, insn, bits_h, bits_l);
}

static bool normalize_string (char * __restrict data, uint32_t * __restrict off, uint32_t size, bool bswap, size_t maxsz) ddsrt_attribute_warn_unused_result ddsrt_nonnull_all;
static bool normalize_string (char * __restrict data, uint32_t * __restrict off, uint32_t size, bool bswap, size_t maxsz)
{
  uint32_t sz;
  if (!read_and_normalize_uint32 (&sz, data, off, size, bswap))
    return false;
  if (sz == 0 || size - *off < sz || maxsz < sz)
    return normalize_error_bool ();
  if (data[*off + sz - 1] != 0)
    return normalize_error_bool ();
  *off += sz;
  return true;
}

static bool normalize_primarray (char * __restrict data, uint32_t * __restrict off, uint32_t size, bool bswap, uint32_t num, enum dds_stream_typecode type, uint32_t xcdr_version) ddsrt_attribute_warn_unused_result ddsrt_nonnull_all;
static bool normalize_primarray (char * __restrict data, uint32_t * __restrict off, uint32_t size, bool bswap, uint32_t num, enum dds_stream_typecode type, uint32_t xcdr_version)
{
  switch (type)
  {
    case DDS_OP_VAL_1BY:
      if ((*off = check_align_prim_many (*off, size, 0, 0, num)) == UINT32_MAX)
        return false;
      *off += num;
      return true;
    case DDS_OP_VAL_2BY:
      if ((*off = check_align_prim_many (*off, size, 1, 1, num)) == UINT32_MAX)
        return false;
      if (bswap)
        dds_stream_swap (data + *off, 2, num);
      *off += 2 * num;
      return true;
    case DDS_OP_VAL_4BY:
      if ((*off = check_align_prim_many (*off, size, 2, 2, num)) == UINT32_MAX)
        return false;
      if (bswap)
        dds_stream_swap (data + *off, 4, num);
      *off += 4 * num;
      return true;
    case DDS_OP_VAL_8BY:
      if ((*off = check_align_prim_many (*off, size, xcdr_version == DDSI_RTPS_CDR_ENC_VERSION_2 ? 2 : 3, 3, num)) == UINT32_MAX)
        return false;
      if (bswap)
        dds_stream_swap (data + *off, 8, num);
      *off += 8 * num;
      return true;
    default:
      abort ();
      break;
  }
  return false;
}

static bool normalize_enumarray (char * __restrict data, uint32_t * __restrict off, uint32_t size, bool bswap, uint32_t enum_sz, uint32_t num, uint32_t max) ddsrt_attribute_warn_unused_result ddsrt_nonnull_all;
static bool normalize_enumarray (char * __restrict data, uint32_t * __restrict off, uint32_t size, bool bswap, uint32_t enum_sz, uint32_t num, uint32_t max)
{
  switch (enum_sz)
  {
    case 1: {
      if ((*off = check_align_prim_many (*off, size, 0, 0, num)) == UINT32_MAX)
        return false;
      uint8_t * const xs = (uint8_t *) (data + *off);
      for (uint32_t i = 0; i < num; i++)
        if (xs[i] > max)
          return normalize_error_bool ();
      *off += num;
      break;
    }
    case 2: {
      if ((*off = check_align_prim_many (*off, size, 1, 1, num)) == UINT32_MAX)
        return false;
      uint16_t * const xs = (uint16_t *) (data + *off);
      for (uint32_t i = 0; i < num; i++)
        if ((uint16_t) (bswap ? (xs[i] = ddsrt_bswap2u (xs[i])) : xs[i]) > max)
          return normalize_error_bool ();
      *off += 2 * num;
      break;
    }
    case 4: {
      if ((*off = check_align_prim_many (*off, size, 2, 2, num)) == UINT32_MAX)
        return false;
      uint32_t * const xs = (uint32_t *) (data + *off);
      for (uint32_t i = 0; i < num; i++)
        if ((uint32_t) (bswap ? (xs[i] = ddsrt_bswap4u (xs[i])) : xs[i]) > max)
          return normalize_error_bool ();
      *off += 4 * num;
      break;
    }
    default:
      return normalize_error_bool ();
  }
  return true;
}

static bool normalize_bitmaskarray (char * __restrict data, uint32_t * __restrict off, uint32_t size, bool bswap, uint32_t xcdr_version, uint32_t insn, uint32_t num, uint32_t bits_h, uint32_t bits_l) ddsrt_attribute_warn_unused_result ddsrt_nonnull_all;
static bool normalize_bitmaskarray (char * __restrict data, uint32_t * __restrict off, uint32_t size, bool bswap, uint32_t xcdr_version, uint32_t insn, uint32_t num, uint32_t bits_h, uint32_t bits_l)
{
  switch (DDS_OP_TYPE_SZ (insn))
  {
    case 1: {
      if ((*off = check_align_prim_many (*off, size, 0, 0, num)) == UINT32_MAX)
        return false;
      uint8_t * const xs = (uint8_t *) (data + *off);
      for (uint32_t i = 0; i < num; i++)
        if (!bitmask_value_valid (xs[i], bits_h, bits_l))
          return normalize_error_bool ();
      *off += num;
      break;
    }
    case 2: {
      if ((*off = check_align_prim_many (*off, size, 1, 1, num)) == UINT32_MAX)
        return false;
      uint16_t * const xs = (uint16_t *) (data + *off);
      for (uint32_t i = 0; i < num; i++)
        if (!bitmask_value_valid (bswap ? (xs[i] = ddsrt_bswap2u (xs[i])) : xs[i], bits_h, bits_l))
          return normalize_error_bool ();
      *off += 2 * num;
      break;
    }
    case 4: {
      if ((*off = check_align_prim_many (*off, size, 2, 2, num)) == UINT32_MAX)
        return false;
      uint32_t * const xs = (uint32_t *) (data + *off);
      for (uint32_t i = 0; i < num; i++)
        if (!bitmask_value_valid (bswap ? (xs[i] = ddsrt_bswap4u (xs[i])) : xs[i], bits_h, bits_l))
          return normalize_error_bool ();
      *off += 4 * num;
      break;
    }
    case 8: {
      if ((*off = check_align_prim_many (*off, size, xcdr_version == DDSI_RTPS_CDR_ENC_VERSION_2 ? 2 : 3, 3, num)) == UINT32_MAX)
        return false;
      uint64_t * const xs = (uint64_t *) (data + *off);
      for (uint32_t i = 0; i < num; i++)
      {
        if (bswap)
        {
          uint32_t x = ddsrt_bswap4u (* (uint32_t *) &xs[i]);
          *(uint32_t *) &xs[i] = ddsrt_bswap4u (* (((uint32_t *) &xs[i]) + 1));
          *(((uint32_t *) &xs[i]) + 1) = x;
        }
        if (!bitmask_value_valid (xs[i], bits_h, bits_l))
          return normalize_error_bool ();
      }
      *off += 8 * num;
      break;
    }
  }
  return true;
}

static bool read_and_normalize_collection_dheader (bool * __restrict has_dheader, uint32_t * __restrict size1, char * __restrict data, uint32_t * __restrict off, uint32_t size, bool bswap, const enum dds_stream_typecode subtype, uint32_t xcdr_version) ddsrt_attribute_warn_unused_result ddsrt_nonnull_all;
static bool read_and_normalize_collection_dheader (bool * __restrict has_dheader, uint32_t * __restrict size1, char * __restrict data, uint32_t * __restrict off, uint32_t size, bool bswap, const enum dds_stream_typecode subtype, uint32_t xcdr_version)
{
  if (is_dheader_needed (subtype, xcdr_version))
  {
    if (!read_and_normalize_uint32 (size1, data, off, size, bswap))
      return false;
    if (*size1 > size - *off)
      return normalize_error_bool ();
    *has_dheader = true;
    *size1 += *off;
    return true;
  }
  else
  {
    *has_dheader = false;
    *size1 = size;
    return true;
  }
}

static const uint32_t *normalize_seq (char * __restrict data, uint32_t * __restrict off, uint32_t size, bool bswap, uint32_t xcdr_version, const uint32_t * __restrict ops, uint32_t insn) ddsrt_attribute_warn_unused_result ddsrt_nonnull_all;
static const uint32_t *normalize_seq (char * __restrict data, uint32_t * __restrict off, uint32_t size, bool bswap, uint32_t xcdr_version, const uint32_t * __restrict ops, uint32_t insn)
{
  const enum dds_stream_typecode subtype = DDS_OP_SUBTYPE (insn);
  uint32_t bound_op = seq_is_bounded (DDS_OP_TYPE (insn)) ? 1 : 0;
  uint32_t bound = bound_op ? ops[2] : 0;
  bool has_dheader;
  uint32_t size1;
  if (!read_and_normalize_collection_dheader (&has_dheader, &size1, data, off, size, bswap, subtype, xcdr_version))
    return NULL;
  uint32_t num;
  if (!read_and_normalize_uint32 (&num, data, off, size1, bswap))
    return NULL;
  if (num == 0)
  {
    if (has_dheader && *off != size1)
      return normalize_error_ops ();
    return skip_sequence_insns (insn, ops);
  }
  if (bound && num > bound)
    return normalize_error_ops ();
  switch (subtype)
  {
    case DDS_OP_VAL_BLN:
      if (!normalize_enumarray (data, off, size1, bswap, 1, num, 1))
        return NULL;
      ops += 2 + bound_op;
      break;
    case DDS_OP_VAL_1BY: case DDS_OP_VAL_2BY: case DDS_OP_VAL_4BY: case DDS_OP_VAL_8BY:
      if (!normalize_primarray (data, off, size1, bswap, num, subtype, xcdr_version))
        return NULL;
      ops += 2 + bound_op;
      break;
    case DDS_OP_VAL_ENU:
      if (!normalize_enumarray (data, off, size1, bswap, DDS_OP_TYPE_SZ (insn), num, ops[2 + bound_op]))
        return NULL;
      ops += 3 + bound_op;
      break;
    case DDS_OP_VAL_BMK:
      if (!normalize_bitmaskarray (data, off, size1, bswap, xcdr_version, insn, num, ops[2 + bound_op], ops[3 + bound_op]))
        return NULL;
      ops += 4 + bound_op;
      break;
    case DDS_OP_VAL_STR: case DDS_OP_VAL_BST: {
      const size_t maxsz = (subtype == DDS_OP_VAL_STR) ? SIZE_MAX : ops[2 + bound_op];
      for (uint32_t i = 0; i < num; i++)
        if (!normalize_string (data, off, size1, bswap, maxsz))
          return NULL;
      ops += (subtype == DDS_OP_VAL_STR ? 2 : 3) + bound_op;
      break;
    }
    case DDS_OP_VAL_SEQ: case DDS_OP_VAL_BSQ: case DDS_OP_VAL_ARR: case DDS_OP_VAL_UNI: case DDS_OP_VAL_STU: {
      const uint32_t jmp = DDS_OP_ADR_JMP (ops[3 + bound_op]);
      uint32_t const * const jsr_ops = ops + DDS_OP_ADR_JSR (ops[3 + bound_op]);
      for (uint32_t i = 0; i < num; i++)
        if (stream_normalize_data_impl (data, off, size1, bswap, xcdr_version, jsr_ops, false) == NULL)
          return NULL;
      ops += jmp ? jmp : (4 + bound_op); /* FIXME: why would jmp be 0? */
      break;
    }
    case DDS_OP_VAL_EXT:
      ops = NULL;
      abort (); /* not supported */
      break;
  }
  if (has_dheader && *off != size1)
    return normalize_error_ops ();
  return ops;
}

static const uint32_t *normalize_arr (char * __restrict data, uint32_t * __restrict off, uint32_t size, bool bswap, uint32_t xcdr_version, const uint32_t * __restrict ops, uint32_t insn) ddsrt_attribute_warn_unused_result ddsrt_nonnull_all;
static const uint32_t *normalize_arr (char * __restrict data, uint32_t * __restrict off, uint32_t size, bool bswap, uint32_t xcdr_version, const uint32_t * __restrict ops, uint32_t insn)
{
  const enum dds_stream_typecode subtype = DDS_OP_SUBTYPE (insn);
  bool has_dheader;
  uint32_t size1;
  if (!read_and_normalize_collection_dheader (&has_dheader, &size1, data, off, size, bswap, subtype, xcdr_version))
    return NULL;
  const uint32_t num = ops[2];
  switch (subtype)
  {
    case DDS_OP_VAL_BLN:
      if (!normalize_enumarray (data, off, size1, bswap, 1, num, 1))
        return NULL;
      ops += 3;
      break;
    case DDS_OP_VAL_1BY: case DDS_OP_VAL_2BY: case DDS_OP_VAL_4BY: case DDS_OP_VAL_8BY:
      if (!normalize_primarray (data, off, size1, bswap, num, subtype, xcdr_version))
        return NULL;
      ops += 3;
      break;
    case DDS_OP_VAL_ENU:
      if (!normalize_enumarray (data, off, size1, bswap, DDS_OP_TYPE_SZ (insn), num, ops[3]))
        return NULL;
      ops += 4;
      break;
    case DDS_OP_VAL_BMK:
      if (!normalize_bitmaskarray (data, off, size1, bswap, xcdr_version, insn, num, ops[3], ops[4]))
        return NULL;
      ops += 5;
      break;
    case DDS_OP_VAL_STR: case DDS_OP_VAL_BST: {
      const size_t maxsz = (subtype == DDS_OP_VAL_STR) ? SIZE_MAX : ops[4];
      for (uint32_t i = 0; i < num; i++)
        if (!normalize_string (data, off, size1, bswap, maxsz))
          return NULL;
      ops += (subtype == DDS_OP_VAL_STR) ? 3 : 5;
      break;
    }
    case DDS_OP_VAL_SEQ: case DDS_OP_VAL_BSQ: case DDS_OP_VAL_ARR: case DDS_OP_VAL_UNI: case DDS_OP_VAL_STU: {
      const uint32_t jmp = DDS_OP_ADR_JMP (ops[3]);
      uint32_t const * const jsr_ops = ops + DDS_OP_ADR_JSR (ops[3]);
      for (uint32_t i = 0; i < num; i++)
        if (stream_normalize_data_impl (data, off, size1, bswap, xcdr_version, jsr_ops, false) == NULL)
          return NULL;
      ops += jmp ? jmp : 5;
      break;
    }
    case DDS_OP_VAL_EXT:
      ops = NULL;
      abort (); /* not supported */
      break;
  }
  if (has_dheader && *off != size1)
    return normalize_error_ops ();
  return ops;
}

static bool normalize_uni_disc (uint32_t * __restrict val, char * __restrict data, uint32_t * __restrict off, uint32_t size, bool bswap, uint32_t insn, const uint32_t * __restrict ops) ddsrt_attribute_warn_unused_result ddsrt_nonnull_all;
static bool normalize_uni_disc (uint32_t * __restrict val, char * __restrict data, uint32_t * __restrict off, uint32_t size, bool bswap, uint32_t insn, const uint32_t * __restrict ops)
{
  switch (DDS_OP_SUBTYPE (insn))
  {
    case DDS_OP_VAL_BLN: {
      bool bval;
      if (!read_and_normalize_bool (&bval, data, off, size))
        return false;
      *val = bval;
      return true;
    }
    case DDS_OP_VAL_1BY:
      if ((*off = check_align_prim (*off, size, 0, 0)) == UINT32_MAX)
        return false;
      *val = *((uint8_t *) (data + *off));
      (*off) += 1;
      return true;
    case DDS_OP_VAL_2BY:
      if ((*off = check_align_prim (*off, size, 1, 1)) == UINT32_MAX)
        return false;
      if (bswap)
        *((uint16_t *) (data + *off)) = ddsrt_bswap2u (*((uint16_t *) (data + *off)));
      *val = *((uint16_t *) (data + *off));
      (*off) += 2;
      return true;
    case DDS_OP_VAL_4BY:
      if ((*off = check_align_prim (*off, size, 2, 2)) == UINT32_MAX)
        return false;
      if (bswap)
        *((uint32_t *) (data + *off)) = ddsrt_bswap4u (*((uint32_t *) (data + *off)));
      *val = *((uint32_t *) (data + *off));
      (*off) += 4;
      return true;
    case DDS_OP_VAL_ENU:
      return read_normalize_enum (val, data, off, size, bswap, insn, ops[4]);
    default:
      abort ();
  }
  return false;
}

static const uint32_t *normalize_uni (char * __restrict data, uint32_t * __restrict off, uint32_t size, bool bswap, uint32_t xcdr_version, const uint32_t * __restrict ops, uint32_t insn) ddsrt_attribute_warn_unused_result ddsrt_nonnull_all;
static const uint32_t *normalize_uni (char * __restrict data, uint32_t * __restrict off, uint32_t size, bool bswap, uint32_t xcdr_version, const uint32_t * __restrict ops, uint32_t insn)
{
  uint32_t disc;
  if (!normalize_uni_disc (&disc, data, off, size, bswap, insn, ops))
    return NULL;
  uint32_t const * const jeq_op = find_union_case (ops, disc);
  ops += DDS_OP_ADR_JMP (ops[3]);
  if (jeq_op)
  {
    const enum dds_stream_typecode valtype = DDS_JEQ_TYPE (jeq_op[0]);
    switch (valtype)
    {
      case DDS_OP_VAL_BLN: if (!normalize_bool (data, off, size)) return NULL; break;
      case DDS_OP_VAL_1BY: if (!normalize_uint8 (off, size)) return NULL; break;
      case DDS_OP_VAL_2BY: if (!normalize_uint16 (data, off, size, bswap)) return NULL; break;
      case DDS_OP_VAL_4BY: if (!normalize_uint32 (data, off, size, bswap)) return NULL; break;
      case DDS_OP_VAL_8BY: if (!normalize_uint64 (data, off, size, bswap, xcdr_version)) return NULL; break;
      case DDS_OP_VAL_STR: if (!normalize_string (data, off, size, bswap, SIZE_MAX)) return NULL; break;
      case DDS_OP_VAL_ENU: if (!normalize_enum (data, off, size, bswap, jeq_op[0], jeq_op[3])) return NULL; break;
      case DDS_OP_VAL_BST: case DDS_OP_VAL_SEQ: case DDS_OP_VAL_BSQ: case DDS_OP_VAL_ARR: case DDS_OP_VAL_UNI: case DDS_OP_VAL_STU: case DDS_OP_VAL_BMK:
        if (stream_normalize_data_impl (data, off, size, bswap, xcdr_version, jeq_op + DDS_OP_ADR_JSR (jeq_op[0]), false) == NULL)
          return NULL;
        break;
      case DDS_OP_VAL_EXT:
        abort (); /* not supported */
        break;
    }
  }
  return ops;
}

static const uint32_t *stream_normalize_adr (uint32_t insn, char * __restrict data, uint32_t * __restrict off, uint32_t size, bool bswap, uint32_t xcdr_version, const uint32_t * __restrict ops, bool is_mutable_member) ddsrt_attribute_warn_unused_result ddsrt_nonnull_all;
static const uint32_t *stream_normalize_adr (uint32_t insn, char * __restrict data, uint32_t * __restrict off, uint32_t size, bool bswap, uint32_t xcdr_version, const uint32_t * __restrict ops, bool is_mutable_member)
{
  if (op_type_optional (insn))
  {
    bool present = true;
    if (!is_mutable_member)
    {
      if (!read_and_normalize_bool (&present, data, off, size))
        return NULL;
    }
    if (!present)
      return dds_stream_skip_adr (insn, ops);
  }
  switch (DDS_OP_TYPE (insn))
  {
    case DDS_OP_VAL_BLN: if (!normalize_bool (data, off, size)) return NULL; ops += 2; break;
    case DDS_OP_VAL_1BY: if (!normalize_uint8 (off, size)) return NULL; ops += 2; break;
    case DDS_OP_VAL_2BY: if (!normalize_uint16 (data, off, size, bswap)) return NULL; ops += 2; break;
    case DDS_OP_VAL_4BY: if (!normalize_uint32 (data, off, size, bswap)) return NULL; ops += 2; break;
    case DDS_OP_VAL_8BY: if (!normalize_uint64 (data, off, size, bswap, xcdr_version)) return NULL; ops += 2; break;
    case DDS_OP_VAL_STR: if (!normalize_string (data, off, size, bswap, SIZE_MAX)) return NULL; ops += 2; break;
    case DDS_OP_VAL_BST: if (!normalize_string (data, off, size, bswap, ops[2])) return NULL; ops += 3; break;
    case DDS_OP_VAL_SEQ: case DDS_OP_VAL_BSQ: ops = normalize_seq (data, off, size, bswap, xcdr_version, ops, insn); if (!ops) return NULL; break;
    case DDS_OP_VAL_ARR: ops = normalize_arr (data, off, size, bswap, xcdr_version, ops, insn); if (!ops) return NULL; break;
    case DDS_OP_VAL_UNI: ops = normalize_uni (data, off, size, bswap, xcdr_version, ops, insn); if (!ops) return NULL; break;
    case DDS_OP_VAL_ENU: if (!normalize_enum (data, off, size, bswap, insn, ops[2])) return NULL; ops += 3; break;
    case DDS_OP_VAL_BMK: if (!normalize_bitmask (data, off, size, bswap, xcdr_version, insn, ops[2], ops[3])) return NULL; ops += 4; break;
    case DDS_OP_VAL_EXT: {
      const uint32_t *jsr_ops = ops + DDS_OP_ADR_JSR (ops[2]);
      const uint32_t jmp = DDS_OP_ADR_JMP (ops[2]);

      /* skip DLC instruction for base type, the base type members are not preceded by a DHEADER */
      if (op_type_base (insn) && jsr_ops[0] == DDS_OP_DLC)
        jsr_ops++;

      if (stream_normalize_data_impl (data, off, size, bswap, xcdr_version, jsr_ops, false) == NULL)
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

static const uint32_t *stream_normalize_delimited (char * __restrict data, uint32_t * __restrict off, uint32_t size, bool bswap, uint32_t xcdr_version, const uint32_t * __restrict ops) ddsrt_attribute_warn_unused_result ddsrt_nonnull_all;
static const uint32_t *stream_normalize_delimited (char * __restrict data, uint32_t * __restrict off, uint32_t size, bool bswap, uint32_t xcdr_version, const uint32_t * __restrict ops)
{
  uint32_t delimited_sz;
  if (!read_and_normalize_uint32 (&delimited_sz, data, off, size, bswap))
    return NULL;

  // can't trust the declared size in the header: certainly it must fit in the remaining bytes
  if (delimited_sz > size - *off)
    return normalize_error_ops ();
  // can't trust the payload either: it must not only fit in the remaining bytes in the input,
  // but also in the declared size in the header
  uint32_t size1 = *off + delimited_sz;
  assert (size1 <= size);

  ops++; /* skip DLC op */
  uint32_t insn;
  while ((insn = *ops) != DDS_OP_RTS && *off < size1)
  {
    switch (DDS_OP (insn))
    {
      case DDS_OP_ADR:
        if ((ops = stream_normalize_adr (insn, data, off, size1, bswap, xcdr_version, ops, false)) == NULL)
          return NULL;
        break;
      case DDS_OP_JSR:
        if (stream_normalize_data_impl (data, off, size1, bswap, xcdr_version, ops + DDS_OP_JUMP (insn), false) == NULL)
          return NULL;
        ops++;
        break;
      case DDS_OP_RTS: case DDS_OP_JEQ: case DDS_OP_JEQ4: case DDS_OP_KOF: case DDS_OP_DLC: case DDS_OP_PLC: case DDS_OP_PLM:
        abort ();
        break;
    }
  }

  if (insn != DDS_OP_RTS)
  {
#if 0 // FIXME: need to deal with type coercion flags
    if (!type_widening_allowed)
      return NULL;
#endif
    /* skip fields that are not in serialized data for appendable type */
    while ((insn = *ops) != DDS_OP_RTS)
      ops = dds_stream_skip_adr (insn, ops);
  }

  // whether we consumed all bytes depends on whether the serialized type is the same as the
  // one we expect, but if the input validation is correct, we cannot have progressed beyond
  // the declared size
  assert (*off <= size1);
  *off = size1;
  return ops;
}

enum normalize_pl_member_result {
  NPMR_NOT_FOUND,
  NPMR_FOUND,
  NPMR_ERROR // found the data, but normalization failed
};

static enum normalize_pl_member_result dds_stream_normalize_pl_member (char * __restrict data, uint32_t m_id, uint32_t * __restrict off, uint32_t size, bool bswap, uint32_t xcdr_version, const uint32_t * __restrict ops) ddsrt_attribute_warn_unused_result ddsrt_nonnull_all;
static enum normalize_pl_member_result dds_stream_normalize_pl_member (char * __restrict data, uint32_t m_id, uint32_t * __restrict off, uint32_t size, bool bswap, uint32_t xcdr_version, const uint32_t * __restrict ops)
{
  uint32_t insn, ops_csr = 0;
  enum normalize_pl_member_result result = NPMR_NOT_FOUND;
  while (result == NPMR_NOT_FOUND && (insn = ops[ops_csr]) != DDS_OP_RTS)
  {
    assert (DDS_OP (insn) == DDS_OP_PLM);
    uint32_t flags = DDS_PLM_FLAGS (insn);
    const uint32_t *plm_ops = ops + ops_csr + DDS_OP_ADR_PLM (insn);
    if (flags & DDS_OP_FLAG_BASE)
    {
      assert (DDS_OP (plm_ops[0]) == DDS_OP_PLC);
      plm_ops++; /* skip PLC to go to first PLM from base type */
      result = dds_stream_normalize_pl_member (data, m_id, off, size, bswap, xcdr_version, plm_ops);
    }
    else if (ops[ops_csr + 1] == m_id)
    {
      if (stream_normalize_data_impl (data, off, size, bswap, xcdr_version, plm_ops, true))
        result = NPMR_FOUND;
      else
        result = NPMR_ERROR;
      break;
    }
    ops_csr += 2;
  }
  return result;
}

static const uint32_t *stream_normalize_pl (char * __restrict data, uint32_t * __restrict off, uint32_t size, bool bswap, uint32_t xcdr_version, const uint32_t * __restrict ops) ddsrt_attribute_warn_unused_result ddsrt_nonnull_all;
static const uint32_t *stream_normalize_pl (char * __restrict data, uint32_t * __restrict off, uint32_t size, bool bswap, uint32_t xcdr_version, const uint32_t * __restrict ops)
{
  /* skip PLC op */
  ops++;

  /* normalize DHEADER */
  uint32_t pl_sz;
  if (!read_and_normalize_uint32 (&pl_sz, data, off, size, bswap))
    return NULL;
  // reject if fewer than pl_sz bytes remain in the input
  if (pl_sz > size - *off)
    return normalize_error_ops ();
  const uint32_t size1 = *off + pl_sz;

  while (*off < size1)
  {
    /* normalize EMHEADER */
    uint32_t em_hdr;
    if (!read_and_normalize_uint32 (&em_hdr, data, off, size1, bswap))
      return NULL;
    uint32_t lc = EMHEADER_LENGTH_CODE (em_hdr), m_id = EMHEADER_MEMBERID (em_hdr), msz;
    bool must_understand = em_hdr & EMHEADER_FLAG_MUSTUNDERSTAND;
    switch (lc)
    {
      case LENGTH_CODE_1B: case LENGTH_CODE_2B: case LENGTH_CODE_4B: case LENGTH_CODE_8B:
        msz = 1u << lc;
        break;
      case LENGTH_CODE_NEXTINT:
        /* NEXTINT */
        if (!read_and_normalize_uint32 (&msz, data, off, size1, bswap))
          return NULL;
        break;
      case LENGTH_CODE_ALSO_NEXTINT: case LENGTH_CODE_ALSO_NEXTINT4: case LENGTH_CODE_ALSO_NEXTINT8:
        /* length is part of serialized data */
        if (!peek_and_normalize_uint32 (&msz, data, off, size1, bswap))
          return NULL;
        if (lc > LENGTH_CODE_ALSO_NEXTINT)
        {
          uint32_t shift = lc - 4;
          if (msz > UINT32_MAX >> shift)
            return normalize_error_ops ();
          msz <<= shift;
        }
        /* length embedded in member does not include it's own 4 bytes, we need to be able
           to add those 4; technically perhaps this would be valid CDR but if so, we don't
           support it */
        if (msz > UINT32_MAX - 4)
          return normalize_error_ops ();
        else
          msz += 4;
        break;
      default:
        abort ();
        break;
    }
    // reject if fewer than msz bytes remain in declared size of the parameter list
    if (msz > size1 - *off)
      return normalize_error_ops ();
    // don't allow member values that exceed its declared size
    const uint32_t size2 = *off + msz;
    switch (dds_stream_normalize_pl_member (data, m_id, off, size2, bswap, xcdr_version, ops))
    {
      case NPMR_NOT_FOUND:
        /* FIXME: the caller should be able to differentiate between a sample that
           is dropped because of an unknown member that has the must-understand flag
           and a sample that is dropped because the data is invalid. This requires
           changes in the cdrstream interface, but also in the serdata interface to
           pass the return value to ddsi_receive. */
        if (must_understand)
          return normalize_error_ops ();
        *off = size2;
        break;
      case NPMR_FOUND:
        if (*off != size2)
          return normalize_error_ops ();
        break;
      case NPMR_ERROR:
        return NULL;
    }
  }

  /* skip all PLM-memberid pairs */
  while (ops[0] != DDS_OP_RTS)
    ops += 2;

  return ops;
}

static const uint32_t *stream_normalize_data_impl (char * __restrict data, uint32_t * __restrict off, uint32_t size, bool bswap, uint32_t xcdr_version, const uint32_t * __restrict ops, bool is_mutable_member) ddsrt_attribute_warn_unused_result ddsrt_nonnull ((1, 2, 6));
static const uint32_t *stream_normalize_data_impl (char * __restrict data, uint32_t * __restrict off, uint32_t size, bool bswap, uint32_t xcdr_version, const uint32_t * __restrict ops, bool is_mutable_member)
{
  uint32_t insn;
  while ((insn = *ops) != DDS_OP_RTS)
  {
    switch (DDS_OP (insn))
    {
      case DDS_OP_ADR: {
        if ((ops = stream_normalize_adr (insn, data, off, size, bswap, xcdr_version, ops, is_mutable_member)) == NULL)
          return NULL;
        break;
      }
      case DDS_OP_JSR: {
        if (stream_normalize_data_impl (data, off, size, bswap, xcdr_version, ops + DDS_OP_JUMP (insn), is_mutable_member) == NULL)
          return NULL;
        ops++;
        break;
      }
      case DDS_OP_RTS: case DDS_OP_JEQ: case DDS_OP_JEQ4: case DDS_OP_KOF: case DDS_OP_PLM: {
        abort ();
        break;
      }
      case DDS_OP_DLC: {
        if (xcdr_version != DDSI_RTPS_CDR_ENC_VERSION_2)
          return normalize_error_ops ();
        if ((ops = stream_normalize_delimited (data, off, size, bswap, xcdr_version, ops)) == NULL)
          return NULL;
        break;
      }
      case DDS_OP_PLC: {
        if (xcdr_version != DDSI_RTPS_CDR_ENC_VERSION_2)
          return normalize_error_ops ();
        if ((ops = stream_normalize_pl (data, off, size, bswap, xcdr_version, ops)) == NULL)
          return NULL;
        break;
      }
    }
  }
  return ops;
}

const uint32_t *dds_stream_normalize_data (char * __restrict data, uint32_t * __restrict off, uint32_t size, bool bswap, uint32_t xcdr_version, const uint32_t * __restrict ops)
{
  return stream_normalize_data_impl (data, off, size, bswap, xcdr_version, ops, false);
}

static bool stream_normalize_key_impl (void * __restrict data, uint32_t size, uint32_t *offs, bool bswap, uint32_t xcdr_version, const uint32_t * __restrict ops, uint16_t key_offset_count, const uint32_t * key_offset_insn) ddsrt_attribute_warn_unused_result ddsrt_nonnull ((1, 3, 6));
static bool stream_normalize_key_impl (void * __restrict data, uint32_t size, uint32_t *offs, bool bswap, uint32_t xcdr_version, const uint32_t * __restrict ops, uint16_t key_offset_count, const uint32_t * key_offset_insn)
{
  uint32_t insn = ops[0];
  assert (insn_key_ok_p (insn));
  switch (DDS_OP_TYPE (insn))
  {
    case DDS_OP_VAL_BLN: if (!normalize_bool (data, offs, size)) return false; break;
    case DDS_OP_VAL_1BY: if (!normalize_uint8 (offs, size)) return false; break;
    case DDS_OP_VAL_2BY: if (!normalize_uint16 (data, offs, size, bswap)) return false; break;
    case DDS_OP_VAL_4BY: if (!normalize_uint32 (data, offs, size, bswap)) return false; break;
    case DDS_OP_VAL_ENU: if (!normalize_enum (data, offs, size, bswap, insn, ops[2])) return false; break;
    case DDS_OP_VAL_BMK: if (!normalize_bitmask (data, offs, size, bswap, xcdr_version, insn, ops[2], ops[3])) return false; break;
    case DDS_OP_VAL_8BY: if (!normalize_uint64 (data, offs, size, bswap, xcdr_version)) return false; break;
    case DDS_OP_VAL_STR: if (!normalize_string (data, offs, size, bswap, SIZE_MAX)) return false; break;
    case DDS_OP_VAL_BST: if (!normalize_string (data, offs, size, bswap, ops[2])) return false; break;
    case DDS_OP_VAL_ARR: if (!normalize_arr (data, offs, size, bswap, xcdr_version, ops, insn)) return false; break;
    case DDS_OP_VAL_EXT: {
      assert (key_offset_count > 0);
      const uint32_t *jsr_ops = ops + DDS_OP_ADR_JSR (ops[2]) + *key_offset_insn;
      if (!stream_normalize_key_impl (data, size, offs, bswap, xcdr_version, jsr_ops, --key_offset_count, ++key_offset_insn))
        return false;
      break;
    }
    case DDS_OP_VAL_SEQ: case DDS_OP_VAL_BSQ: case DDS_OP_VAL_UNI: case DDS_OP_VAL_STU:
      abort ();
      break;
  }
  return true;
}

static bool stream_normalize_key (void * __restrict data, uint32_t size, bool bswap, uint32_t xcdr_version, const struct dds_cdrstream_desc * __restrict desc, uint32_t *actual_size) ddsrt_attribute_warn_unused_result ddsrt_nonnull_all;
static bool stream_normalize_key (void * __restrict data, uint32_t size, bool bswap, uint32_t xcdr_version, const struct dds_cdrstream_desc * __restrict desc, uint32_t *actual_size)
{
  uint32_t offs = 0;
  for (uint32_t i = 0; i < desc->keys.nkeys; i++)
  {
    const uint32_t *op = desc->ops.ops + desc->keys.keys[i].ops_offs;
    switch (DDS_OP (*op))
    {
      case DDS_OP_KOF: {
        uint16_t n_offs = DDS_OP_LENGTH (*op);
        if (!stream_normalize_key_impl (data, size, &offs, bswap, xcdr_version, desc->ops.ops + op[1], --n_offs, op + 2))
          return false;
        break;
      }
      case DDS_OP_ADR: {
        if (!stream_normalize_key_impl (data, size, &offs, bswap, xcdr_version, op, 0, NULL))
          return false;
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

bool dds_stream_normalize (void * __restrict data, uint32_t size, bool bswap, uint32_t xcdr_version, const struct dds_cdrstream_desc * __restrict desc, bool just_key, uint32_t * __restrict actual_size)
{
  uint32_t off = 0;
  if (size > CDR_SIZE_MAX)
    return normalize_error_bool ();
  else if (just_key)
    return stream_normalize_key (data, size, bswap, xcdr_version, desc, actual_size);
  else if (!stream_normalize_data_impl (data, &off, size, bswap, xcdr_version, desc->ops.ops, false))
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

static const uint32_t *dds_stream_free_sample_seq (char * __restrict addr, const struct dds_cdrstream_allocator * __restrict allocator, const uint32_t * __restrict ops, uint32_t insn)
{
  dds_sequence_t * const seq = (dds_sequence_t *) addr;
  uint32_t num = (seq->_buffer == NULL) ? 0 : (seq->_maximum > seq->_length) ? seq->_maximum : seq->_length;
  const enum dds_stream_typecode subtype = DDS_OP_SUBTYPE (insn);
  uint32_t bound_op = seq_is_bounded (DDS_OP_TYPE (insn)) ? 1 : 0;
  if ((seq->_release && num) || subtype > DDS_OP_VAL_STR)
  {
    switch (subtype)
    {
      case DDS_OP_VAL_BLN: case DDS_OP_VAL_1BY: case DDS_OP_VAL_2BY: case DDS_OP_VAL_4BY: case DDS_OP_VAL_8BY:
        ops += 2 + bound_op;
        break;
      case DDS_OP_VAL_BST: case DDS_OP_VAL_ENU:
        ops += 3 + bound_op;
        break;
      case DDS_OP_VAL_BMK:
        ops += 4 + bound_op;
        break;
      case DDS_OP_VAL_STR: {
        char **ptr = (char **) seq->_buffer;
        while (num--)
          allocator->free (*ptr++);
        ops += 2 + bound_op;
        break;
      }
      case DDS_OP_VAL_SEQ: case DDS_OP_VAL_BSQ: case DDS_OP_VAL_ARR: case DDS_OP_VAL_UNI: case DDS_OP_VAL_STU: {
        const uint32_t elem_size = ops[2 + bound_op];
        const uint32_t jmp = DDS_OP_ADR_JMP (ops[3 + bound_op]);
        const uint32_t *jsr_ops = ops + DDS_OP_ADR_JSR (ops[3 + bound_op]);
        char *ptr = (char *) seq->_buffer;
        while (num--)
        {
          dds_stream_free_sample (ptr, allocator, jsr_ops);
          ptr += elem_size;
        }
        ops += jmp ? jmp : (4 + bound_op);
        break;
      }
      case DDS_OP_VAL_EXT: {
        abort (); /* not supported */
        break;
      }
    }
  }
  else
    ops = skip_sequence_insns (insn, ops);

  if (seq->_release)
  {
    allocator->free (seq->_buffer);
    seq->_maximum = 0;
    seq->_length = 0;
    seq->_buffer = NULL;
  }
  return ops;
}

static const uint32_t *dds_stream_free_sample_arr (char * __restrict addr, const struct dds_cdrstream_allocator * __restrict allocator, const uint32_t * __restrict ops, uint32_t insn)
{
  ops += 2;
  const enum dds_stream_typecode subtype = DDS_OP_SUBTYPE (insn);
  uint32_t num = *ops++;
  switch (subtype)
  {
    case DDS_OP_VAL_BLN: case DDS_OP_VAL_1BY: case DDS_OP_VAL_2BY: case DDS_OP_VAL_4BY: case DDS_OP_VAL_8BY: break;
    case DDS_OP_VAL_ENU: ops++; break;
    case DDS_OP_VAL_BMK: case DDS_OP_VAL_BST: ops += 2; break;
    case DDS_OP_VAL_STR: {
      char **ptr = (char **) addr;
      while (num--)
        allocator->free (*ptr++);
      break;
    }
    case DDS_OP_VAL_SEQ: case DDS_OP_VAL_BSQ: case DDS_OP_VAL_ARR: case DDS_OP_VAL_UNI: case DDS_OP_VAL_STU: {
      const uint32_t *jsr_ops = ops + DDS_OP_ADR_JSR (*ops) - 3;
      const uint32_t jmp = DDS_OP_ADR_JMP (*ops);
      const uint32_t elem_size = ops[1];
      while (num--)
      {
        dds_stream_free_sample (addr, allocator, jsr_ops);
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

static const uint32_t *dds_stream_free_sample_uni (char * __restrict discaddr, char * __restrict baseaddr, const struct dds_cdrstream_allocator * __restrict allocator, const uint32_t * __restrict ops, uint32_t insn)
{
  uint32_t disc = 0;
  switch (DDS_OP_SUBTYPE (insn))
  {
    case DDS_OP_VAL_BLN: case DDS_OP_VAL_1BY: disc = *((uint8_t *) discaddr); break;
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

    /* de-reference addr in case of an external member, except strings */
    if (op_type_external (jeq_op[0]))
    {
      assert (DDS_OP (jeq_op[0]) == DDS_OP_JEQ4);
      valaddr = *((char **) valaddr);
      if (!valaddr)
        goto no_ext_member;
    }

    switch (subtype)
    {
      case DDS_OP_VAL_BLN: case DDS_OP_VAL_1BY: case DDS_OP_VAL_2BY: case DDS_OP_VAL_4BY: case DDS_OP_VAL_8BY: case DDS_OP_VAL_BST: case DDS_OP_VAL_ENU: break;
      case DDS_OP_VAL_STR:
        allocator->free (*((char **) valaddr));
        *((char **) valaddr) = NULL;
        break;
      case DDS_OP_VAL_UNI: case DDS_OP_VAL_STU: case DDS_OP_VAL_SEQ: case DDS_OP_VAL_BSQ: case DDS_OP_VAL_ARR: case DDS_OP_VAL_BMK:
        dds_stream_free_sample (valaddr, allocator, jeq_op + DDS_OP_ADR_JSR (jeq_op[0]));
        break;
      case DDS_OP_VAL_EXT:
        abort (); /* not supported */
        break;
    }

    /* free buffer of the external field */
    if (op_type_external (jeq_op[0]))
    {
      allocator->free (valaddr);
      valaddr = NULL;
    }
  }
no_ext_member:
  return ops;
}

static const uint32_t *dds_stream_free_sample_pl (char * __restrict addr, const struct dds_cdrstream_allocator * __restrict allocator, const uint32_t * __restrict ops)
{
  uint32_t insn;
  assert (ops[0] == DDS_OP_PLC);
  ops++; /* skip PLC op */
  while ((insn = *ops) != DDS_OP_RTS)
  {
    switch (DDS_OP (insn))
    {
      case DDS_OP_PLM: {
        const uint32_t *plm_ops = ops + DDS_OP_ADR_PLM (insn);
        uint32_t flags = DDS_PLM_FLAGS (insn);
        if (flags & DDS_OP_FLAG_BASE)
          (void) dds_stream_free_sample_pl (addr, allocator, plm_ops);
        else
          dds_stream_free_sample (addr, allocator, plm_ops);
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

static const uint32_t *stream_free_sample_adr_nonexternal (uint32_t insn, void * __restrict addr, void * __restrict data, const struct dds_cdrstream_allocator * __restrict allocator, const uint32_t * __restrict ops)
{
  assert (DDS_OP (insn) == DDS_OP_ADR);

  switch (DDS_OP_TYPE (insn))
  {
    case DDS_OP_VAL_BLN: case DDS_OP_VAL_1BY: case DDS_OP_VAL_2BY: case DDS_OP_VAL_4BY: case DDS_OP_VAL_8BY: ops += 2; break;
    case DDS_OP_VAL_STR: {
      allocator->free (*((char **) addr));
      *(char **) addr = NULL;
      ops += 2;
      break;
    }
    case DDS_OP_VAL_BST: case DDS_OP_VAL_ENU: ops += 3; break;
    case DDS_OP_VAL_BMK: ops += 4; break;
    case DDS_OP_VAL_SEQ: case DDS_OP_VAL_BSQ: ops = dds_stream_free_sample_seq (addr, allocator, ops, insn); break;
    case DDS_OP_VAL_ARR: ops = dds_stream_free_sample_arr (addr, allocator, ops, insn); break;
    case DDS_OP_VAL_UNI: ops = dds_stream_free_sample_uni (addr, data, allocator, ops, insn); break;
    case DDS_OP_VAL_EXT: {
      const uint32_t *jsr_ops = ops + DDS_OP_ADR_JSR (ops[2]);
      const uint32_t jmp = DDS_OP_ADR_JMP (ops[2]);
      dds_stream_free_sample (addr, allocator, jsr_ops);
      ops += jmp ? jmp : 3;
      break;
    }
    case DDS_OP_VAL_STU:
      abort (); /* op type STU only supported as subtype */
      break;
  }

  return ops;
}

static const uint32_t *stream_free_sample_adr (uint32_t insn, void * __restrict data, const struct dds_cdrstream_allocator * __restrict allocator, const uint32_t * __restrict ops)
{
  assert (DDS_OP (insn) == DDS_OP_ADR);
  if (!op_type_external (insn))
  {
    void *addr = (char *) data + ops[1];
    ops = stream_free_sample_adr_nonexternal (insn, addr, data, allocator, ops);
  }
  else
  {
    void **ext_addr = (void **) ((char *) data + ops[1]);
    void *addr = *ext_addr;
    if (addr == NULL)
    {
      ops = dds_stream_skip_adr (insn, ops);
    }
    else
    {
      ops = stream_free_sample_adr_nonexternal (insn, addr, data, allocator, ops);
      allocator->free (*ext_addr);
      *ext_addr = NULL;
    }
  }
  return ops;
}

void dds_stream_free_sample (void * __restrict data, const struct dds_cdrstream_allocator * __restrict allocator, const uint32_t * __restrict ops)
{
  uint32_t insn;
  while ((insn = *ops) != DDS_OP_RTS)
  {
    switch (DDS_OP (insn))
    {
      case DDS_OP_ADR:
        ops = stream_free_sample_adr (insn, data, allocator, ops);
        break;
      case DDS_OP_JSR:
        dds_stream_free_sample (data, allocator, ops + DDS_OP_JUMP (insn));
        ops++;
        break;
      case DDS_OP_RTS: case DDS_OP_JEQ: case DDS_OP_JEQ4: case DDS_OP_KOF: case DDS_OP_PLM:
        abort ();
        break;
      case DDS_OP_DLC:
        ops++;
        break;
      case DDS_OP_PLC:
        ops = dds_stream_free_sample_pl (data, allocator, ops);
        break;
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

static void dds_stream_extract_key_from_key_prim_op (dds_istream_t * __restrict is, dds_ostream_t * __restrict os, const struct dds_cdrstream_allocator * __restrict allocator, const uint32_t * __restrict ops, uint16_t key_offset_count, const uint32_t * key_offset_insn)
{
  const uint32_t insn = *ops;
  assert ((insn & DDS_OP_FLAG_KEY) && ((DDS_OP (insn)) == DDS_OP_ADR));
  switch (DDS_OP_TYPE (insn))
  {
    case DDS_OP_VAL_BLN:
    case DDS_OP_VAL_1BY: dds_os_put1 (os, allocator, dds_is_get1 (is)); break;
    case DDS_OP_VAL_2BY: dds_os_put2 (os, allocator, dds_is_get2 (is)); break;
    case DDS_OP_VAL_4BY: dds_os_put4 (os, allocator, dds_is_get4 (is)); break;
    case DDS_OP_VAL_8BY: dds_os_put8 (os, allocator, dds_is_get8 (is)); break;
    case DDS_OP_VAL_ENU: case DDS_OP_VAL_BMK:
      switch (DDS_OP_TYPE_SZ (insn))
      {
        case 1: dds_os_put1 (os, allocator, dds_is_get1 (is)); break;
        case 2: dds_os_put2 (os, allocator, dds_is_get2 (is)); break;
        case 4: dds_os_put4 (os, allocator, dds_is_get4 (is)); break;
        case 8: assert (DDS_OP_TYPE(insn) == DDS_OP_VAL_BMK); dds_os_put8 (os, allocator, dds_is_get8 (is)); break;
        default: abort ();
      }
      break;
    case DDS_OP_VAL_STR: case DDS_OP_VAL_BST: {
      uint32_t sz = dds_is_get4 (is);
      dds_os_put4 (os, allocator, sz);
      dds_os_put_bytes (os, allocator, is->m_buffer + is->m_index, sz);
      is->m_index += sz;
      break;
    }
    case DDS_OP_VAL_ARR: {
      const enum dds_stream_typecode subtype = DDS_OP_SUBTYPE (insn);
      uint32_t elem_size, offs = 0, xcdrv = ((struct dds_ostream *)os)->m_xcdr_version;
      if (is_dheader_needed (subtype, xcdrv))
      {
        /* In case of non-primitive element type, reserve space for DHEADER in the
           output stream, and skip the DHEADER in the input */
        dds_os_reserve4 (os, allocator);
        offs = ((struct dds_ostream *)os)->m_index;
        (void) dds_is_get4 (is);
      }
      if (is_primitive_type (subtype))
        elem_size = get_primitive_size (subtype);
      else if (subtype == DDS_OP_VAL_ENU || subtype == DDS_OP_VAL_BMK)
        elem_size = DDS_OP_TYPE_SZ (insn);
      else
        abort ();
      const align_t align = dds_cdr_get_align (os->m_xcdr_version, elem_size);
      const uint32_t num = ops[2];
      dds_cdr_alignto (is, align);
      dds_cdr_alignto_clear_and_resize (os, allocator, align, num * elem_size);
      void * const dst = os->m_buffer + os->m_index;
      dds_is_get_bytes (is, dst, num, elem_size);
      os->m_index += num * elem_size;
      /* set DHEADER */
      if (is_dheader_needed (subtype, xcdrv))
        *((uint32_t *) (((struct dds_ostream *)os)->m_buffer + offs - 4)) = ((struct dds_ostream *)os)->m_index - offs;
      break;
    }
    case DDS_OP_VAL_EXT: {
      assert (key_offset_count > 0);
      const uint32_t *jsr_ops = ops + DDS_OP_ADR_JSR (ops[2]) + *key_offset_insn;
      dds_stream_extract_key_from_key_prim_op (is, os, allocator, jsr_ops, --key_offset_count, ++key_offset_insn);
      break;
    }
    case DDS_OP_VAL_SEQ: case DDS_OP_VAL_BSQ: case DDS_OP_VAL_UNI: case DDS_OP_VAL_STU: {
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

static void dds_stream_extract_keyBE_from_key_prim_op (dds_istream_t * __restrict is, dds_ostreamBE_t * __restrict os, const struct dds_cdrstream_allocator * __restrict allocator, const uint32_t * __restrict ops, uint16_t key_offset_count, const uint32_t * key_offset_insn)
{
  const uint32_t insn = *ops;
  assert ((insn & DDS_OP_FLAG_KEY) && ((DDS_OP (insn)) == DDS_OP_ADR));
  switch (DDS_OP_TYPE (insn))
  {
    case DDS_OP_VAL_BLN:
    case DDS_OP_VAL_1BY: dds_os_put1BE (os, allocator, dds_is_get1 (is)); break;
    case DDS_OP_VAL_2BY: dds_os_put2BE (os, allocator, dds_is_get2 (is)); break;
    case DDS_OP_VAL_4BY: dds_os_put4BE (os, allocator, dds_is_get4 (is)); break;
    case DDS_OP_VAL_8BY: dds_os_put8BE (os, allocator, dds_is_get8 (is)); break;
    case DDS_OP_VAL_ENU: case DDS_OP_VAL_BMK:
      switch (DDS_OP_TYPE_SZ (insn))
      {
        case 1: dds_os_put1BE (os, allocator, dds_is_get1 (is)); break;
        case 2: dds_os_put2BE (os, allocator, dds_is_get2 (is)); break;
        case 4: dds_os_put4BE (os, allocator, dds_is_get4 (is)); break;
        case 8: assert (DDS_OP_TYPE (insn) == DDS_OP_VAL_BMK); dds_os_put8BE (os, allocator, dds_is_get8 (is)); break;
        default: abort ();
      }
      break;
    case DDS_OP_VAL_STR: case DDS_OP_VAL_BST: {
      uint32_t sz = dds_is_get4 (is);
      dds_os_put4BE (os, allocator, sz);
      dds_os_put_bytes (&os->x, allocator, is->m_buffer + is->m_index, sz);
      is->m_index += sz;
      break;
    }
    case DDS_OP_VAL_ARR: {
      const enum dds_stream_typecode subtype = DDS_OP_SUBTYPE (insn);
      uint32_t elem_size, offs = 0, xcdrv = ((struct dds_ostream *)os)->m_xcdr_version;
      if (is_dheader_needed (subtype, xcdrv))
      {
        /* In case of non-primitive element type, reserve space for DHEADER in the
           output stream, and skip the DHEADER in the input */
        dds_os_reserve4BE (os, allocator);
        offs = ((struct dds_ostream *)os)->m_index;
        (void) dds_is_get4 (is);
      }
      if (is_primitive_type (subtype))
        elem_size = get_primitive_size (subtype);
      else if (subtype == DDS_OP_VAL_ENU || subtype == DDS_OP_VAL_BMK)
        elem_size = DDS_OP_TYPE_SZ (insn);
      else
        abort ();
      const align_t align = dds_cdr_get_align (os->x.m_xcdr_version, elem_size);
      const uint32_t num = ops[2];
      dds_cdr_alignto (is, align);
      dds_cdr_alignto_clear_and_resizeBE (os, allocator, align, num * elem_size);
      void const * const src = is->m_buffer + is->m_index;
      void * const dst = os->x.m_buffer + os->x.m_index;
#if DDSRT_ENDIAN == DDSRT_LITTLE_ENDIAN
      dds_stream_swap_copy (dst, src, elem_size, num);
#else
      memcpy (dst, src, num * elem_size);
#endif
      os->x.m_index += num * elem_size;
      is->m_index += num * elem_size;

      /* set DHEADER */
      if (is_dheader_needed (subtype, xcdrv))
        *((uint32_t *) (((struct dds_ostream *)os)->m_buffer + offs - 4)) = ddsrt_toBE4u(((struct dds_ostream *)os)->m_index - offs);
      break;
    }
    case DDS_OP_VAL_EXT: {
      assert (key_offset_count > 0);
      const uint32_t *jsr_ops = ops + DDS_OP_ADR_JSR (ops[2]) + *key_offset_insn;
      dds_stream_extract_keyBE_from_key_prim_op (is, os, allocator, jsr_ops, --key_offset_count, ++key_offset_insn);
      break;
    }
    case DDS_OP_VAL_SEQ: case DDS_OP_VAL_BSQ: case DDS_OP_VAL_UNI: case DDS_OP_VAL_STU: {
      abort ();
      break;
    }
  }
}

static void dds_stream_extract_key_from_data_skip_subtype (dds_istream_t * __restrict is, uint32_t num, uint32_t insn, uint32_t subtype, const uint32_t * __restrict subops)
{
  switch (subtype)
  {
    case DDS_OP_VAL_BLN: case DDS_OP_VAL_1BY: case DDS_OP_VAL_2BY: case DDS_OP_VAL_4BY: case DDS_OP_VAL_8BY: {
      const uint32_t elem_size = get_primitive_size (subtype);
      dds_cdr_alignto (is, dds_cdr_get_align (is->m_xcdr_version, elem_size));
      is->m_index += num * elem_size;
      break;
    }
    case DDS_OP_VAL_ENU: case DDS_OP_VAL_BMK: {
      /* Get size of enumerated type: bitmask in union (JEQ4) is a special case, because the
         bitmask definition is in the serializer instructions after the union case instructions,
         so we need to get the size from subops[0] for a bitmask. Enums are defined inline in
         the union case instructions, so we'll use insn to get the size of an enum type. */
      const uint32_t elem_size = DDS_OP_TYPE_SZ (DDS_OP (insn) == DDS_OP_JEQ4 && subtype == DDS_OP_VAL_BMK ? subops[0] : insn);
      dds_cdr_alignto (is, dds_cdr_get_align (is->m_xcdr_version, elem_size));
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
    case DDS_OP_VAL_SEQ: case DDS_OP_VAL_BSQ: case DDS_OP_VAL_ARR: case DDS_OP_VAL_UNI: case DDS_OP_VAL_STU: {
      uint32_t remain = UINT32_MAX;
      for (uint32_t i = 0; i < num; i++)
        dds_stream_extract_key_from_data1 (is, NULL, NULL, 0, NULL, NULL, NULL, subops, false, false, remain, &remain, NULL, NULL);
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
  const uint32_t insn = *ops;
  assert (DDS_OP_TYPE (insn) == DDS_OP_VAL_ARR);
  const enum dds_stream_typecode subtype = DDS_OP_SUBTYPE (insn);
  const uint32_t num = ops[2];

  // if DHEADER present, use its value to skip array
  if (is_dheader_needed (subtype, is->m_xcdr_version))
  {
    const uint32_t sz = dds_is_get4 (is);
    is->m_index += sz;
  }
  else if (type_has_subtype_or_members (subtype))
    dds_stream_extract_key_from_data_skip_subtype (is, num, insn, subtype, ops + DDS_OP_ADR_JSR (ops[3]));
  else
    dds_stream_extract_key_from_data_skip_subtype (is, num, insn, subtype, NULL);
  return skip_array_insns (insn, ops);
}

static const uint32_t *dds_stream_extract_key_from_data_skip_sequence (dds_istream_t * __restrict is, const uint32_t * __restrict ops)
{
  const uint32_t insn = *ops;
  uint32_t bound_op = seq_is_bounded (DDS_OP_TYPE (insn)) ? 1 : 0;
  const enum dds_stream_typecode subtype = DDS_OP_SUBTYPE (insn);

  // if DHEADER present, use its value to skip sequence
  if (is_dheader_needed (subtype, is->m_xcdr_version))
  {
    const uint32_t sz = dds_is_get4 (is);
    is->m_index += sz;
  }
  else
  {
    const uint32_t num = dds_is_get4 (is);
    if (num > 0)
    {
      if (type_has_subtype_or_members (subtype))
        dds_stream_extract_key_from_data_skip_subtype (is, num, insn, subtype, ops + DDS_OP_ADR_JSR (ops[3 + bound_op]));
      else
        dds_stream_extract_key_from_data_skip_subtype (is, num, insn, subtype, NULL);
    }
  }
  return skip_sequence_insns (insn, ops);
}

static const uint32_t *dds_stream_extract_key_from_data_skip_union (dds_istream_t * __restrict is, const uint32_t * __restrict ops)
{
  const uint32_t insn = *ops;
  assert (DDS_OP_TYPE (insn) == DDS_OP_VAL_UNI);
  const uint32_t disc = read_union_discriminant (is, insn);
  uint32_t const * const jeq_op = find_union_case (ops, disc);
  if (jeq_op)
    dds_stream_extract_key_from_data_skip_subtype (is, 1, jeq_op[0], DDS_JEQ_TYPE (jeq_op[0]), jeq_op + DDS_OP_ADR_JSR (jeq_op[0]));
  return ops + DDS_OP_ADR_JMP (ops[3]);
}

static const uint32_t *dds_stream_extract_key_from_data_skip_adr (dds_istream_t * __restrict is, const uint32_t * __restrict ops, uint32_t type)
{
  switch (type)
  {
    case DDS_OP_VAL_BLN: case DDS_OP_VAL_1BY: case DDS_OP_VAL_2BY: case DDS_OP_VAL_4BY: case DDS_OP_VAL_8BY: case DDS_OP_VAL_STR: case DDS_OP_VAL_BST: case DDS_OP_VAL_ENU: case DDS_OP_VAL_BMK:
      dds_stream_extract_key_from_data_skip_subtype (is, 1, ops[0], type, NULL);
      if (type == DDS_OP_VAL_BST || type == DDS_OP_VAL_ARR || type == DDS_OP_VAL_ENU)
        ops += 3;
      else if (type == DDS_OP_VAL_BMK)
        ops += 4;
      else
        ops += 2;
      break;
    case DDS_OP_VAL_SEQ: case DDS_OP_VAL_BSQ:
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

void dds_stream_read_sample (dds_istream_t * __restrict is, void * __restrict data, const struct dds_cdrstream_allocator * __restrict allocator, const struct dds_cdrstream_desc * __restrict desc)
{
  size_t opt_size = is->m_xcdr_version == DDSI_RTPS_CDR_ENC_VERSION_1 ? desc->opt_size_xcdr1 : desc->opt_size_xcdr2;
  if (opt_size)
  {
    /* Layout of struct & CDR is the same, but sizeof(struct) may include padding at
       the end that is not present in CDR, so we must use type->opt_size_xcdrx to avoid a
       potential out-of-bounds read */
    dds_is_get_bytes (is, data, (uint32_t) opt_size, 1);
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
      dds_stream_free_sample (data, allocator, desc->ops.ops);
      memset (data, 0, desc->size);
    }
    (void) dds_stream_read_impl (is, data, allocator, desc->ops.ops, false);
  }
}

static void dds_stream_read_key_impl (dds_istream_t * __restrict is, char * __restrict sample, const struct dds_cdrstream_allocator * __restrict allocator, const uint32_t * __restrict ops, uint16_t key_offset_count, const uint32_t * key_offset_insn)
{
  void *dst = sample + ops[1];
  uint32_t insn = ops[0];
  assert (insn_key_ok_p (insn));

  if (op_type_external (insn))
    dds_stream_alloc_external (ops, insn, &dst, allocator);

  switch (DDS_OP_TYPE (insn))
  {
    case DDS_OP_VAL_BLN:
    case DDS_OP_VAL_1BY: *((uint8_t *) dst) = dds_is_get1 (is); break;
    case DDS_OP_VAL_2BY: *((uint16_t *) dst) = dds_is_get2 (is); break;
    case DDS_OP_VAL_4BY: *((uint32_t *) dst) = dds_is_get4 (is); break;
    case DDS_OP_VAL_8BY: *((uint64_t *) dst) = dds_is_get8 (is); break;
    case DDS_OP_VAL_ENU:
      switch (DDS_OP_TYPE_SZ (insn))
      {
        case 1: *((uint32_t *) dst) = dds_is_get1 (is); break;
        case 2: *((uint32_t *) dst) = dds_is_get2 (is); break;
        case 4: *((uint32_t *) dst) = dds_is_get4 (is); break;
        default: abort ();
      }
      break;
    case DDS_OP_VAL_BMK:
      switch (DDS_OP_TYPE_SZ (insn))
      {
        case 1: *((uint8_t *) dst) = dds_is_get1 (is); break;
        case 2: *((uint16_t *) dst) = dds_is_get2 (is); break;
        case 4: *((uint32_t *) dst) = dds_is_get4 (is); break;
        case 8: *((uint64_t *) dst) = dds_is_get8 (is); break;
        default: abort ();
      }
      break;
    case DDS_OP_VAL_STR: *((char **) dst) = dds_stream_reuse_string (is, *((char **) dst), allocator); break;
    case DDS_OP_VAL_BST: (void) dds_stream_reuse_string_bound (is, dst, allocator, ops[2], false); break;
    case DDS_OP_VAL_ARR: {
      const enum dds_stream_typecode subtype = DDS_OP_SUBTYPE (insn);
      uint32_t num = ops[2];
      /* In case of non-primitive element type skip the DHEADER in the input */
      if (is_dheader_needed (subtype, is->m_xcdr_version))
        (void) dds_is_get4 (is);
      switch (subtype)
      {
        case DDS_OP_VAL_BLN: case DDS_OP_VAL_1BY: case DDS_OP_VAL_2BY: case DDS_OP_VAL_4BY: case DDS_OP_VAL_8BY:
          dds_is_get_bytes (is, dst, num, get_primitive_size (subtype));
          break;
        case DDS_OP_VAL_ENU:
          switch (DDS_OP_TYPE_SZ (insn))
          {
            case 1:
              for (uint32_t i = 0; i < num; i++)
                ((uint32_t *) dst)[i] = dds_is_get1 (is);
              break;
            case 2:
              for (uint32_t i = 0; i < num; i++)
                ((uint32_t *) dst)[i] = dds_is_get2 (is);
              break;
            case 4:
              dds_is_get_bytes (is, dst, num, 4);
              break;
          }
          break;
        case DDS_OP_VAL_BMK: {
          const uint32_t elem_size = DDS_OP_TYPE_SZ (insn);
          dds_is_get_bytes (is, dst, num, elem_size);
          break;
        }
        default:
          abort ();
      }
      break;
    }
    case DDS_OP_VAL_SEQ: case DDS_OP_VAL_BSQ: case DDS_OP_VAL_UNI: case DDS_OP_VAL_STU: abort (); break;
    case DDS_OP_VAL_EXT:
    {
      assert (key_offset_count > 0);
      const uint32_t *jsr_ops = ops + DDS_OP_ADR_JSR (ops[2]) + *key_offset_insn;
      dds_stream_read_key_impl (is, dst, allocator, jsr_ops, --key_offset_count, ++key_offset_insn);
      break;
    }
  }
}

void dds_stream_read_key (dds_istream_t * __restrict is, char * __restrict sample, const struct dds_cdrstream_allocator * __restrict allocator, const struct dds_cdrstream_desc * __restrict desc)
{
  for (uint32_t i = 0; i < desc->keys.nkeys; i++)
  {
    const uint32_t *op = desc->ops.ops + desc->keys.keys[i].ops_offs;
    switch (DDS_OP (*op))
    {
      case DDS_OP_KOF: {
        uint16_t n_offs = DDS_OP_LENGTH (*op);
        dds_stream_read_key_impl (is, sample, allocator, desc->ops.ops + op[1], --n_offs, op + 2);
        break;
      }
      case DDS_OP_ADR: {
        dds_stream_read_key_impl (is, sample, allocator, op, 0, NULL);
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
#include "dds_cdrstream_keys.part.c"
#undef NAME_BYTE_ORDER_EXT

#if DDSRT_ENDIAN == DDSRT_LITTLE_ENDIAN

static void dds_stream_swap_if_needed_insituBE (void * __restrict vbuf, uint32_t size, uint32_t num)
{
  dds_stream_swap (vbuf, size, num);
}

// Big-endian implementation
#define NAME_BYTE_ORDER_EXT BE
#include "dds_cdrstream_keys.part.c"
#undef NAME_BYTE_ORDER_EXT

#else /* if DDSRT_ENDIAN == DDSRT_LITTLE_ENDIAN */

void dds_stream_write_keyBE (dds_ostreamBE_t * __restrict os, const char * __restrict sample, const struct dds_cdrstream_allocator * __restrict allocator, const struct dds_cdrstream_desc * __restrict desc)
{
  dds_stream_write_key (&os->x, allocator, sample, desc);
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

static bool prtf_enum_bitmask (char * __restrict *buf, size_t * __restrict bufsize, dds_istream_t * __restrict is, uint32_t flags)
{
  switch (DDS_OP_FLAGS_SZ (flags))
  {
    case 1: {
      const uint8_t val = dds_is_get1 (is);
      return prtf (buf, bufsize, "%"PRIu8, val);
    }
    case 2: {
      const uint16_t val = dds_is_get2 (is);
      return prtf (buf, bufsize, "%"PRIu16, val);
    }
    case 4: {
      const uint32_t val = dds_is_get4 (is);
      return prtf (buf, bufsize, "%"PRIu32, val);
    }
    case 8: {
      const uint64_t val = dds_is_get8 (is);
      return prtf (buf, bufsize, "%"PRIu64, val);
    }
    default:
      abort ();
  }
  return false;
}

static bool prtf_simple (char * __restrict *buf, size_t * __restrict bufsize, dds_istream_t * __restrict is, enum dds_stream_typecode type, uint32_t flags)
{
  switch (type)
  {
    case DDS_OP_VAL_BLN: {
      const bool x = dds_is_get1 (is);
      return prtf (buf, bufsize, "%s", x ? "true" : "false");
    }
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
    case DDS_OP_VAL_4BY: {
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
    case DDS_OP_VAL_ENU: case DDS_OP_VAL_BMK:
      return prtf_enum_bitmask (buf, bufsize, is, flags);
    case DDS_OP_VAL_STR: case DDS_OP_VAL_BST: return prtf_str (buf, bufsize, is);
    case DDS_OP_VAL_ARR: case DDS_OP_VAL_SEQ: case DDS_OP_VAL_BSQ: case DDS_OP_VAL_UNI: case DDS_OP_VAL_STU: case DDS_OP_VAL_EXT:
      abort ();
  }
  return false;
}

static bool prtf_simple_array (char * __restrict *buf, size_t * __restrict bufsize, dds_istream_t * __restrict is, uint32_t num, enum dds_stream_typecode type, uint32_t flags)
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
    case DDS_OP_VAL_ENU: case DDS_OP_VAL_BMK:
      for (size_t i = 0; cont && i < num; i++)
      {
        if (i != 0)
          (void) prtf (buf, bufsize, ",");
        cont = prtf_enum_bitmask (buf, bufsize, is, flags);
      }
      break;
    case DDS_OP_VAL_BLN: case DDS_OP_VAL_2BY: case DDS_OP_VAL_4BY: case DDS_OP_VAL_8BY:
    case DDS_OP_VAL_STR: case DDS_OP_VAL_BST:
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

static const uint32_t *dds_stream_print_sample1 (char * __restrict *buf, size_t * __restrict bufsize, dds_istream_t * __restrict is, const uint32_t * __restrict ops, bool add_braces, bool is_mutable_member);

static const uint32_t *prtf_seq (char * __restrict *buf, size_t *bufsize, dds_istream_t * __restrict is, const uint32_t * __restrict ops, uint32_t insn)
{
  const enum dds_stream_typecode subtype = DDS_OP_SUBTYPE (insn);
  uint32_t bound_op = seq_is_bounded (DDS_OP_TYPE (insn)) ? 1 : 0;
  if (is_dheader_needed (subtype, is->m_xcdr_version))
    (void) dds_is_get4 (is);

  const uint32_t num = dds_is_get4 (is);
  if (num == 0)
  {
    (void) prtf (buf, bufsize, "{}");
    return skip_sequence_insns (insn, ops);
  }
  switch (subtype)
  {
    case DDS_OP_VAL_BLN: case DDS_OP_VAL_1BY: case DDS_OP_VAL_2BY: case DDS_OP_VAL_4BY: case DDS_OP_VAL_8BY:
      (void) prtf_simple_array (buf, bufsize, is, num, subtype, DDS_OP_FLAGS (insn));
      return ops + 2 + bound_op;
    case DDS_OP_VAL_STR: case DDS_OP_VAL_BST: case DDS_OP_VAL_ENU: case DDS_OP_VAL_BMK: {
      (void) prtf_simple_array (buf, bufsize, is, num, subtype, DDS_OP_FLAGS (insn));
      const uint32_t *ret_ops = ops + 2 + bound_op;
      if (subtype == DDS_OP_VAL_BMK)
        ret_ops += 2;
      else if (subtype == DDS_OP_VAL_BST || subtype == DDS_OP_VAL_ENU)
        ret_ops++;
      return ret_ops;
    }
    case DDS_OP_VAL_SEQ: case DDS_OP_VAL_BSQ: case DDS_OP_VAL_ARR: case DDS_OP_VAL_UNI: case DDS_OP_VAL_STU: {
      const uint32_t jmp = DDS_OP_ADR_JMP (ops[3 + bound_op]);
      uint32_t const * const jsr_ops = ops + DDS_OP_ADR_JSR (ops[3 + bound_op]);
      bool cont = prtf (buf, bufsize, "{");
      for (uint32_t i = 0; cont && i < num; i++)
      {
        if (i > 0)
          (void) prtf (buf, bufsize, ",");
        cont = dds_stream_print_sample1 (buf, bufsize, is, jsr_ops, subtype == DDS_OP_VAL_STU, false) != NULL;
      }
      (void) prtf (buf, bufsize, "}");
      return ops + (jmp ? jmp : (4 + bound_op)); /* FIXME: why would jmp be 0? */
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
  if (is_dheader_needed (subtype, is->m_xcdr_version))
    (void) dds_is_get4 (is);
  const uint32_t num = ops[2];
  switch (subtype)
  {
    case DDS_OP_VAL_BLN: case DDS_OP_VAL_1BY: case DDS_OP_VAL_2BY: case DDS_OP_VAL_4BY: case DDS_OP_VAL_8BY:
    case DDS_OP_VAL_STR: case DDS_OP_VAL_BST: case DDS_OP_VAL_ENU: case DDS_OP_VAL_BMK: {
      (void) prtf_simple_array (buf, bufsize, is, num, subtype, DDS_OP_FLAGS (insn));
      const uint32_t *ret_ops = ops + 3;
      if (subtype == DDS_OP_VAL_BST || subtype == DDS_OP_VAL_BMK)
        ret_ops += 2;
      else if (subtype == DDS_OP_VAL_ENU)
        ret_ops++;
      return ret_ops;
    }
    case DDS_OP_VAL_SEQ: case DDS_OP_VAL_BSQ: case DDS_OP_VAL_ARR: case DDS_OP_VAL_UNI: case DDS_OP_VAL_STU: {
      const uint32_t *jsr_ops = ops + DDS_OP_ADR_JSR (ops[3]);
      const uint32_t jmp = DDS_OP_ADR_JMP (ops[3]);
      bool cont = prtf (buf, bufsize, "{");
      for (uint32_t i = 0; cont && i < num; i++)
      {
        if (i > 0) (void) prtf (buf, bufsize, ",");
        cont = dds_stream_print_sample1 (buf, bufsize, is, jsr_ops, subtype == DDS_OP_VAL_STU, false) != NULL;
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
  const uint32_t disc = read_union_discriminant (is, insn);
  uint32_t const * const jeq_op = find_union_case (ops, disc);
  (void) prtf (buf, bufsize, "%"PRIu32":", disc);
  ops += DDS_OP_ADR_JMP (ops[3]);
  if (jeq_op)
  {
    const enum dds_stream_typecode valtype = DDS_JEQ_TYPE (jeq_op[0]);
    switch (valtype)
    {
      case DDS_OP_VAL_BLN: case DDS_OP_VAL_1BY: case DDS_OP_VAL_2BY: case DDS_OP_VAL_4BY: case DDS_OP_VAL_8BY: case DDS_OP_VAL_ENU:
      case DDS_OP_VAL_STR: case DDS_OP_VAL_BST:
        (void) prtf_simple (buf, bufsize, is, valtype, DDS_OP_FLAGS (jeq_op[0]));
        break;
      case DDS_OP_VAL_SEQ: case DDS_OP_VAL_BSQ: case DDS_OP_VAL_ARR: case DDS_OP_VAL_UNI: case DDS_OP_VAL_STU: case DDS_OP_VAL_BMK:
        (void) dds_stream_print_sample1 (buf, bufsize, is, jeq_op + DDS_OP_ADR_JSR (jeq_op[0]), valtype == DDS_OP_VAL_STU, false);
        break;
      case DDS_OP_VAL_EXT: {
        abort (); /* not supported, use UNI instead */
        break;
      }
    }
  }
  return ops;
}

static const uint32_t * dds_stream_print_adr (char * __restrict *buf, size_t * __restrict bufsize, uint32_t insn, dds_istream_t * __restrict is, const uint32_t * __restrict ops, bool is_mutable_member)
{
  if (!stream_is_member_present (insn, is, is_mutable_member))
  {
    (void) prtf (buf, bufsize, "NULL");
    return dds_stream_skip_adr (insn, ops);
  }
  switch (DDS_OP_TYPE (insn))
  {
    case DDS_OP_VAL_BLN: case DDS_OP_VAL_1BY: case DDS_OP_VAL_2BY: case DDS_OP_VAL_4BY: case DDS_OP_VAL_8BY:
    case DDS_OP_VAL_STR:
      if (!prtf_simple (buf, bufsize, is, DDS_OP_TYPE (insn), DDS_OP_FLAGS (insn)))
        return NULL;
      ops += 2;
      break;
    case DDS_OP_VAL_BST: case DDS_OP_VAL_ENU: case DDS_OP_VAL_BMK:
      if (!prtf_simple (buf, bufsize, is, DDS_OP_TYPE (insn), DDS_OP_FLAGS (insn)))
        return NULL;
      ops += 3 + (DDS_OP_TYPE (insn) == DDS_OP_VAL_BMK ? 1 : 0);
      break;
    case DDS_OP_VAL_SEQ: case DDS_OP_VAL_BSQ:
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
      /* skip DLC instruction for base type, DHEADER is not in the data for base types */
      if (op_type_base (insn) && jsr_ops[0] == DDS_OP_DLC)
        jsr_ops++;
      if (dds_stream_print_sample1 (buf, bufsize, is, jsr_ops, true, false) == NULL)
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

static const uint32_t *prtf_delimited (char * __restrict *buf, size_t *bufsize, dds_istream_t * __restrict is, const uint32_t * __restrict ops)
{
  uint32_t delimited_sz = dds_is_get4 (is), delimited_offs = is->m_index, insn;
  bool needs_comma = false;
  if (!prtf (buf, bufsize, "dlh:%"PRIu32, delimited_sz))
    return NULL;
  ops++;
  while ((insn = *ops) != DDS_OP_RTS)
  {
    if (needs_comma)
      (void) prtf (buf, bufsize, ",");
    needs_comma = true;
    switch (DDS_OP (insn))
    {
      case DDS_OP_ADR:
        /* skip fields that are not in serialized data for appendable type */
        if ((ops = (is->m_index - delimited_offs < delimited_sz) ? dds_stream_print_adr (buf, bufsize, insn, is, ops, false) : dds_stream_skip_adr (insn, ops)) == NULL)
          return NULL;
        break;
      case DDS_OP_JSR:
        if (dds_stream_print_sample1 (buf, bufsize, is, ops + DDS_OP_JUMP (insn), false, false) == NULL)
          return NULL;
        ops++;
        break;
      case DDS_OP_RTS: case DDS_OP_JEQ: case DDS_OP_JEQ4: case DDS_OP_KOF: case DDS_OP_DLC: case DDS_OP_PLC: case DDS_OP_PLM: {
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

static bool prtf_plm (char * __restrict *buf, size_t *bufsize, dds_istream_t * __restrict is, uint32_t m_id, const uint32_t * __restrict ops)
{
  uint32_t insn, ops_csr = 0;
  bool found = false;
  while (!found && (insn = ops[ops_csr]) != DDS_OP_RTS)
  {
    assert (DDS_OP (insn) == DDS_OP_PLM);
    uint32_t flags = DDS_PLM_FLAGS (insn);
    const uint32_t *plm_ops = ops + ops_csr + DDS_OP_ADR_PLM (insn);
    if (flags & DDS_OP_FLAG_BASE)
    {
      assert (DDS_OP (plm_ops[0]) == DDS_OP_PLC);
      plm_ops++; /* skip PLC to go to first PLM from base type */
      found = prtf_plm (buf, bufsize, is, m_id, plm_ops);
    }
    else if (ops[ops_csr + 1] == m_id)
    {
      (void) dds_stream_print_sample1 (buf, bufsize, is, plm_ops, true, true);
      found = true;
      break;
    }
    ops_csr += 2;
  }
  return found;
}

static const uint32_t *prtf_pl (char * __restrict *buf, size_t *bufsize, dds_istream_t * __restrict is, const uint32_t * __restrict ops)
{
  /* skip PLC op */
  ops++;

  uint32_t pl_sz = dds_is_get4 (is), pl_offs = is->m_index;
  if (!prtf (buf, bufsize, "pl:%"PRIu32, pl_sz))
    return NULL;

  while (is->m_index - pl_offs < pl_sz)
  {
    /* read emheader and next_int */
    uint32_t em_hdr = dds_is_get4 (is);
    uint32_t lc = EMHEADER_LENGTH_CODE (em_hdr), m_id = EMHEADER_MEMBERID (em_hdr), msz;
    if (!prtf (buf, bufsize, ",lc:%"PRIu32",m:%"PRIu32",", lc, m_id))
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
    if (!prtf_plm (buf, bufsize, is, m_id, ops))
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

static const uint32_t * dds_stream_print_sample1 (char * __restrict *buf, size_t * __restrict bufsize, dds_istream_t * __restrict is, const uint32_t * __restrict ops, bool add_braces, bool is_mutable_member)
{
  uint32_t insn;
  bool cont = true;
  bool needs_comma = false;
  if (add_braces)
    (void) prtf (buf, bufsize, "{");
  while (ops && cont && (insn = *ops) != DDS_OP_RTS)
  {
    if (needs_comma)
      (void) prtf (buf, bufsize, ",");
    needs_comma = true;
    switch (DDS_OP (insn))
    {
      case DDS_OP_ADR:
        ops = dds_stream_print_adr (buf, bufsize, insn, is, ops, is_mutable_member);
        break;
      case DDS_OP_JSR:
        cont = dds_stream_print_sample1 (buf, bufsize, is, ops + DDS_OP_JUMP (insn), true, is_mutable_member) != NULL;
        ops++;
        break;
      case DDS_OP_RTS: case DDS_OP_JEQ: case DDS_OP_JEQ4: case DDS_OP_KOF: case DDS_OP_PLM:
        abort ();
        break;
      case DDS_OP_DLC:
        assert (is->m_xcdr_version == DDSI_RTPS_CDR_ENC_VERSION_2);
        ops = prtf_delimited (buf, bufsize, is, ops);
        break;
      case DDS_OP_PLC:
        assert (is->m_xcdr_version == DDSI_RTPS_CDR_ENC_VERSION_2);
        ops = prtf_pl (buf, bufsize, is, ops);
        break;
    }
  }
  if (add_braces)
    (void) prtf (buf, bufsize, "}");
  return ops;
}

size_t dds_stream_print_sample (dds_istream_t * __restrict is, const struct dds_cdrstream_desc * __restrict desc, char * __restrict buf, size_t bufsize)
{
  (void) dds_stream_print_sample1 (&buf, &bufsize, is, desc->ops.ops, true, false);
  return bufsize;
}

static void dds_stream_print_key_impl (dds_istream_t * __restrict is, const uint32_t *ops, uint16_t key_offset_count, const uint32_t * key_offset_insn,
  char * __restrict *buf, size_t * __restrict bufsize, bool *cont)
{
  uint32_t insn = *ops;
  assert (insn_key_ok_p (insn));
  assert (cont);
  switch (DDS_OP_TYPE (insn))
  {
    case DDS_OP_VAL_BLN: case DDS_OP_VAL_1BY: case DDS_OP_VAL_2BY: case DDS_OP_VAL_4BY: case DDS_OP_VAL_8BY: case DDS_OP_VAL_ENU:
    case DDS_OP_VAL_STR: case DDS_OP_VAL_BST: case DDS_OP_VAL_BMK:
      *cont = prtf_simple (buf, bufsize, is, DDS_OP_TYPE (insn), DDS_OP_FLAGS (insn));
      break;
    case DDS_OP_VAL_ARR:
      *cont = prtf_arr (buf, bufsize, is, ops, insn);
      break;
    case DDS_OP_VAL_SEQ: case DDS_OP_VAL_BSQ: case DDS_OP_VAL_UNI: case DDS_OP_VAL_STU:
      abort ();
      break;
    case DDS_OP_VAL_EXT:
      assert (key_offset_count > 0);
      const uint32_t *jsr_ops = ops + DDS_OP_ADR_JSR (ops[2]) + *key_offset_insn;
      dds_stream_print_key_impl (is, jsr_ops, --key_offset_count, ++key_offset_insn, buf, bufsize, cont);
      break;
  }
}

size_t dds_stream_print_key (dds_istream_t * __restrict is, const struct dds_cdrstream_desc * __restrict desc, char * __restrict buf, size_t bufsize)
{
  bool cont = prtf (&buf, &bufsize, ":k:{");
  bool needs_comma = false;
  for (uint32_t i = 0; cont && i < desc->keys.nkeys; i++)
  {
    if (needs_comma)
      (void) prtf (&buf, &bufsize, ",");
    needs_comma = true;
    const uint32_t *op = desc->ops.ops + desc->keys.keys[i].ops_offs;
    switch (DDS_OP (*op))
    {
      case DDS_OP_KOF: {
        uint16_t n_offs = DDS_OP_LENGTH (*op);
        dds_stream_print_key_impl (is, desc->ops.ops + op[1], --n_offs, op + 2, &buf, &bufsize, &cont);
        break;
      }
      case DDS_OP_ADR: {
        dds_stream_print_key_impl (is, op, 0, NULL, &buf, &bufsize, &cont);
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

/* Gets the (minimum) extensibility of the types used for this topic, and returns the XCDR
   version that is required for (de)serializing the type for this topic descriptor */
uint16_t dds_stream_minimum_xcdr_version (const uint32_t * __restrict ops)
{
  uint16_t min_xcdrv = DDSI_RTPS_CDR_ENC_VERSION_1;
  const uint32_t *ops_end = ops;
  dds_stream_countops1 (ops, &ops_end, &min_xcdrv, 0, NULL);
  return min_xcdrv;
}

/* Gets the extensibility of the top-level type for a topic, by inspecting the serializer ops */
bool dds_stream_extensibility (const uint32_t * __restrict ops, enum dds_cdr_type_extensibility *ext)
{
  uint32_t insn;

  assert (ext);
  while ((insn = *ops) != DDS_OP_RTS)
  {
    switch (DDS_OP (insn))
    {
      case DDS_OP_ADR:
        *ext = DDS_CDR_TYPE_EXT_FINAL;
        return true;
      case DDS_OP_JSR:
        if (DDS_OP_JUMP (insn) > 0)
          return dds_stream_extensibility (ops + DDS_OP_JUMP (insn), ext);
        break;
      case DDS_OP_DLC:
        *ext = DDS_CDR_TYPE_EXT_APPENDABLE;
        return true;
      case DDS_OP_PLC:
        *ext = DDS_CDR_TYPE_EXT_MUTABLE;
        return true;
      case DDS_OP_RTS: case DDS_OP_JEQ: case DDS_OP_JEQ4: case DDS_OP_KOF: case DDS_OP_PLM:
        abort ();
        break;
    }
  }
  return false;
}

uint32_t dds_stream_type_nesting_depth (const uint32_t * __restrict ops)
{
  uint32_t nesting_depth = 0;
  const uint32_t *ops_end = ops;
  dds_stream_countops1 (ops, &ops_end, NULL, 0, &nesting_depth);
  return nesting_depth;
}

void dds_cdrstream_desc_fini (struct dds_cdrstream_desc *desc, const struct dds_cdrstream_allocator * __restrict allocator)
{
  if (desc->keys.nkeys > 0 && desc->keys.keys != NULL)
    allocator->free (desc->keys.keys);
  allocator->free (desc->ops.ops);
}

