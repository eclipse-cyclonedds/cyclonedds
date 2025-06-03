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
#include <stdint.h>
#include <wchar.h>

#include "dds/ddsrt/endian.h"
#include "dds/ddsrt/md5.h"
#include "dds/ddsrt/mh3.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/string.h"
#include "dds/ddsrt/static_assert.h"
#include "dds/cdr/dds_cdrstream.h"
#include "dds/ddsc/dds_data_type_properties.h"

typedef struct restrict_ostream_base {
  unsigned char * restrict m_buffer;
  uint32_t m_size;          /* Buffer size */
  uint32_t m_index;         /* Read/write offset from start of buffer */
  uint32_t m_xcdr_version;  /* XCDR version to use for serializing data */
  uint32_t m_align_off;
} restrict_ostream_base_t;

// We memcpy a dds_ostream(|BE|LE)_t * to a restrict_ostream(|BE|LE)_t *, so
// a minimal verification that the memory layout of a dds_ostream is a prefix
// of that of restrict_ostream seems like a good idea.
DDSRT_STATIC_ASSERT(
  sizeof (restrict_ostream_base_t) >= sizeof (dds_ostream_t) &&
  offsetof (restrict_ostream_base_t, m_buffer) == offsetof (dds_ostream_t, m_buffer) &&
  offsetof (restrict_ostream_base_t, m_size) == offsetof (dds_ostream_t, m_size) &&
  offsetof (restrict_ostream_base_t, m_index) == offsetof (dds_ostream_t, m_index) &&
  offsetof (restrict_ostream_base_t, m_xcdr_version) == offsetof (dds_ostream_t, m_xcdr_version));

typedef struct restrict_ostream {
  restrict_ostream_base_t x;
} restrict_ostream_t;

typedef struct restrict_ostreamBE {
  restrict_ostream_base_t x;
} restrict_ostreamBE_t;

typedef struct restrict_ostreamLE {
  restrict_ostream_base_t x;
} restrict_ostreamLE_t;

#define TOKENPASTE(a, b) a ## b
#define TOKENPASTE2(a, b) TOKENPASTE(a, b)
#define TOKENPASTE3(a, b, c) TOKENPASTE2(a, TOKENPASTE2(b, c))

#define NAME_BYTE_ORDER(name) TOKENPASTE2(name, NAME_BYTE_ORDER_EXT)
#define NAME2_BYTE_ORDER(prefix, postfix) TOKENPASTE3(prefix, NAME_BYTE_ORDER_EXT, postfix)
#define DDS_OSTREAM_T TOKENPASTE3(dds_ostream, NAME_BYTE_ORDER_EXT, _t)
#define RESTRICT_OSTREAM_T TOKENPASTE3(restrict_ostream, NAME_BYTE_ORDER_EXT, _t)

#define EMHEADER_FLAG_MASK            0x80000000u
#define EMHEADER_FLAG_MUSTUNDERSTAND  (1u << 31)
#define EMHEADER_LENGTH_CODE_MASK     0x70000000u
#define EMHEADER_LENGTH_CODE(x)       (((x) & EMHEADER_LENGTH_CODE_MASK) >> 28)
#define EMHEADER_MEMBERID_MASK        0x0fffffffu
#define EMHEADER_MEMBERID(x)          ((x) & EMHEADER_MEMBERID_MASK)

#define XCDR1_MAX_ALIGN 8
#define XCDR2_MAX_ALIGN 4

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

#define dds_os_put1BO                                       NAME_BYTE_ORDER(dds_os_put1)
#define dds_os_put2BO                                       NAME_BYTE_ORDER(dds_os_put2)
#define dds_os_put4BO                                       NAME_BYTE_ORDER(dds_os_put4)
#define dds_os_put8BO                                       NAME_BYTE_ORDER(dds_os_put8)
#define dds_os_reserve4BO                                   NAME_BYTE_ORDER(dds_os_reserve4)
#define dds_os_reserve8BO                                   NAME_BYTE_ORDER(dds_os_reserve8)
#define dds_ostreamBO_fini                                  NAME2_BYTE_ORDER(dds_ostream, _fini)
#define dds_stream_write_stringBO                           NAME_BYTE_ORDER(dds_stream_write_string)
#define dds_stream_write_wstringBO                          NAME_BYTE_ORDER(dds_stream_write_wstring)
#define dds_stream_write_wcharBO                            NAME_BYTE_ORDER(dds_stream_write_wchar)
#define dds_stream_write_seqBO                              NAME_BYTE_ORDER(dds_stream_write_seq)
#define dds_stream_write_arrBO                              NAME_BYTE_ORDER(dds_stream_write_arr)
#define dds_stream_write_bool_valueBO                       NAME_BYTE_ORDER(dds_stream_write_bool_value)
#define dds_stream_write_bool_arrBO                         NAME_BYTE_ORDER(dds_stream_write_bool_arr)
#define dds_stream_write_enum_valueBO                       NAME_BYTE_ORDER(dds_stream_write_enum_value)
#define dds_stream_write_enum_arrBO                         NAME_BYTE_ORDER(dds_stream_write_enum_arr)
#define dds_stream_write_bitmask_valueBO                    NAME_BYTE_ORDER(dds_stream_write_bitmask_value)
#define dds_stream_write_bitmask_arrBO                      NAME_BYTE_ORDER(dds_stream_write_bitmask_arr)
#define dds_stream_write_union_discriminantBO               NAME_BYTE_ORDER(dds_stream_write_union_discriminant)
#define dds_stream_write_uniBO                              NAME_BYTE_ORDER(dds_stream_write_uni)
#define dds_stream_writeBO                                  NAME_BYTE_ORDER(dds_stream_write)
#define dds_stream_write_implBO                             NAME_BYTE_ORDER(dds_stream_write_impl)
#define dds_stream_write_paramheaderBO                      NAME_BYTE_ORDER(dds_stream_write_paramheader)
#define dds_stream_write_adrBO                              NAME_BYTE_ORDER(dds_stream_write_adr)
#define dds_stream_write_xcdr2_plBO                         NAME_BYTE_ORDER(dds_stream_write_xcdr2_pl)
#define dds_stream_write_xcdr2_pl_memberlistBO              NAME_BYTE_ORDER(dds_stream_write_xcdr2_pl_memberlist)
#define dds_stream_write_xcdr2_pl_memberBO                  NAME_BYTE_ORDER(dds_stream_write_xcdr2_pl_member)
#define dds_stream_write_delimitedBO                        NAME_BYTE_ORDER(dds_stream_write_delimited)
#define dds_stream_write_keyBO                              NAME_BYTE_ORDER(dds_stream_write_key)
#define dds_stream_write_keyBO_restrict                     NAME2_BYTE_ORDER(dds_stream_write_key, _restrict)
#define dds_stream_write_keyBO_impl                         NAME2_BYTE_ORDER(dds_stream_write_key, _impl)
#define dds_stream_to_BO_insitu                             NAME2_BYTE_ORDER(dds_stream_to_, _insitu)
#define dds_stream_extract_keyBO_from_data_restrict         NAME2_BYTE_ORDER(dds_stream_extract_key, _from_data_restrict)
#define dds_stream_extract_keyBO_from_data                  NAME2_BYTE_ORDER(dds_stream_extract_key, _from_data)
#define dds_stream_extract_keyBO_from_data1                 NAME2_BYTE_ORDER(dds_stream_extract_key, _from_data1)
#define dds_stream_extract_keyBO_from_data_adr              NAME2_BYTE_ORDER(dds_stream_extract_key, _from_data_adr)
#define dds_stream_extract_keyBO_from_key_prim_op           NAME2_BYTE_ORDER(dds_stream_extract_key, _from_key_prim_op)
#define dds_stream_extract_keyBO_from_data_delimited        NAME2_BYTE_ORDER(dds_stream_extract_key, _from_data_delimited)
#define dds_stream_extract_keyBO_from_data_xcdr2_pl         NAME2_BYTE_ORDER(dds_stream_extract_key, _from_data_xcdr2_pl)
#define dds_stream_extract_keyBO_from_data_xcdr2_pl_member  NAME2_BYTE_ORDER(dds_stream_extract_key, _from_data_xcdr2_pl_member)
#define dds_stream_extract_keyBO_from_key_impl              NAME2_BYTE_ORDER(dds_stream_extract_key, _from_key_impl)
#define dds_stream_extract_keyBO_from_key_optimized         NAME2_BYTE_ORDER(dds_stream_extract_key, _from_key_optimized)
#define dds_stream_extract_keyBO_from_key                   NAME2_BYTE_ORDER(dds_stream_extract_key, _from_key)

struct key_props {
  uint32_t sz_xcdrv1;
  uint32_t sz_xcdrv2;
  uint16_t min_xcdrv;
  bool is_appendable;
  bool is_mutable;
  bool is_sequence;
  bool is_array_nonprim;
};

enum cdr_data_kind {
  CDR_KIND_DATA,
  CDR_KIND_KEY
};

/**
 * @brief Indicates if the sample data is initialized
 *
 * While deserializing a key or sample, the sample data state is passed in recursive
 * calls to read functions. This state indicates if the sample data for the current
 * scope is initialized or uninitialized. When the state is uninitialized, the
 * sample data within the current scope may not be read. See also the comment in
 * @ref stream_union_switch_case.
 */
enum sample_data_state {
  SAMPLE_DATA_INITIALIZED,
  SAMPLE_DATA_UNINITIALIZED
};

struct dds_cdrstream_ops_info {
  const uint32_t *toplevel_op;
  const uint32_t *ops_end;
  uint16_t min_xcdrv;
  uint32_t nesting_max;
  dds_data_type_properties_t data_types;
};

const static struct dds_cdrstream_desc_mid_table static_empty_mid_table = { .table = (struct ddsrt_hh *) &ddsrt_hh_empty, .op0 = NULL };

static const uint32_t *dds_stream_skip_adr (uint32_t insn, const uint32_t *ops)
  ddsrt_attribute_warn_unused_result ddsrt_nonnull_all;

static const uint32_t *dds_stream_skip_default (char * restrict data, const struct dds_cdrstream_allocator *allocator, const uint32_t *ops, enum sample_data_state sample_state)
  ddsrt_attribute_warn_unused_result ddsrt_nonnull_all;

static const uint32_t *dds_stream_extract_key_from_data1 (dds_istream_t *is, restrict_ostream_t *os, const struct dds_cdrstream_allocator *allocator, const struct dds_cdrstream_desc_mid_table *mid_table,
  const uint32_t *ops, bool mutable_member, bool mutable_member_or_parent, uint32_t n_keys, uint32_t * restrict keys_remaining)
  ddsrt_nonnull ((1, 3, 4, 5, 9));

#if DDSRT_ENDIAN == DDSRT_LITTLE_ENDIAN
static const uint32_t *dds_stream_extract_keyBE_from_data1 (dds_istream_t *is, restrict_ostreamBE_t *os, const struct dds_cdrstream_allocator *allocator, const struct dds_cdrstream_desc_mid_table *mid_table,
  const uint32_t *ops, bool mutable_member, bool mutable_member_or_parent, uint32_t n_keys, uint32_t * restrict keys_remaining)
  ddsrt_nonnull ((1, 3, 4, 5, 9));
#endif

static const uint32_t *stream_normalize_data_impl (char * restrict data, uint32_t * restrict off, uint32_t size, bool bswap, uint32_t xcdr_version, const struct dds_cdrstream_desc_mid_table *mid_table, const uint32_t *ops, bool is_mutable_member, enum cdr_data_kind cdr_kind)
  ddsrt_attribute_warn_unused_result ddsrt_nonnull_all;

static const uint32_t *dds_stream_read_impl (dds_istream_t *is, char * restrict data, const struct dds_cdrstream_allocator *allocator, const uint32_t *ops, bool is_mutable_member, enum cdr_data_kind cdr_kind, enum sample_data_state sample_state)
  ddsrt_attribute_warn_unused_result ddsrt_nonnull_all;

static const uint32_t *stream_free_sample_adr (uint32_t insn, void * restrict data, const struct dds_cdrstream_allocator *allocator, const uint32_t *ops)
  ddsrt_attribute_warn_unused_result ddsrt_nonnull_all;

static const uint32_t *dds_stream_skip_adr_default (uint32_t insn, char * restrict data, const struct dds_cdrstream_allocator *allocator, const uint32_t *ops, enum sample_data_state sample_state)
  ddsrt_attribute_warn_unused_result ddsrt_nonnull_all;

static const uint32_t *dds_stream_key_size (const uint32_t *ops, struct key_props *k)
  ddsrt_attribute_warn_unused_result ddsrt_nonnull_all;

static const uint32_t *dds_stream_free_sample_uni (char * restrict discaddr, char * restrict baseaddr, const struct dds_cdrstream_allocator *allocator, const uint32_t *ops, uint32_t insn)
  ddsrt_nonnull_all;

static const uint32_t *dds_stream_write_implLE (restrict_ostreamLE_t *os, const struct dds_cdrstream_allocator *allocator, const struct dds_cdrstream_desc_mid_table *mid_table, const char *data, const uint32_t *ops, bool is_mutable_member, enum cdr_data_kind cdr_kind)
  ddsrt_attribute_warn_unused_result ddsrt_nonnull_all;

static const uint32_t *dds_stream_write_implBE (restrict_ostreamBE_t *os, const struct dds_cdrstream_allocator *allocator, const struct dds_cdrstream_desc_mid_table *mid_table, const char *data, const uint32_t *ops, bool is_mutable_member, enum cdr_data_kind cdr_kind)
  ddsrt_attribute_warn_unused_result ddsrt_nonnull_all;

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

ddsrt_nonnull_all
static void dds_ostream_grow (restrict_ostream_base_t *os, const struct dds_cdrstream_allocator *allocator, uint32_t size)
{
  uint32_t needed = size + os->m_index;

  /* Reallocate on 4k boundry */

  uint32_t new_size = (needed & ~(uint32_t)0xfff) + 0x1000;
  uint8_t *old = os->m_buffer;

  os->m_buffer = allocator->realloc (old, new_size);
  os->m_size = new_size;
}

dds_ostream_t dds_ostream_from_buffer (void *buffer, size_t size, uint16_t write_encoding_version)
{
  dds_ostream_t os;
  os.m_buffer = buffer;
  os.m_size = (uint32_t) size;
  os.m_index = 0;
  os.m_xcdr_version = write_encoding_version;
  return os;
}

ddsrt_nonnull_all
static void dds_cdr_resize (restrict_ostream_base_t *os, const struct dds_cdrstream_allocator *allocator, uint32_t l)
{
  if (os->m_size < l + os->m_index)
    dds_ostream_grow (os, allocator, l);
}

void dds_istream_init (dds_istream_t *is, uint32_t size, const void *input, uint32_t xcdr_version)
{
  is->m_buffer = input;
  is->m_size = size;
  is->m_index = 0;
  is->m_xcdr_version = xcdr_version;
}

void dds_ostream_init (dds_ostream_t *os, const struct dds_cdrstream_allocator *allocator, uint32_t size, uint32_t xcdr_version)
{
  os->m_buffer = size ? allocator->malloc (size) : NULL;
  os->m_size = size;
  os->m_index = 0;
  os->m_xcdr_version = xcdr_version;
}

void dds_ostreamLE_init (dds_ostreamLE_t *os, const struct dds_cdrstream_allocator *allocator, uint32_t size, uint32_t xcdr_version)
{
  dds_ostream_init (&os->x, allocator, size, xcdr_version);
}

void dds_ostreamBE_init (dds_ostreamBE_t *os, const struct dds_cdrstream_allocator *allocator, uint32_t size, uint32_t xcdr_version)
{
  dds_ostream_init (&os->x, allocator, size, xcdr_version);
}

void dds_istream_fini (dds_istream_t *is)
{
  (void) is;
}

void dds_ostream_fini (dds_ostream_t *os, const struct dds_cdrstream_allocator *allocator)
{
  if (os->m_size)
    allocator->free (os->m_buffer);
}

void dds_ostreamLE_fini (dds_ostreamLE_t *os, const struct dds_cdrstream_allocator *allocator)
{
  dds_ostream_fini (&os->x, allocator);
}

void dds_ostreamBE_fini (dds_ostreamBE_t *os, const struct dds_cdrstream_allocator *allocator)
{
  dds_ostream_fini (&os->x, allocator);
}

ddsrt_nonnull_all
static void dds_cdr_alignto (dds_istream_t *is, align_t a)
{
  is->m_index = (is->m_index + ALIGN(a) - 1) & ~(ALIGN(a) - 1);
  assert (is->m_index < is->m_size);
}

ddsrt_nonnull_all
static uint32_t dds_cdr_alignto_clear_and_resize_base (restrict_ostream_base_t *os, const struct dds_cdrstream_allocator *allocator, align_t a, uint32_t extra)
{
  const uint32_t m = (os->m_index - os->m_align_off) % ALIGN(a);
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

uint32_t dds_cdr_alignto4_clear_and_resize (dds_ostream_t *os, const struct dds_cdrstream_allocator *allocator, uint32_t xcdr_version)
{
  restrict_ostream_base_t ros;
  memcpy (&ros, os, sizeof (*os));
  ros.m_align_off = 0;
  uint32_t ret = dds_cdr_alignto_clear_and_resize_base (&ros, allocator, dds_cdr_get_align (xcdr_version, 4), 0);
  memcpy (os, &ros, sizeof (*os));
  return ret;
}

ddsrt_nonnull_all
static uint8_t dds_is_get1 (dds_istream_t *is)
{
  assert (is->m_index < is->m_size);
  uint8_t v = *(is->m_buffer + is->m_index);
  is->m_index++;
  return v;
}

ddsrt_nonnull_all
static uint16_t dds_is_get2 (dds_istream_t *is)
{
  dds_cdr_alignto (is, dds_cdr_get_align (is->m_xcdr_version, 2));
  uint16_t v = * ((uint16_t *) (is->m_buffer + is->m_index));
  is->m_index += 2;
  return v;
}

ddsrt_nonnull_all
static uint32_t dds_is_get4 (dds_istream_t *is)
{
  dds_cdr_alignto (is, dds_cdr_get_align (is->m_xcdr_version, 4));
  uint32_t v = * ((uint32_t *) (is->m_buffer + is->m_index));
  is->m_index += 4;
  return v;
}

ddsrt_nonnull_all
static uint32_t dds_is_peek4 (dds_istream_t *is)
{
  dds_cdr_alignto (is, dds_cdr_get_align (is->m_xcdr_version, 4));
  uint32_t v = * ((uint32_t *) (is->m_buffer + is->m_index));
  return v;
}

ddsrt_nonnull_all
static uint64_t dds_is_get8 (dds_istream_t *is)
{
  dds_cdr_alignto (is, dds_cdr_get_align (is->m_xcdr_version, 8));
  size_t off_low = (DDSRT_ENDIAN == DDSRT_LITTLE_ENDIAN) ? 0 : 4, off_high = 4 - off_low;
  uint32_t v_low = * ((uint32_t *) (is->m_buffer + is->m_index + off_low)),
    v_high = * ((uint32_t *) (is->m_buffer + is->m_index + off_high));
  uint64_t v = (uint64_t) v_high << 32 | v_low;
  is->m_index += 8;
  return v;
}

ddsrt_nonnull_all
static void dds_is_get_bytes (dds_istream_t *is, void * restrict b, uint32_t num, uint32_t elem_size)
{
  dds_cdr_alignto (is, dds_cdr_get_align (is->m_xcdr_version, elem_size));
  memcpy (b, is->m_buffer + is->m_index, num * elem_size);
  is->m_index += num * elem_size;
}

ddsrt_nonnull_all
static void dds_os_put1_base (restrict_ostream_base_t *os, const struct dds_cdrstream_allocator *allocator, uint8_t v)
{
  dds_cdr_resize (os, allocator, 1);
  *((uint8_t *) (os->m_buffer + os->m_index)) = v;
  os->m_index += 1;
}

ddsrt_nonnull_all
static void dds_os_put2_base (restrict_ostream_base_t *os, const struct dds_cdrstream_allocator *allocator, uint16_t v)
{
  dds_cdr_alignto_clear_and_resize_base (os, allocator, dds_cdr_get_align (os->m_xcdr_version, 2), 2);
  *((uint16_t *) (os->m_buffer + os->m_index)) = v;
  os->m_index += 2;
}

ddsrt_nonnull_all
static void dds_os_put4_base (restrict_ostream_base_t *os, const struct dds_cdrstream_allocator *allocator, uint32_t v)
{
  dds_cdr_alignto_clear_and_resize_base (os, allocator, dds_cdr_get_align (os->m_xcdr_version, 4), 4);
  *((uint32_t *) (os->m_buffer + os->m_index)) = v;
  os->m_index += 4;
}

ddsrt_nonnull_all
static void dds_os_put8_base (restrict_ostream_base_t *os, const struct dds_cdrstream_allocator *allocator, uint64_t v)
{
  dds_cdr_alignto_clear_and_resize_base (os, allocator, dds_cdr_get_align (os->m_xcdr_version, 8), 8);
  size_t off_low = (DDSRT_ENDIAN == DDSRT_LITTLE_ENDIAN) ? 0 : 4, off_high = 4 - off_low;
  *((uint32_t *) (os->m_buffer + os->m_index + off_low)) = (uint32_t) v;
  *((uint32_t *) (os->m_buffer + os->m_index + off_high)) = (uint32_t) (v >> 32);
  os->m_index += 8;
}

ddsrt_nonnull_all
static uint32_t dds_os_reserve4_base (restrict_ostream_base_t *os, const struct dds_cdrstream_allocator *allocator)
{
  dds_cdr_alignto_clear_and_resize_base (os, allocator, dds_cdr_get_align (os->m_xcdr_version, 4), 4);
  os->m_index += 4;
  return os->m_index;
}

ddsrt_nonnull_all
static uint32_t dds_os_reserve8_base (restrict_ostream_base_t *os, const struct dds_cdrstream_allocator *allocator)
{
  dds_cdr_alignto_clear_and_resize_base (os, allocator, dds_cdr_get_align (os->m_xcdr_version, 8), 8);
  os->m_index += 8;
  return os->m_index;
}

ddsrt_nonnull_all
static void dds_os_put1 (restrict_ostream_t *os, const struct dds_cdrstream_allocator *allocator, uint8_t v)  { dds_os_put1_base (&os->x, allocator, v); }
ddsrt_nonnull_all
static void dds_os_put2 (restrict_ostream_t *os, const struct dds_cdrstream_allocator *allocator, uint16_t v) { dds_os_put2_base (&os->x, allocator, v); }
ddsrt_nonnull_all
static void dds_os_put4 (restrict_ostream_t *os, const struct dds_cdrstream_allocator *allocator, uint32_t v) { dds_os_put4_base (&os->x, allocator, v); }
ddsrt_nonnull_all
static void dds_os_put8 (restrict_ostream_t *os, const struct dds_cdrstream_allocator *allocator, uint64_t v) { dds_os_put8_base (&os->x, allocator, v); }
ddsrt_nonnull_all
static uint32_t dds_os_reserve4 (restrict_ostream_t *os, const struct dds_cdrstream_allocator *allocator) { return dds_os_reserve4_base (&os->x, allocator); }
ddsrt_nonnull_all
static uint32_t dds_os_reserve8 (restrict_ostream_t *os, const struct dds_cdrstream_allocator *allocator) { return dds_os_reserve8_base (&os->x, allocator); }

ddsrt_nonnull_all
static void dds_os_put1LE (restrict_ostreamLE_t *os, const struct dds_cdrstream_allocator *allocator, uint8_t v)  { dds_os_put1_base (&os->x, allocator, v); }
ddsrt_nonnull_all
static void dds_os_put2LE (restrict_ostreamLE_t *os, const struct dds_cdrstream_allocator *allocator, uint16_t v) { dds_os_put2_base (&os->x, allocator, ddsrt_toLE2u (v)); }
ddsrt_nonnull_all
static void dds_os_put4LE (restrict_ostreamLE_t *os, const struct dds_cdrstream_allocator *allocator, uint32_t v) { dds_os_put4_base (&os->x, allocator, ddsrt_toLE4u (v)); }
ddsrt_nonnull_all
static void dds_os_put8LE (restrict_ostreamLE_t *os, const struct dds_cdrstream_allocator *allocator, uint64_t v) { dds_os_put8_base (&os->x, allocator, ddsrt_toLE8u (v)); }
ddsrt_nonnull_all
static uint32_t dds_os_reserve4LE (restrict_ostreamLE_t *os, const struct dds_cdrstream_allocator *allocator) { return dds_os_reserve4_base (&os->x, allocator); }
ddsrt_nonnull_all
static uint32_t dds_os_reserve8LE (restrict_ostreamLE_t *os, const struct dds_cdrstream_allocator *allocator) { return dds_os_reserve8_base (&os->x, allocator); }

ddsrt_nonnull_all
static void dds_os_put1BE (restrict_ostreamBE_t *os, const struct dds_cdrstream_allocator *allocator, uint8_t v)  { dds_os_put1_base (&os->x, allocator, v); }
ddsrt_nonnull_all
static void dds_os_put2BE (restrict_ostreamBE_t *os, const struct dds_cdrstream_allocator *allocator, uint16_t v) { dds_os_put2_base (&os->x, allocator, ddsrt_toBE2u (v)); }
ddsrt_nonnull_all
static void dds_os_put4BE (restrict_ostreamBE_t *os, const struct dds_cdrstream_allocator *allocator, uint32_t v) { dds_os_put4_base (&os->x, allocator, ddsrt_toBE4u (v)); }
ddsrt_nonnull_all
static void dds_os_put8BE (restrict_ostreamBE_t *os, const struct dds_cdrstream_allocator *allocator, uint64_t v) { dds_os_put8_base (&os->x, allocator, ddsrt_toBE8u (v)); }
ddsrt_nonnull_all
static uint32_t dds_os_reserve4BE (restrict_ostreamBE_t *os, const struct dds_cdrstream_allocator *allocator) { return dds_os_reserve4_base (&os->x, allocator); }
ddsrt_nonnull_all
static uint32_t dds_os_reserve8BE (restrict_ostreamBE_t *os, const struct dds_cdrstream_allocator *allocator) { return dds_os_reserve8_base (&os->x, allocator); }

ddsrt_nonnull_all
static void dds_stream_swap (void *vbuf, uint32_t size, uint32_t num)
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

ddsrt_nonnull_all
static void dds_os_put_bytes_base (restrict_ostream_base_t *os, const struct dds_cdrstream_allocator *allocator, const void *b, uint32_t l)
{
  dds_cdr_resize (os, allocator, l);
  memcpy (os->m_buffer + os->m_index, b, l);
  os->m_index += l;
}

ddsrt_nonnull ((1, 2, 3))
static void dds_os_put_bytes_aligned_base (restrict_ostream_base_t *os, const struct dds_cdrstream_allocator *allocator, const void *data, uint32_t num, uint32_t elem_sz, align_t cdr_align, void **dst)
{
  const uint32_t sz = num * elem_sz;
  dds_cdr_alignto_clear_and_resize_base (os, allocator, cdr_align, sz);
  if (dst)
    *dst = os->m_buffer + os->m_index;
  memcpy (os->m_buffer + os->m_index, data, sz);
  os->m_index += sz;
}

static inline bool is_primitive_type (enum dds_stream_typecode type)
{
  return type <= DDS_OP_VAL_8BY || type == DDS_OP_VAL_BLN || type == DDS_OP_VAL_WCHAR;
}

static inline bool is_primitive_or_enum_type (enum dds_stream_typecode type)
{
  return is_primitive_type (type) || type == DDS_OP_VAL_ENU;
}

static inline bool is_dheader_needed (enum dds_stream_typecode type, uint32_t xcdrv)
{
  return !is_primitive_type (type) && xcdrv == DDSI_RTPS_CDR_ENC_VERSION_2;
}

static uint32_t get_primitive_size (enum dds_stream_typecode type)
{
  DDSRT_STATIC_ASSERT (DDS_OP_VAL_1BY == 1 && DDS_OP_VAL_2BY == 2 && DDS_OP_VAL_4BY == 3 && DDS_OP_VAL_8BY == 4);
  assert (is_primitive_type (type));
  if (type <= DDS_OP_VAL_8BY)
    return (uint32_t)1 << ((uint32_t) type - 1);
  else if (type == DDS_OP_VAL_BLN)
    return 1;
  else if (type == DDS_OP_VAL_WCHAR)
    return 2;
  else
    abort ();
}

ddsrt_nonnull_all
static uint32_t get_collection_elem_size (uint32_t insn, const uint32_t *ops)
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
    case DDS_OP_VAL_WSTR:
      return sizeof (wchar_t *);
    case DDS_OP_VAL_WCHAR:
      return sizeof (wchar_t);
    case DDS_OP_VAL_BST: case DDS_OP_VAL_BWSTR:
    case DDS_OP_VAL_SEQ: case DDS_OP_VAL_BSQ: case DDS_OP_VAL_ARR: case DDS_OP_VAL_UNI: case DDS_OP_VAL_STU:
      if (DDS_OP_TYPE (insn) == DDS_OP_VAL_ARR)
        return ops[4];
      break;
    case DDS_OP_VAL_EXT:
      break;
  }
  abort ();
  return 0u;
}

ddsrt_nonnull_all
static uint32_t get_adr_type_size (uint32_t insn, const uint32_t *ops)
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
    case DDS_OP_VAL_WSTR:
      sz = sizeof (wchar_t *);
      break;
    case DDS_OP_VAL_WCHAR:
      sz = sizeof (wchar_t);
      break;
    case DDS_OP_VAL_BST: case DDS_OP_VAL_BWSTR:
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

ddsrt_nonnull_all
static uint32_t get_jeq4_type_size (const enum dds_stream_typecode valtype, const uint32_t *jeq_op)
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
    case DDS_OP_VAL_WSTR:
      sz = sizeof (wchar_t *);
      break;
    case DDS_OP_VAL_WCHAR:
      sz = sizeof (wchar_t);
      break;
    case DDS_OP_VAL_BMK:
    case DDS_OP_VAL_BST: case DDS_OP_VAL_BWSTR:
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

ddsrt_nonnull_all
static inline bool check_optimize_impl (uint32_t xcdr_version, const uint32_t *ops, uint32_t size, uint32_t num, uint32_t *off, uint32_t member_offs)
{
  align_t cdr_align = dds_cdr_get_align (xcdr_version, size);
  if (*off % ALIGN(cdr_align))
    *off += ALIGN(cdr_align) - (*off % ALIGN(cdr_align));
  if (member_offs + ops[1] != *off)
    return false;
  *off += num * size;
  return true;
}

ddsrt_nonnull_all
static uint32_t dds_stream_check_optimize1 (const struct dds_cdrstream_desc *desc, uint32_t xcdr_version, const uint32_t *ops, uint32_t off, uint32_t member_offs)
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
      case DDS_OP_VAL_BLN:
        // rejecting "memcpy" if there's a boolean, because we want to guarantee true comes out as 1, not something != 0
        return 0;
      case DDS_OP_VAL_1BY: case DDS_OP_VAL_2BY: case DDS_OP_VAL_4BY: case DDS_OP_VAL_8BY:
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
          case DDS_OP_VAL_BLN:
            // rejecting "memcpy" if there's a boolean, because we want to guarantee true comes out as 1, not something != 0
            return 0;
          case DDS_OP_VAL_1BY: case DDS_OP_VAL_2BY: case DDS_OP_VAL_4BY: case DDS_OP_VAL_8BY:
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
      case DDS_OP_VAL_WSTR:
      case DDS_OP_VAL_BWSTR:
      case DDS_OP_VAL_WCHAR:
      case DDS_OP_VAL_STU:
      case DDS_OP_VAL_UNI:
        return 0;
    }
  }
  return off;
#undef ALLOW_ENUM
}

ddsrt_nonnull_all
size_t dds_stream_check_optimize (const struct dds_cdrstream_desc *desc, uint32_t xcdr_version)
{
  size_t opt_size = dds_stream_check_optimize1 (desc, xcdr_version, desc->ops.ops, 0, 0);
  // off < desc can occur if desc->size includes "trailing" padding
  assert (opt_size <= desc->size);
  return opt_size;
}

ddsrt_nonnull_all
static void dds_stream_get_ops_info1 (const uint32_t *ops, uint32_t nestc, struct dds_cdrstream_ops_info *info);

ddsrt_nonnull_all
static const uint32_t *dds_stream_get_ops_info_seq (const uint32_t *ops, uint32_t insn, uint32_t nestc, struct dds_cdrstream_ops_info *info)
{
  uint32_t bound_op = seq_is_bounded (DDS_OP_TYPE (insn)) ? 1 : 0;
  const enum dds_stream_typecode subtype = DDS_OP_SUBTYPE (insn);
  switch (subtype)
  {
    case DDS_OP_VAL_BLN: case DDS_OP_VAL_1BY: case DDS_OP_VAL_2BY: case DDS_OP_VAL_4BY: case DDS_OP_VAL_8BY:
      ops += 2 + bound_op;
      break;
    case DDS_OP_VAL_STR:
      ops += 2 + bound_op;
      info->data_types |= DDS_DATA_TYPE_CONTAINS_STRING;
      break;
    case DDS_OP_VAL_BST:
      ops += 3 + bound_op;
      info->data_types |= DDS_DATA_TYPE_CONTAINS_BSTRING;
      break;
    case DDS_OP_VAL_WSTR:
      ops += 2 + bound_op;
      info->data_types |= DDS_DATA_TYPE_CONTAINS_WSTRING;
      break;
    case DDS_OP_VAL_BWSTR:
      ops += 3 + bound_op;
      info->data_types |= DDS_DATA_TYPE_CONTAINS_BWSTRING;
      break;
    case DDS_OP_VAL_WCHAR:
      ops += 2 + bound_op;
      info->data_types |= DDS_DATA_TYPE_CONTAINS_WCHAR;
      break;
    case DDS_OP_VAL_ENU:
      ops += 3 + bound_op;
      info->data_types |= DDS_DATA_TYPE_CONTAINS_ENUM;
      break;
    case DDS_OP_VAL_BMK:
      ops += 4 + bound_op;
      info->data_types |= DDS_DATA_TYPE_CONTAINS_BITMASK;
      break;
    case DDS_OP_VAL_SEQ: case DDS_OP_VAL_BSQ: case DDS_OP_VAL_ARR: case DDS_OP_VAL_UNI: case DDS_OP_VAL_STU: {
      const uint32_t jmp = DDS_OP_ADR_JMP (ops[3 + bound_op]);
      uint32_t const * const jsr_ops = ops + DDS_OP_ADR_JSR (ops[3 + bound_op]);
      if (ops + 4 + bound_op > info->ops_end)
        info->ops_end = ops + 4 + bound_op;
      if (DDS_OP_ADR_JSR (ops[3 + bound_op]) > 0)
        dds_stream_get_ops_info1 (jsr_ops, nestc + (subtype == DDS_OP_VAL_UNI || subtype == DDS_OP_VAL_STU ? 1 : 0), info);
      ops += (jmp ? jmp : (4 + bound_op)); /* FIXME: why would jmp be 0? */
      break;
    }
    case DDS_OP_VAL_EXT:
      abort (); // not allowed
      break;
  }
  if (ops > info->ops_end)
    info->ops_end = ops;
  return ops;
}

ddsrt_nonnull_all
static const uint32_t *dds_stream_get_ops_info_arr (const uint32_t *ops, uint32_t insn, uint32_t nestc, struct dds_cdrstream_ops_info *info)
{
  const enum dds_stream_typecode subtype = DDS_OP_SUBTYPE (insn);
  switch (subtype)
  {
    case DDS_OP_VAL_BLN: case DDS_OP_VAL_1BY: case DDS_OP_VAL_2BY: case DDS_OP_VAL_4BY: case DDS_OP_VAL_8BY:
      ops += 3;
      break;
    case DDS_OP_VAL_STR:
      ops += 3;
      info->data_types |= DDS_DATA_TYPE_CONTAINS_STRING;
      break;
    case DDS_OP_VAL_WSTR:
      ops += 3;
      info->data_types |= DDS_DATA_TYPE_CONTAINS_WSTRING;
      break;
    case DDS_OP_VAL_WCHAR:
      ops += 3;
      info->data_types |= DDS_DATA_TYPE_CONTAINS_WCHAR;
      break;
    case DDS_OP_VAL_ENU:
      ops += 4;
      info->data_types |= DDS_DATA_TYPE_CONTAINS_ENUM;
      break;
    case DDS_OP_VAL_BST:
      ops += 5;
      info->data_types |= DDS_DATA_TYPE_CONTAINS_BSTRING;
      break;
    case DDS_OP_VAL_BWSTR:
      ops += 5;
      info->data_types |= DDS_DATA_TYPE_CONTAINS_BWSTRING;
      break;
    case DDS_OP_VAL_BMK:
      ops += 5;
      info->data_types |= DDS_DATA_TYPE_CONTAINS_BITMASK;
      break;
    case DDS_OP_VAL_SEQ: case DDS_OP_VAL_BSQ: case DDS_OP_VAL_ARR: case DDS_OP_VAL_UNI: case DDS_OP_VAL_STU: {
      const uint32_t jmp = DDS_OP_ADR_JMP (ops[3]);
      const uint32_t *jsr_ops = ops + DDS_OP_ADR_JSR (ops[3]);
      if (ops + 5 > info->ops_end)
        info->ops_end = ops + 5;
      if (DDS_OP_ADR_JSR (ops[3]) > 0)
        dds_stream_get_ops_info1 (jsr_ops, nestc + (subtype == DDS_OP_VAL_UNI || subtype == DDS_OP_VAL_STU ? 1 : 0), info);
      ops += (jmp ? jmp : 5);
      break;
    }
    case DDS_OP_VAL_EXT:
      abort (); // not allowed
      break;
  }
  if (ops > info->ops_end)
    info->ops_end = ops;
  return ops;
}

ddsrt_nonnull_all
static const uint32_t *dds_stream_get_ops_info_uni (const uint32_t *ops, uint32_t nestc, struct dds_cdrstream_ops_info *info)
{
  enum dds_stream_typecode disc_type = DDS_OP_SUBTYPE (ops[0]);
  if (disc_type == DDS_OP_VAL_ENU)
    info->data_types |= DDS_DATA_TYPE_CONTAINS_ENUM;

  const uint32_t numcases = ops[2];
  const uint32_t *jeq_op = ops + DDS_OP_ADR_JSR (ops[3]);
  for (uint32_t i = 0; i < numcases; i++)
  {
    const enum dds_stream_typecode valtype = DDS_JEQ_TYPE (jeq_op[0]);
    if (op_type_external (jeq_op[0]) && valtype != DDS_OP_VAL_STR && valtype != DDS_OP_VAL_WSTR)
      info->data_types |= DDS_DATA_TYPE_CONTAINS_EXTERNAL;
    switch (valtype)
    {
      case DDS_OP_VAL_BLN: case DDS_OP_VAL_1BY: case DDS_OP_VAL_2BY: case DDS_OP_VAL_4BY: case DDS_OP_VAL_8BY:
        break;
      case DDS_OP_VAL_STR:
        info->data_types |= DDS_DATA_TYPE_CONTAINS_STRING;
        break;
      case DDS_OP_VAL_WSTR:
        info->data_types |= DDS_DATA_TYPE_CONTAINS_WSTRING;
        break;
      case DDS_OP_VAL_WCHAR:
        info->data_types |= DDS_DATA_TYPE_CONTAINS_WCHAR;
        break;
      case DDS_OP_VAL_ENU:
        info->data_types |= DDS_DATA_TYPE_CONTAINS_ENUM;
        break;
      case DDS_OP_VAL_STU:
        info->data_types |= DDS_DATA_TYPE_CONTAINS_STRUCT;
        /* fall-through */
      case DDS_OP_VAL_BST: case DDS_OP_VAL_BWSTR:
      case DDS_OP_VAL_SEQ: case DDS_OP_VAL_BSQ: case DDS_OP_VAL_ARR: case DDS_OP_VAL_UNI: case DDS_OP_VAL_BMK:
        if (DDS_OP_ADR_JSR (jeq_op[0]) > 0)
          dds_stream_get_ops_info1 (jeq_op + DDS_OP_ADR_JSR (jeq_op[0]), nestc + (valtype == DDS_OP_VAL_UNI || valtype == DDS_OP_VAL_STU ? 1 : 0), info);
        break;
      case DDS_OP_VAL_EXT:
        abort (); // not allowed
        break;
    }
    jeq_op += (DDS_OP (jeq_op[0]) == DDS_OP_JEQ) ? 3 : 4;
  }
  if (jeq_op > info->ops_end)
    info->ops_end = jeq_op;
  ops += DDS_OP_ADR_JMP (ops[3]);
  if (ops > info->ops_end)
    info->ops_end = ops;
  return ops;
}

ddsrt_nonnull_all
static const uint32_t *dds_stream_get_ops_info_xcdr2_pl (const uint32_t *ops, uint32_t nestc, struct dds_cdrstream_ops_info *info)
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
          (void) dds_stream_get_ops_info_xcdr2_pl (plm_ops, nestc, info);
        else
          dds_stream_get_ops_info1 (plm_ops, nestc, info);
        ops += 2;
        break;
      }
      default:
        abort (); /* only list of (PLM, member-id) supported */
        break;
    }
  }
  if (ops > info->ops_end)
    info->ops_end = ops;
  return ops;
}

ddsrt_nonnull_all
static void dds_stream_get_ops_info1 (const uint32_t *ops, uint32_t nestc, struct dds_cdrstream_ops_info *info)
{
  uint32_t insn;
  if (info->nesting_max < nestc)
    info->nesting_max = nestc;
  while ((insn = *ops) != DDS_OP_RTS)
  {
    switch (DDS_OP (insn))
    {
      case DDS_OP_ADR: {
        if (info->toplevel_op == NULL)
        {
          info->toplevel_op = ops;
          if (DDS_OP_TYPE (insn) != DDS_OP_VAL_UNI)
            info->data_types |= DDS_DATA_TYPE_CONTAINS_STRUCT;
        }
        if ((insn & DDS_OP_FLAG_KEY) && nestc == 0)
          info->data_types |= DDS_DATA_TYPE_CONTAINS_KEY;
        if (op_type_optional (insn))
          info->data_types |= DDS_DATA_TYPE_CONTAINS_OPTIONAL;
        if (op_type_external (insn) && DDS_OP_TYPE (insn) != DDS_OP_VAL_STR && DDS_OP_TYPE (insn) != DDS_OP_VAL_WSTR)
          info->data_types |= DDS_DATA_TYPE_CONTAINS_EXTERNAL;
        switch (DDS_OP_TYPE (insn))
        {
          case DDS_OP_VAL_BLN: case DDS_OP_VAL_1BY: case DDS_OP_VAL_2BY: case DDS_OP_VAL_4BY: case DDS_OP_VAL_8BY:
            ops += 2;
            break;
          case DDS_OP_VAL_STR:
            ops += 2;
            info->data_types |= DDS_DATA_TYPE_CONTAINS_STRING;
            break;
          case DDS_OP_VAL_WSTR:
            ops += 2;
            info->data_types |= DDS_DATA_TYPE_CONTAINS_WSTRING;
            break;
          case DDS_OP_VAL_WCHAR:
            ops += 2;
            info->data_types |= DDS_DATA_TYPE_CONTAINS_WCHAR;
            break;
          case DDS_OP_VAL_BST:
            ops += 3;
            info->data_types |= DDS_DATA_TYPE_CONTAINS_BSTRING;
            break;
          case DDS_OP_VAL_BWSTR:
            ops += 3;
            info->data_types |= DDS_DATA_TYPE_CONTAINS_BWSTRING;
            break;
          case DDS_OP_VAL_ENU:
            ops += 3;
            info->data_types |= DDS_DATA_TYPE_CONTAINS_ENUM;
            break;
          case DDS_OP_VAL_BMK:
            ops += 4;
            info->data_types |= DDS_DATA_TYPE_CONTAINS_BITMASK;
            break;
          case DDS_OP_VAL_SEQ:
            ops = dds_stream_get_ops_info_seq (ops, insn, nestc, info);
            info->data_types |= DDS_DATA_TYPE_CONTAINS_SEQUENCE;
            break;
          case DDS_OP_VAL_BSQ:
            ops = dds_stream_get_ops_info_seq (ops, insn, nestc, info);
            info->data_types |= DDS_DATA_TYPE_CONTAINS_BSEQUENCE;
            break;
          case DDS_OP_VAL_ARR:
            ops = dds_stream_get_ops_info_arr (ops, insn, nestc, info);
            info->data_types |= DDS_DATA_TYPE_CONTAINS_ARRAY;
            break;
          case DDS_OP_VAL_UNI:
            ops = dds_stream_get_ops_info_uni (ops, nestc, info);
            info->data_types |= DDS_DATA_TYPE_CONTAINS_UNION;
            break;
          case DDS_OP_VAL_EXT: {
            const uint32_t *jsr_ops = ops + DDS_OP_ADR_JSR (ops[2]);
            const uint32_t jmp = DDS_OP_ADR_JMP (ops[2]);
            if (DDS_OP_ADR_JSR (ops[2]) > 0)
              dds_stream_get_ops_info1 (jsr_ops, nestc + 1, info);
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
          dds_stream_get_ops_info1 (ops + DDS_OP_JUMP (insn), nestc, info);
        ops++;
        break;
      }
      case DDS_OP_RTS: case DDS_OP_JEQ: case DDS_OP_JEQ4: case DDS_OP_KOF: case DDS_OP_PLM: case DDS_OP_MID: {
        abort ();
        break;
      }
      case DDS_OP_DLC: {
        info->min_xcdrv = DDSI_RTPS_CDR_ENC_VERSION_2;
        ops++;
        break;
      }
      case DDS_OP_PLC: {
        info->min_xcdrv = DDSI_RTPS_CDR_ENC_VERSION_2;
        ops = dds_stream_get_ops_info_xcdr2_pl (ops, nestc, info);
        break;
      }
    }
  }
  ++ops; /* skip RTS op */
  if (ops > info->ops_end)
    info->ops_end = ops;
}

ddsrt_nonnull_all
static void dds_stream_countops_keyoffset (const uint32_t *ops, const dds_key_descriptor_t *key, const uint32_t **ops_end)
{
  assert (key);
  assert (*ops_end);
  if (key->m_offset >= (uint32_t) (*ops_end - ops))
  {
    assert (DDS_OP (ops[key->m_offset]) == DDS_OP_KOF);
    *ops_end = ops + key->m_offset + 1 + DDS_OP_LENGTH (ops[key->m_offset]);
  }
}

ddsrt_nonnull_all
static void dds_stream_get_ops_info (const uint32_t *ops, struct dds_cdrstream_ops_info *info)
{
  info->toplevel_op = NULL;
  info->ops_end = ops;
  info->min_xcdrv = DDSI_RTPS_CDR_ENC_VERSION_1;
  info->nesting_max = 0;
  info->data_types = 0ull;
  dds_stream_get_ops_info1 (ops, 0, info);
}

ddsrt_attribute_warn_unused_result ddsrt_nonnull_all
static char *dds_stream_reuse_string_bound (dds_istream_t *is, char * restrict str, const uint32_t size)
{
  const uint32_t length = dds_is_get4 (is);
  const void *src = is->m_buffer + is->m_index;
  /* FIXME: validation now rejects data containing an oversize bounded string,
     so this check is superfluous, but perhaps rejecting such a sample is the
     wrong thing to do */
  memcpy (str, src, length > size ? size : length);
  if (length > size)
    str[size - 1] = '\0';
  is->m_index += length;
  return str;
}

ddsrt_attribute_warn_unused_result ddsrt_nonnull ((1, 3))
static char *dds_stream_reuse_string (dds_istream_t *is, char * restrict str, const struct dds_cdrstream_allocator *allocator, enum sample_data_state sample_state)
{
  const uint32_t length = dds_is_get4 (is);
  const void *src = is->m_buffer + is->m_index;
  is->m_index += length;
  if (sample_state == SAMPLE_DATA_INITIALIZED && str != NULL)
  {
    if (length == 1 && str[0] == '\0')
      return str;
    allocator->free (str);
  }
  str = allocator->malloc (length);
  memcpy (str, src, length);
  return str;
}

ddsrt_attribute_warn_unused_result ddsrt_nonnull ((2))
static char *dds_stream_reuse_string_empty (char * restrict str, const struct dds_cdrstream_allocator *allocator, enum sample_data_state sample_state)
{
  if (sample_state == SAMPLE_DATA_INITIALIZED && str != NULL)
  {
    if (str[0] == '\0')
      return str;
    allocator->free (str);
  }
  str = allocator->malloc (1);
  str[0] = '\0';
  return str;
}

ddsrt_nonnull_all
static size_t wstring_utf16_len (const wchar_t *str)
{
  // assume sizeof(wchar_t) = uint16_t or uint32_t
  // assume uint16_t implies UTF-16 and uint32_t implies UTF-32
  // I think that should work for Unix and Windows
  DDSRT_STATIC_ASSERT (sizeof (wchar_t) == 2 || sizeof (wchar_t) == 4);
  size_t n = 0;
  for (; *str != L'\0'; n++, str++)
    if ((uint32_t) *str >= 0x10000) // necessarily false if wchar_t is 16 bits
      n++;
  return n;
}

ddsrt_nonnull_all
static void wstring_from_utf16 (wchar_t * restrict dst, size_t dstlen, const uint16_t *src, size_t srclen)
{
  // src, srclen without terminating 0
  // dst, dstlen including terminating 0
  assert (dstlen > 0);
  if (sizeof (wchar_t) == 2)
  {
    memcpy (dst, src, 2 * (dstlen < srclen ? dstlen : srclen));
    dst[dstlen - 1] = L'\0';
  }
  else
  {
    // resulting string may end up shorter than srclen because of surrogate pairs in input
    uint16_t w1 = 0;
    uint32_t di = 0;
    for (uint32_t i = 0; i < srclen && di < dstlen - 1; i++)
    {
      if (src[i] < 0xd800 || src[i] >= 0xe000)
        dst[di++] = (wchar_t) src[i];
      else if (src[i] >= 0xd800 && src[i] < 0xdc00)
      {
        // should not have buffered a low surrogate, but we don't care
        w1 = src[i];
      }
      else
      {
        // high surrogate must have been seen, but ... whatever
        uint32_t u1 = ((uint32_t) (w1 - 0xd800) << 10) | (uint32_t) (src[i] - 0xdc00);
        dst[di++] = (wchar_t) (u1 + 0x10000);
        w1 = 0;
      }
    }
    dst[di] = L'\0';
  }
}

ddsrt_nonnull_all
static wchar_t *dds_stream_reuse_wstring_bound (dds_istream_t *is, wchar_t * restrict str, const uint32_t size)
{
  const uint32_t cdrsize = dds_is_get4 (is);
  const uint16_t *src = (const uint16_t *) (is->m_buffer + is->m_index);
  is->m_index += cdrsize;
  wstring_from_utf16 (str, size, src, cdrsize / 2);
  return str;
}

ddsrt_attribute_warn_unused_result ddsrt_nonnull ((1, 3))
static wchar_t *dds_stream_reuse_wstring (dds_istream_t *is, wchar_t * restrict str, const struct dds_cdrstream_allocator *allocator, enum sample_data_state sample_state)
{
  const uint32_t cdrsize = dds_is_get4 (is);
  const uint16_t *src = (const uint16_t *) (is->m_buffer + is->m_index);
  is->m_index += cdrsize;
  if (sample_state == SAMPLE_DATA_INITIALIZED && str != NULL)
  {
    if (cdrsize == 0 && str[0] == L'\0')
      return str;
    allocator->free (str);
  }
  // if there are surrogates in the input and wchar_t is UTF-32, then we overallocate a bit
  str = allocator->malloc (sizeof (wchar_t) * (cdrsize / 2 + 1));
  wstring_from_utf16 (str, cdrsize / 2 + 1, src, cdrsize / 2);
  return str;
}

ddsrt_attribute_warn_unused_result ddsrt_nonnull ((2))
static wchar_t *dds_stream_reuse_wstring_empty (wchar_t * restrict str, const struct dds_cdrstream_allocator *allocator, enum sample_data_state sample_state)
{
  if (sample_state == SAMPLE_DATA_INITIALIZED && str != NULL)
  {
    if (str[0] == L'\0')
      return str;
    allocator->free (str);
  }
  str = allocator->malloc (sizeof (wchar_t));
  str[0] = L'\0';
  return str;
}

ddsrt_nonnull_all
static void dds_stream_skip_forward (dds_istream_t *is, uint32_t len, const uint32_t elem_size)
{
  if (elem_size && len)
    is->m_index += len * elem_size;
}

ddsrt_nonnull_all
static void dds_stream_skip_string (dds_istream_t *is)
{
  const uint32_t length = dds_is_get4 (is);
  dds_stream_skip_forward (is, length, 1);
}

ddsrt_nonnull_all
static void dds_stream_skip_wstring (dds_istream_t *is)
{
  const uint32_t length = dds_is_get4 (is);
  dds_stream_skip_forward (is, length, 1);
}

#ifndef NDEBUG
static bool key_optimized_allowed (uint32_t insn)
{
  return (DDS_OP (insn) == DDS_OP_ADR && (insn & DDS_OP_FLAG_KEY) &&
  (!type_has_subtype_or_members (DDS_OP_TYPE (insn)) // don't allow seq, uni, arr (unless exception below), struct (unless exception below)
    || (DDS_OP_TYPE (insn) == DDS_OP_VAL_ARR && (is_primitive_or_enum_type (DDS_OP_SUBTYPE (insn)) || DDS_OP_SUBTYPE (insn) == DDS_OP_VAL_BMK)) // allow prim-array, enum-array and bitmask-array as key
    || DDS_OP_TYPE (insn) == DDS_OP_VAL_EXT // allow fields in nested structs as key
  ));
}
#endif

ddsrt_attribute_warn_unused_result ddsrt_nonnull_all
static uint32_t read_union_discriminant (dds_istream_t *is, uint32_t insn)
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
  abort ();
  return 0;
}

ddsrt_attribute_warn_unused_result ddsrt_nonnull_all
static const uint32_t *find_union_case (const uint32_t *union_ops, uint32_t disc)
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

ddsrt_attribute_warn_unused_result ddsrt_nonnull_all
static const uint32_t *skip_sequence_insns (uint32_t insn, const uint32_t *ops)
{
  uint32_t bound_op = seq_is_bounded (DDS_OP_TYPE (insn)) ? 1 : 0;
  switch (DDS_OP_SUBTYPE (insn))
  {
    case DDS_OP_VAL_BLN: case DDS_OP_VAL_1BY: case DDS_OP_VAL_2BY: case DDS_OP_VAL_4BY: case DDS_OP_VAL_8BY:
    case DDS_OP_VAL_STR: case DDS_OP_VAL_WSTR: case DDS_OP_VAL_WCHAR:
      return ops + 2 + bound_op;
    case DDS_OP_VAL_BST: case DDS_OP_VAL_BWSTR: case DDS_OP_VAL_ENU:
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

ddsrt_attribute_warn_unused_result ddsrt_nonnull_all
static const uint32_t *skip_array_insns (uint32_t insn, const uint32_t *ops)
{
  assert (DDS_OP_TYPE (insn) == DDS_OP_VAL_ARR);
  switch (DDS_OP_SUBTYPE (insn))
  {
    case DDS_OP_VAL_BLN: case DDS_OP_VAL_1BY: case DDS_OP_VAL_2BY: case DDS_OP_VAL_4BY: case DDS_OP_VAL_8BY:
    case DDS_OP_VAL_STR: case DDS_OP_VAL_WSTR: case DDS_OP_VAL_WCHAR:
      return ops + 3;
    case DDS_OP_VAL_ENU:
      return ops + 4;
    case DDS_OP_VAL_BST: case DDS_OP_VAL_BWSTR: case DDS_OP_VAL_BMK:
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

ddsrt_attribute_warn_unused_result ddsrt_nonnull_all
static const uint32_t *skip_array_default (uint32_t insn, char * restrict data, const struct dds_cdrstream_allocator *allocator, const uint32_t *ops, enum sample_data_state sample_state)
{
  const enum dds_stream_typecode subtype = DDS_OP_SUBTYPE (insn);
  const uint32_t num = ops[2];
  switch (subtype)
  {
    case DDS_OP_VAL_BLN: case DDS_OP_VAL_1BY: case DDS_OP_VAL_2BY: case DDS_OP_VAL_4BY: case DDS_OP_VAL_8BY: case DDS_OP_VAL_WCHAR: {
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
        ptr[i] = dds_stream_reuse_string_empty (ptr[i], allocator, sample_state);
      return ops + 3;
    }
    case DDS_OP_VAL_WSTR: {
      wchar_t **ptr = (wchar_t **) data;
      for (uint32_t i = 0; i < num; i++)
        ptr[i] = dds_stream_reuse_wstring_empty (ptr[i], allocator, sample_state);
      return ops + 3;
    }
    case DDS_OP_VAL_BST: {
      char *ptr = (char *) data;
      const uint32_t elem_size = ops[4];
      for (uint32_t i = 0; i < num; i++)
        (ptr + i * elem_size)[0] = '\0';
      return ops + 5;
    }
    case DDS_OP_VAL_BWSTR: {
      wchar_t *ptr = (wchar_t *) data;
      const uint32_t elem_size = (uint32_t) sizeof (*ptr) * ops[4];
      for (uint32_t i = 0; i < num; i++)
        (ptr + i * elem_size)[0] = L'\0';
      return ops + 5;
    }
    case DDS_OP_VAL_SEQ: case DDS_OP_VAL_BSQ: case DDS_OP_VAL_ARR: case DDS_OP_VAL_UNI: case DDS_OP_VAL_STU: {
      const uint32_t *jsr_ops = ops + DDS_OP_ADR_JSR (ops[3]);
      const uint32_t jmp = DDS_OP_ADR_JMP (ops[3]);
      const uint32_t elem_size = ops[4];
      for (uint32_t i = 0; i < num; i++)
        (void) dds_stream_skip_default (data + i * elem_size, allocator, jsr_ops, sample_state);
      return ops + (jmp ? jmp : 5);
    }
    case DDS_OP_VAL_EXT: {
      abort (); /* not supported */
      break;
    }
  }
  return NULL;
}

ddsrt_attribute_warn_unused_result ddsrt_nonnull_all
static inline uint32_t const * stream_union_switch_case (uint32_t insn, uint32_t disc, char * restrict discaddr, char * restrict baseaddr, const struct dds_cdrstream_allocator *allocator, const uint32_t *ops, enum sample_data_state *sample_state)
{
  /* Switching union cases causes big trouble if some cases have sequences or strings,
     and other cases have other things mapped to those addresses.  So, pretend to be
     nice by freeing whatever was allocated, and set the sample data state to UNINITIALIZED.
     This will make any preallocated buffers go to waste, but it does allow reusing the message
     from read-to-read, at the somewhat reasonable price of a slower deserialization. */
  if (*sample_state == SAMPLE_DATA_INITIALIZED)
  {
    dds_stream_free_sample_uni (discaddr, baseaddr, allocator, ops, insn);
    *sample_state = SAMPLE_DATA_UNINITIALIZED;
  }

  switch (DDS_OP_SUBTYPE (insn))
  {
    case DDS_OP_VAL_BLN: case DDS_OP_VAL_1BY: *((uint8_t *) discaddr) = (uint8_t) disc; break;
    case DDS_OP_VAL_2BY: *((uint16_t *) discaddr) = (uint16_t) disc; break;
    case DDS_OP_VAL_4BY: case DDS_OP_VAL_ENU: *((uint32_t *) discaddr) = disc; break;
    default: break;
  }

  return find_union_case (ops, disc);
}

ddsrt_nonnull_all
static void dds_stream_union_member_alloc_external (uint32_t const * const jeq_op, const enum dds_stream_typecode valtype, void ** valaddr, const struct dds_cdrstream_allocator *allocator, enum sample_data_state *sample_state)
{
  assert (DDS_OP (jeq_op[0]) == DDS_OP_JEQ4);
  if (*sample_state != SAMPLE_DATA_INITIALIZED || *((char **) *valaddr) == NULL)
  {
    uint32_t sz = get_jeq4_type_size (valtype, jeq_op);
    *((char **) *valaddr) = allocator->malloc (sz);
    *sample_state = SAMPLE_DATA_UNINITIALIZED;
  }
  *valaddr = *((char **) *valaddr);
}

ddsrt_attribute_warn_unused_result ddsrt_nonnull_all
static const uint32_t * skip_union_default (uint32_t insn, char * restrict discaddr, char * restrict baseaddr, const struct dds_cdrstream_allocator *allocator, const uint32_t *ops, enum sample_data_state sample_state)
{
  const uint32_t disc = 0;
  uint32_t const * const jeq_op = stream_union_switch_case (insn, disc, discaddr, baseaddr, allocator, ops, &sample_state);
  ops += DDS_OP_ADR_JMP (ops[3]);
  if (jeq_op)
  {
    const enum dds_stream_typecode valtype = DDS_JEQ_TYPE (jeq_op[0]);
    void *valaddr = baseaddr + jeq_op[2];

    if (op_type_external (jeq_op[0]))
      dds_stream_union_member_alloc_external (jeq_op, valtype, &valaddr, allocator, &sample_state);

    switch (valtype)
    {
      case DDS_OP_VAL_BLN: case DDS_OP_VAL_1BY: *((uint8_t *) valaddr) = 0; break;
      case DDS_OP_VAL_2BY: *((uint16_t *) valaddr) = 0; break;
      case DDS_OP_VAL_4BY: case DDS_OP_VAL_ENU: *((uint32_t *) valaddr) = 0; break;
      case DDS_OP_VAL_8BY: *((uint64_t *) valaddr) = 0; break;
      case DDS_OP_VAL_STR: *(char **) valaddr = dds_stream_reuse_string_empty (*((char **) valaddr), allocator, sample_state); break;
      case DDS_OP_VAL_WSTR: *(wchar_t **) valaddr = dds_stream_reuse_wstring_empty (*((wchar_t **) valaddr), allocator, sample_state); break;
      case DDS_OP_VAL_WCHAR: *((wchar_t *) valaddr) = L'\0'; break;
      case DDS_OP_VAL_BST: case DDS_OP_VAL_BWSTR: case DDS_OP_VAL_SEQ: case DDS_OP_VAL_BSQ: case DDS_OP_VAL_ARR: case DDS_OP_VAL_UNI: case DDS_OP_VAL_STU: case DDS_OP_VAL_BMK:
        (void) dds_stream_skip_default (valaddr, allocator, jeq_op + DDS_OP_ADR_JSR (jeq_op[0]), sample_state);
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
    case DDS_OP_VAL_2BY: case DDS_OP_VAL_WCHAR:
      return LENGTH_CODE_NEXTINT;

    /* Sequence length (item count) is used to calculate byte length */
    case DDS_OP_VAL_4BY:
      return LENGTH_CODE_ALSO_NEXTINT4;
    case DDS_OP_VAL_8BY:
      return LENGTH_CODE_ALSO_NEXTINT8;

    /* Sequences with non-primitive subtype contain a DHEADER */
    case DDS_OP_VAL_STR: case DDS_OP_VAL_WSTR: case DDS_OP_VAL_BST: case DDS_OP_VAL_BWSTR: case DDS_OP_VAL_ENU: case DDS_OP_VAL_BMK:
    case DDS_OP_VAL_SEQ: case DDS_OP_VAL_BSQ: case DDS_OP_VAL_ARR: case DDS_OP_VAL_UNI: case DDS_OP_VAL_STU:
      return LENGTH_CODE_ALSO_NEXTINT;

    /* not supported */
    case DDS_OP_VAL_EXT:
      abort ();
      break;
  }
  abort ();
  return 0u;
}

static uint32_t get_length_code_arr (const enum dds_stream_typecode subtype)
{
  switch (subtype)
  {
    /* An array with primitive subtype does not include a DHEADER,
       so we have to include a NEXTINT */
    case DDS_OP_VAL_BLN: case DDS_OP_VAL_1BY: case DDS_OP_VAL_2BY: case DDS_OP_VAL_4BY: case DDS_OP_VAL_8BY: case DDS_OP_VAL_WCHAR:
      return LENGTH_CODE_NEXTINT;

    /* Arrays with non-primitive subtype contain a DHEADER */
    case DDS_OP_VAL_STR: case DDS_OP_VAL_WSTR: case DDS_OP_VAL_BST: case DDS_OP_VAL_BWSTR: case DDS_OP_VAL_ENU: case DDS_OP_VAL_BMK:
    case DDS_OP_VAL_SEQ: case DDS_OP_VAL_BSQ: case DDS_OP_VAL_ARR: case DDS_OP_VAL_UNI: case DDS_OP_VAL_STU:
      return LENGTH_CODE_ALSO_NEXTINT;

    /* not supported */
    case DDS_OP_VAL_EXT:
      abort ();
      break;
  }
  abort ();
  return 0u;
}

ddsrt_attribute_warn_unused_result ddsrt_nonnull_all
static uint32_t get_length_code (const uint32_t *ops)
{
  const uint32_t insn = *ops;
  assert (insn != DDS_OP_RTS);
  switch (DDS_OP (insn))
  {
    case DDS_OP_ADR: {
      switch (DDS_OP_TYPE (insn))
      {
        case DDS_OP_VAL_BLN: case DDS_OP_VAL_1BY: return LENGTH_CODE_1B;
        case DDS_OP_VAL_2BY: case DDS_OP_VAL_WCHAR: return LENGTH_CODE_2B;
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
        case DDS_OP_VAL_STR: case DDS_OP_VAL_WSTR: case DDS_OP_VAL_BST: case DDS_OP_VAL_BWSTR:
          return LENGTH_CODE_ALSO_NEXTINT; /* nextint overlaps with length from serialized string data */
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
    case DDS_OP_RTS: case DDS_OP_JEQ: case DDS_OP_JEQ4: case DDS_OP_KOF: case DDS_OP_PLM: case DDS_OP_MID:
      abort ();
      break;
    case DDS_OP_DLC: case DDS_OP_PLC:
      /* members of (final/appendable/mutable) aggregated types are included using ADR | EXT */
      abort();
      break;
  }
  return 0;
}

ddsrt_attribute_warn_unused_result ddsrt_nonnull_all
static bool is_member_present (const char *data, const uint32_t *ops)
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
      case DDS_OP_RTS: case DDS_OP_JEQ: case DDS_OP_JEQ4: case DDS_OP_KOF: case DDS_OP_MID:
      case DDS_OP_DLC: case DDS_OP_PLC: case DDS_OP_PLM:
        abort ();
        break;
    }
  }
  abort ();
  return false;
}

ddsrt_attribute_warn_unused_result ddsrt_nonnull_all
static bool find_member_id (const struct dds_cdrstream_desc_mid_table *mid_table, const uint32_t *adr_op, uint32_t *mid)
{
  struct dds_cdrstream_desc_mid tmpl = { .adr_offs = (uint32_t) (adr_op - mid_table->op0) };
  const struct dds_cdrstream_desc_mid *m = ddsrt_hh_lookup (mid_table->table, &tmpl);
  if (m != NULL)
    *mid = m->mid;
  return m != NULL;
}

#if DDSRT_ENDIAN == DDSRT_LITTLE_ENDIAN
ddsrt_nonnull_all
static inline void dds_stream_to_BE_insitu (void *vbuf, uint32_t size, uint32_t num)
{
  dds_stream_swap (vbuf, size, num);
}
ddsrt_nonnull_all
static inline void dds_stream_to_LE_insitu (void *vbuf, uint32_t size, uint32_t num)
{
  (void) vbuf;
  (void) size;
  (void) num;
}
#define dds_stream_to__insitu dds_stream_to_LE_insitu
#else /* if DDSRT_ENDIAN == DDSRT_LITTLE_ENDIAN */
ddsrt_nonnull_all
static inline void dds_stream_to_BE_insitu (void *vbuf, uint32_t size, uint32_t num)
{
  (void) vbuf;
  (void) size;
  (void) num;
}
ddsrt_nonnull_all
static inline void dds_stream_to_LE_insitu (void *vbuf, uint32_t size, uint32_t num)
{
  dds_stream_swap (vbuf, size, num);
}
#define dds_stream_to__insitu dds_stream_to_BE_insitu
#endif /* if DDSRT_ENDIAN == DDSRT_LITTLE_ENDIAN */

// Little-endian
#define NAME_BYTE_ORDER_EXT LE
#include "dds_cdrstream_write.part.h"
#undef NAME_BYTE_ORDER_EXT

// Big-endian
#define NAME_BYTE_ORDER_EXT BE
#include "dds_cdrstream_write.part.h"
#undef NAME_BYTE_ORDER_EXT

// Native-endian
#define NAME_BYTE_ORDER_EXT
#include "dds_cdrstream_write.part.h"
#undef NAME_BYTE_ORDER_EXT

#ifndef NDEBUG
#define STREAM_SIZE_CHECK_INIT(str) const size_t check_start_index = (str).m_index
#define STREAM_SIZE_CHECK(str) do { \
    const size_t check_size = dds_stream_getsize_sample (data, desc, (str).m_xcdr_version); \
    assert (!res || check_size == (str).m_index - check_start_index); \
  } while (0)
#else
#define STREAM_SIZE_CHECK_INIT(str) do {} while (0)
#define STREAM_SIZE_CHECK(str) do {} while (0)
#endif

#if DDSRT_ENDIAN == DDSRT_LITTLE_ENDIAN

bool dds_stream_write_sample (dds_ostream_t *os, const struct dds_cdrstream_allocator *allocator, const void *data, const struct dds_cdrstream_desc *desc)
{
  STREAM_SIZE_CHECK_INIT (*os);
  const bool res = dds_stream_write_sampleLE ((dds_ostreamLE_t *) os, allocator, data, desc);
  STREAM_SIZE_CHECK (*os);
  return res;
}

bool dds_stream_write_sampleLE (dds_ostreamLE_t *os, const struct dds_cdrstream_allocator *allocator, const void *data, const struct dds_cdrstream_desc *desc)
{
  STREAM_SIZE_CHECK_INIT (os->x);
  const size_t opt_size = os->x.m_xcdr_version == DDSI_RTPS_CDR_ENC_VERSION_1 ? desc->opt_size_xcdr1 : desc->opt_size_xcdr2;
  bool res;
  if (opt_size && desc->align && (os->x.m_index % desc->align) == 0) {
    restrict_ostream_t ros;
    memcpy (&ros, os, sizeof (*os));
    ros.x.m_align_off = 0;
    dds_os_put_bytes_base (&ros.x, allocator, data, (uint32_t) opt_size);
    memcpy (os, &ros, sizeof (*os));
    res = true;
  } else {
    res = dds_stream_writeLE (os, allocator, &desc->member_ids, data, desc->ops.ops) != NULL;
  }
  STREAM_SIZE_CHECK (os->x);
  return res;
}

bool dds_stream_write_sampleBE (dds_ostreamBE_t *os, const struct dds_cdrstream_allocator *allocator, const void *data, const struct dds_cdrstream_desc *desc)
{
  STREAM_SIZE_CHECK_INIT (os->x);
  const bool res = (dds_stream_writeBE (os, allocator, &desc->member_ids, data, desc->ops.ops) != NULL);
  STREAM_SIZE_CHECK (os->x);
  return res;
}

#else /* if DDSRT_ENDIAN == DDSRT_LITTLE_ENDIAN */

bool dds_stream_write_sample (dds_ostream_t *os, const struct dds_cdrstream_allocator *allocator, const void *data, const struct dds_cdrstream_desc *desc)
{
  STREAM_SIZE_CHECK_INIT (*os);
  const bool res = dds_stream_write_sampleBE ((dds_ostreamBE_t *) os, allocator, data, desc);
  STREAM_SIZE_CHECK (*os);
  return res;
}

bool dds_stream_write_sampleLE (dds_ostreamLE_t *os, const struct dds_cdrstream_allocator *allocator, const void *data, const struct dds_cdrstream_desc *desc)
{
  STREAM_SIZE_CHECK_INIT (os->x);
  const bool res = (dds_stream_writeLE (os, allocator, &desc->member_ids, data, desc->ops.ops) != NULL);
  STREAM_SIZE_CHECK (os->x);
  return res;
}

bool dds_stream_write_sampleBE (dds_ostreamBE_t *os, const struct dds_cdrstream_allocator *allocator, const void *data, const struct dds_cdrstream_desc *desc)
{
  STREAM_SIZE_CHECK_INIT (os->x);
  const size_t opt_size = os->x.m_xcdr_version == DDSI_RTPS_CDR_ENC_VERSION_1 ? desc->opt_size_xcdr1 : desc->opt_size_xcdr2;
  bool res;
  if (opt_size && desc->align && (os->x.m_index % desc->align) == 0) {
    restrict_ostream_t ros;
    memcpy (&ros, os, sizeof (*os));
    ros.x.m_align_off = 0;
    dds_os_put_bytes_base (&ros.x, allocator, data, (uint32_t) opt_size);
    memcpy (os, &ros, sizeof (*os));
    res = true;
  } else {
    res = dds_stream_writeBE (os, allocator, &desc->member_ids, data, desc->ops.ops) != NULL;
  }
  STREAM_SIZE_CHECK (os->x);
  return res;
}

#endif /* if DDSRT_ENDIAN == DDSRT_LITTLE_ENDIAN */

#undef STREAM_SIZE_CHECK
#undef STREAM_SIZE_CHECK_INIT

const uint32_t * dds_stream_write_with_byte_order (dds_ostream_t *os, const struct dds_cdrstream_allocator *allocator, const struct dds_cdrstream_desc_mid_table *mid_table, const char *data, const uint32_t *ops, enum ddsrt_byte_order_selector bo)
{
  if (bo == DDSRT_BOSEL_LE)
    return dds_stream_writeLE ((dds_ostreamLE_t *) os, allocator, mid_table, data, ops);
  else if (bo == DDSRT_BOSEL_BE)
    return dds_stream_writeBE ((dds_ostreamBE_t *) os, allocator, mid_table, data, ops);
  else
    return dds_stream_write (os, allocator, mid_table, data, ops);
}

struct getsize_state {
  size_t pos;
  size_t align_off; // for XCDR1 mutable encoding
  const size_t alignmask; // max align (= 4 or 8 depending on XCDR version) - 1 => 3 or 7
  const enum cdr_data_kind cdr_kind;
  const uint32_t xcdr_version;
};

ddsrt_attribute_warn_unused_result ddsrt_nonnull_all
static const uint32_t *dds_stream_getsize_impl (struct getsize_state *st, const char *data, const uint32_t *ops, bool is_mutable_member);

ddsrt_nonnull_all
static inline void getsize_reserve (struct getsize_state *st, uint32_t elemsz)
{
  // elemsz is also alignment
  assert (elemsz == 1 || elemsz == 2 || elemsz == 4 || elemsz == 8);
  assert (st->alignmask == 3 || st->alignmask == 7);
  const size_t a = (elemsz - 1) & st->alignmask;
  st->pos = ((st->pos - st->align_off + a) & ~a) + elemsz;
}

ddsrt_nonnull_all
static inline void getsize_reserve_many (struct getsize_state *st, uint32_t elemsz, uint32_t n)
{
  // elemsz is also alignment
  assert (elemsz == 1 || elemsz == 2 || elemsz == 4 || elemsz == 8);
  assert (st->alignmask == 3 || st->alignmask == 7);
  const size_t a = (elemsz - 1) & st->alignmask;
  st->pos = ((st->pos - st->align_off + a) & ~a) + n * elemsz;
}

ddsrt_nonnull ((1))
static void dds_stream_getsize_string (struct getsize_state *st, const char *val)
{
  uint32_t size = val ? (uint32_t) strlen (val) + 1 : 1; // string includes '\0'
  getsize_reserve (st, 4);
  getsize_reserve_many (st, 1, size);
}

ddsrt_nonnull ((1))
static void dds_stream_getsize_wstring (struct getsize_state *st, const wchar_t *val)
{
  uint32_t size = val ? (uint32_t) wstring_utf16_len (val) : 0; // wstring does not include a terminator
  getsize_reserve (st, 4);
  getsize_reserve_many (st, 2, size);
}

ddsrt_attribute_warn_unused_result ddsrt_nonnull_all
static const uint32_t *dds_stream_getsize_seq (struct getsize_state *st, const char *addr, const uint32_t *ops, uint32_t insn)
{
  const dds_sequence_t * const seq = (const dds_sequence_t *) addr;
  uint32_t xcdrv = st->xcdr_version;

  const enum dds_stream_typecode subtype = DDS_OP_SUBTYPE (insn);
  uint32_t bound_op = seq_is_bounded (DDS_OP_TYPE (insn)) ? 1 : 0;
  uint32_t bound = bound_op ? ops[2] : 0;

  if (is_dheader_needed (subtype, xcdrv))
  {
    /* getsize_reserve space for DHEADER */
    getsize_reserve (st, 4);
  }

  const uint32_t num = seq->_length;
  if (bound && num > bound)
    return NULL;
  if (num > 0 && seq->_buffer == NULL)
    return NULL;

  getsize_reserve (st, 4);
  if (num == 0)
  {
    ops = skip_sequence_insns (insn, ops);
  }
  else
  {
    /* following length, stream is aligned to mod 4 */
    switch (subtype)
    {
      case DDS_OP_VAL_BLN:
        getsize_reserve_many (st, 1, num);
        ops += 2 + bound_op;
        break;
      case DDS_OP_VAL_1BY: case DDS_OP_VAL_2BY: case DDS_OP_VAL_4BY: case DDS_OP_VAL_8BY: case DDS_OP_VAL_WCHAR:
        getsize_reserve_many (st, get_primitive_size (subtype), num);
        ops += 2 + bound_op;
        break;
      case DDS_OP_VAL_ENU:
        getsize_reserve_many (st, DDS_OP_TYPE_SZ (insn), num);
        ops += 3 + bound_op;
        break;
      case DDS_OP_VAL_BMK:
        getsize_reserve_many (st, DDS_OP_TYPE_SZ (insn), num);
        ops += 4 + bound_op;
        break;
      case DDS_OP_VAL_STR: {
        const char **ptr = (const char **) seq->_buffer;
        for (uint32_t i = 0; i < num; i++)
          dds_stream_getsize_string (st, ptr[i]);
        ops += 2 + bound_op;
        break;
      }
      case DDS_OP_VAL_WSTR: {
        const wchar_t **ptr = (const wchar_t **) seq->_buffer;
        for (uint32_t i = 0; i < num; i++)
          dds_stream_getsize_wstring (st, ptr[i]);
        ops += 2 + bound_op;
        break;
      }
      case DDS_OP_VAL_BST: {
        const char *ptr = (const char *) seq->_buffer;
        const uint32_t elem_size = ops[2 + bound_op];
        for (uint32_t i = 0; i < num; i++)
          dds_stream_getsize_string (st, ptr + i * elem_size);
        ops += 3 + bound_op;
        break;
      }
      case DDS_OP_VAL_BWSTR: {
        const wchar_t *ptr = (const wchar_t *) seq->_buffer;
        const uint32_t elem_size = (uint32_t) sizeof (*ptr) * ops[2 + bound_op];
        for (uint32_t i = 0; i < num; i++)
          dds_stream_getsize_wstring (st, ptr + i * elem_size);
        ops += 3 + bound_op;
        break;
      }
      case DDS_OP_VAL_SEQ: case DDS_OP_VAL_BSQ: case DDS_OP_VAL_ARR: case DDS_OP_VAL_UNI: case DDS_OP_VAL_STU: {
        const uint32_t elem_size = ops[2 + bound_op];
        const uint32_t jmp = DDS_OP_ADR_JMP (ops[3 + bound_op]);
        uint32_t const * const jsr_ops = ops + DDS_OP_ADR_JSR (ops[3 + bound_op]);
        const char *ptr = (const char *) seq->_buffer;
        for (uint32_t i = 0; i < num; i++)
          if (!dds_stream_getsize_impl (st, ptr + i * elem_size, jsr_ops, false))
            return NULL;
        ops += (jmp ? jmp : (4 + bound_op)); /* FIXME: why would jmp be 0? */
        break;
      }
      case DDS_OP_VAL_EXT:
        abort (); /* op type EXT as sequence subtype not supported */
        return NULL;
    }
  }

  return ops;
}

static const uint32_t *dds_stream_getsize_arr (struct getsize_state *st, const char *addr, const uint32_t *ops, uint32_t insn)
{
  const enum dds_stream_typecode subtype = DDS_OP_SUBTYPE (insn);
  uint32_t xcdrv = st->xcdr_version;
  if (is_dheader_needed (subtype, xcdrv))
  {
    /* getsize_reserve space for DHEADER */
    getsize_reserve (st, 4);
  }
  const uint32_t num = ops[2];
  switch (subtype)
  {
    case DDS_OP_VAL_BLN:
      getsize_reserve_many (st, 1, num);
      ops += 3;
      break;
    case DDS_OP_VAL_1BY: case DDS_OP_VAL_2BY: case DDS_OP_VAL_4BY: case DDS_OP_VAL_8BY: case DDS_OP_VAL_WCHAR:
      getsize_reserve_many (st, get_primitive_size (subtype), num);
      ops += 3;
      break;
    case DDS_OP_VAL_ENU:
      getsize_reserve_many (st, DDS_OP_TYPE_SZ (insn), num);
      ops += 4;
      break;
    case DDS_OP_VAL_BMK:
      getsize_reserve_many (st, DDS_OP_TYPE_SZ (insn), num);
      ops += 5;
      break;
    case DDS_OP_VAL_STR: {
      const char **ptr = (const char **) addr;
      for (uint32_t i = 0; i < num; i++)
        dds_stream_getsize_string (st, ptr[i]);
      ops += 3;
      break;
    }
    case DDS_OP_VAL_WSTR: {
      const wchar_t **ptr = (const wchar_t **) addr;
      for (uint32_t i = 0; i < num; i++)
        dds_stream_getsize_wstring (st, ptr[i]);
      ops += 3;
      break;
    }
    case DDS_OP_VAL_BST: {
      const char *ptr = (const char *) addr;
      const uint32_t elem_size = ops[4];
      for (uint32_t i = 0; i < num; i++)
        dds_stream_getsize_string (st, ptr + i * elem_size);
      ops += 5;
      break;
    }
    case DDS_OP_VAL_BWSTR: {
      const wchar_t *ptr = (const wchar_t *) addr;
      const uint32_t elem_size = (uint32_t) sizeof (*ptr) * ops[4];
      for (uint32_t i = 0; i < num; i++)
        dds_stream_getsize_wstring (st, ptr + i * elem_size);
      ops += 5;
      break;
    }
    case DDS_OP_VAL_SEQ: case DDS_OP_VAL_BSQ: case DDS_OP_VAL_ARR: case DDS_OP_VAL_UNI: case DDS_OP_VAL_STU: {
      const uint32_t * jsr_ops = ops + DDS_OP_ADR_JSR (ops[3]);
      const uint32_t jmp = DDS_OP_ADR_JMP (ops[3]);
      const uint32_t elem_size = ops[4];
      for (uint32_t i = 0; i < num; i++)
        if (!dds_stream_getsize_impl (st, addr + i * elem_size, jsr_ops, false))
          return NULL;
      ops += (jmp ? jmp : 5);
      break;
    }
    case DDS_OP_VAL_EXT:
      abort (); /* op type EXT as array subtype not supported */
      break;
  }

  return ops;
}

static bool dds_stream_getsize_union_discriminant (struct getsize_state *st, uint32_t insn, const void *addr, uint32_t *disc)
{
  assert (disc);
  enum dds_stream_typecode type = DDS_OP_SUBTYPE (insn);
  assert (type == DDS_OP_VAL_BLN || type == DDS_OP_VAL_1BY || type == DDS_OP_VAL_2BY || type == DDS_OP_VAL_4BY || type == DDS_OP_VAL_ENU);
  switch (type)
  {
    case DDS_OP_VAL_BLN:
      *disc = *((const uint8_t *) addr) != 0;
      getsize_reserve (st, 1);
      break;
    case DDS_OP_VAL_1BY:
      *disc = *((const uint8_t *) addr);
      getsize_reserve (st, 1);
      break;
    case DDS_OP_VAL_2BY:
      *disc = *((const uint16_t *) addr);
      getsize_reserve (st, 2);
      break;
    case DDS_OP_VAL_4BY:
      *disc = *((const uint32_t *) addr);
      getsize_reserve (st, 4);
      break;
    case DDS_OP_VAL_ENU:
      *disc = *((const uint32_t *) addr);
      getsize_reserve (st, DDS_OP_TYPE_SZ (insn));
      break;
    default:
      abort ();
  }
  return true;
}

static const uint32_t *dds_stream_getsize_uni (struct getsize_state *st, const char *discaddr, const char *baseaddr, const uint32_t *ops, uint32_t insn)
{
  uint32_t disc;
  if (!dds_stream_getsize_union_discriminant (st, insn, discaddr, &disc))
    return NULL;
  uint32_t const * const jeq_op = find_union_case (ops, disc);
  ops += DDS_OP_ADR_JMP (ops[3]);
  if (jeq_op)
  {
    const enum dds_stream_typecode valtype = DDS_JEQ_TYPE (jeq_op[0]);
    const void *valaddr = baseaddr + jeq_op[2];

    /* Union members cannot be optional, only external. For string types, the pointer
       is dereferenced below (and there is no extra pointer indirection when using
       @external for STR types) */
    if (op_type_external (jeq_op[0]) && valtype != DDS_OP_VAL_STR && valtype != DDS_OP_VAL_WSTR)
    {
      assert (DDS_OP (jeq_op[0]) == DDS_OP_JEQ4);
      valaddr = *(char **) valaddr;
      if (!valaddr)
        return NULL;
    }

    switch (valtype)
    {
      case DDS_OP_VAL_BLN:
      case DDS_OP_VAL_1BY: getsize_reserve (st, 1); break;
      case DDS_OP_VAL_WCHAR:
      case DDS_OP_VAL_2BY: getsize_reserve (st, 2); break;
      case DDS_OP_VAL_4BY: getsize_reserve (st, 4); break;
      case DDS_OP_VAL_8BY: getsize_reserve (st, 8); break;
      case DDS_OP_VAL_ENU: getsize_reserve (st, DDS_OP_TYPE_SZ (jeq_op[0])); break;
      case DDS_OP_VAL_STR: dds_stream_getsize_string (st, *(const char **) valaddr); break;
      case DDS_OP_VAL_WSTR: dds_stream_getsize_wstring (st, *(const wchar_t **) valaddr); break;
      case DDS_OP_VAL_BST: dds_stream_getsize_string (st, (const char *) valaddr); break;
      case DDS_OP_VAL_BWSTR: dds_stream_getsize_wstring (st, (const wchar_t *) valaddr); break;
      case DDS_OP_VAL_SEQ: case DDS_OP_VAL_BSQ: case DDS_OP_VAL_ARR: case DDS_OP_VAL_UNI: case DDS_OP_VAL_STU: case DDS_OP_VAL_BMK:
        if (!dds_stream_getsize_impl (st, valaddr, jeq_op + DDS_OP_ADR_JSR (jeq_op[0]), false))
          return NULL;
        break;
      case DDS_OP_VAL_EXT:
        abort (); /* op type EXT as union subtype not supported */
        break;
    }
  }
  return ops;
}

static const uint32_t *dds_stream_getsize_adr (uint32_t insn, struct getsize_state *st, const char *data, const uint32_t *ops, bool is_mutable_member)
{
  const void *addr = data + ops[1];
  if (op_type_external (insn) || op_type_optional (insn) || DDS_OP_TYPE (insn) == DDS_OP_VAL_STR || DDS_OP_TYPE (insn) == DDS_OP_VAL_WSTR)
  {
    addr = *(char **) addr;
    if (addr == NULL && !(op_type_optional (insn) || DDS_OP_TYPE (insn) == DDS_OP_VAL_STR || DDS_OP_TYPE (insn) == DDS_OP_VAL_WSTR))
      return NULL;
  }

  const bool is_key = (insn & DDS_OP_FLAG_KEY);
  if (st->cdr_kind == CDR_KIND_KEY && !is_key)
    return dds_stream_skip_adr (insn, ops);

  bool alignment_offset_by_4 = false;
  if (op_type_optional (insn))
  {
    if (!is_mutable_member)
    {
      if (st->xcdr_version != DDSI_RTPS_CDR_ENC_VERSION_1)
        getsize_reserve (st, 1);
      else
      {
        getsize_reserve_many (st, 4, 3);
        if (addr && (st->pos % 8) != 0)
        {
          st->align_off += 4;
          alignment_offset_by_4 = true;
        }
      }
    }
    if (!addr)
      return dds_stream_skip_adr (insn, ops);
  }
  assert (addr || DDS_OP_TYPE (insn) == DDS_OP_VAL_STR || DDS_OP_TYPE (insn) == DDS_OP_VAL_WSTR);

  switch (DDS_OP_TYPE (insn))
  {
    case DDS_OP_VAL_BLN:
    case DDS_OP_VAL_1BY: getsize_reserve (st, 1); ops += 2; break;
    case DDS_OP_VAL_WCHAR:
    case DDS_OP_VAL_2BY: getsize_reserve (st, 2); ops += 2; break;
    case DDS_OP_VAL_4BY: getsize_reserve (st, 4); ops += 2; break;
    case DDS_OP_VAL_8BY: getsize_reserve (st, 8); ops += 2; break;
    case DDS_OP_VAL_ENU: getsize_reserve (st, DDS_OP_TYPE_SZ (insn)); ops += 3; break;
    case DDS_OP_VAL_BMK: getsize_reserve (st, DDS_OP_TYPE_SZ (insn)); ops += 4; break;
    case DDS_OP_VAL_STR: dds_stream_getsize_string (st, (const char *) addr); ops += 2; break;
    case DDS_OP_VAL_WSTR: dds_stream_getsize_wstring (st, (const wchar_t *) addr); ops += 2; break;
    case DDS_OP_VAL_BST: dds_stream_getsize_string (st, (const char *) addr); ops += 3; break;
    case DDS_OP_VAL_BWSTR: dds_stream_getsize_wstring (st, (const wchar_t *) addr); ops += 3; break;
    case DDS_OP_VAL_SEQ: case DDS_OP_VAL_BSQ: ops = dds_stream_getsize_seq (st, addr, ops, insn); break;
    case DDS_OP_VAL_ARR: ops = dds_stream_getsize_arr (st, addr, ops, insn); break;
    case DDS_OP_VAL_UNI: ops = dds_stream_getsize_uni (st, addr, data, ops, insn); break;
    case DDS_OP_VAL_EXT: {
      const uint32_t *jsr_ops = ops + DDS_OP_ADR_JSR (ops[2]);
      const uint32_t jmp = DDS_OP_ADR_JMP (ops[2]);

      /* skip DLC instruction for base type, so that the DHEADER is not
          serialized for base types */
      if (op_type_base (insn) && jsr_ops[0] == DDS_OP_DLC)
        jsr_ops++;

      /* don't forward is_mutable_member, subtype can have other extensibility */
      if (!dds_stream_getsize_impl (st, addr, jsr_ops, false))
        return NULL;
      ops += jmp ? jmp : 3;
      break;
    }
    case DDS_OP_VAL_STU: abort (); break; /* op type STU only supported as subtype */
  }

  if (alignment_offset_by_4)
    st->align_off -= 4;
  return ops;
}

static const uint32_t *dds_stream_getsize_delimited (struct getsize_state *st, const char *data, const uint32_t *ops)
{
  getsize_reserve (st, 4);
  if (!(ops = dds_stream_getsize_impl (st, data, ops + 1, false)))
    return NULL;
  return ops;
}

static bool dds_stream_getsize_xcdr2_pl_member (struct getsize_state *st, const char *data, const uint32_t *ops)
{
  /* get flags from first member op */
  uint32_t flags = DDS_OP_FLAGS (ops[0]);
  bool is_key = flags & (DDS_OP_FLAG_MU | DDS_OP_FLAG_KEY);

  if (st->cdr_kind == CDR_KIND_KEY && !is_key)
    return true;

  uint32_t lc = get_length_code (ops);
  assert (lc <= LENGTH_CODE_ALSO_NEXTINT8);
  /* space for emheader */
  getsize_reserve (st, (lc != LENGTH_CODE_NEXTINT) ? 4 : 8);
  if (!(dds_stream_getsize_impl (st, data, ops, true)))
    return false;
  return true;
}

static const uint32_t *dds_stream_getsize_xcdr2_pl_memberlist (struct getsize_state *st, const char *data, const uint32_t *ops)
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
          if (!dds_stream_getsize_xcdr2_pl_memberlist (st, data, plm_ops))
            return NULL;
        }
        else if (is_member_present (data, plm_ops))
        {
          if (!dds_stream_getsize_xcdr2_pl_member (st, data, plm_ops))
            return NULL;
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

static const uint32_t *dds_stream_getsize_xcdr2_pl (struct getsize_state *st, const char *data, const uint32_t *ops)
{
  /* skip PLC op */
  ops++;
  /* alloc space for dheader */
  getsize_reserve (st, 4);
  /* members, including members from base types */
  return dds_stream_getsize_xcdr2_pl_memberlist (st, data, ops);
}

static const uint32_t *dds_stream_getsize_impl (struct getsize_state *st, const char *data, const uint32_t *ops0, bool is_mutable_member)
{
  const uint32_t *ops = ops0;
  uint32_t insn;
  while (ops && (insn = *ops) != DDS_OP_RTS)
  {
    switch (DDS_OP (insn))
    {
      case DDS_OP_ADR:
        ops = dds_stream_getsize_adr (insn, st, data, ops, is_mutable_member);
        break;
      case DDS_OP_JSR:
        if (!dds_stream_getsize_impl (st, data, ops + DDS_OP_JUMP (insn), is_mutable_member))
          return NULL;
        ops++;
        break;
      case DDS_OP_RTS: case DDS_OP_JEQ: case DDS_OP_JEQ4: case DDS_OP_KOF: case DDS_OP_PLM: case DDS_OP_MID:
        abort ();
        break;
      case DDS_OP_DLC:
        assert (st->xcdr_version == DDSI_RTPS_CDR_ENC_VERSION_2);
        ops = dds_stream_getsize_delimited (st, data, ops);
        break;
      case DDS_OP_PLC:
        assert (st->xcdr_version == DDSI_RTPS_CDR_ENC_VERSION_2);
        ops = dds_stream_getsize_xcdr2_pl (st, data, ops);
        break;
    }
  }
  return ops;
}

ddsrt_attribute_warn_unused_result ddsrt_nonnull_all
static size_t dds_stream_getsize_sample_impl (const char *data, const uint32_t *ops, uint32_t xcdr_version)
{
  struct getsize_state st = {
    .pos = 0,
    .align_off = 0,
    .alignmask = (xcdr_version == DDSI_RTPS_CDR_ENC_VERSION_2 ? 3 : 7),
    .cdr_kind = CDR_KIND_DATA,
    .xcdr_version = xcdr_version
  };
  (void) dds_stream_getsize_impl (&st, data, ops, false);
  return st.pos;
}

ddsrt_nonnull_all
size_t dds_stream_getsize_sample (const char *data, const struct dds_cdrstream_desc *desc, uint32_t xcdr_version)
{
  return dds_stream_getsize_sample_impl (data, desc->ops.ops, xcdr_version);
}

ddsrt_nonnull ((1, 2, 3))
static void dds_stream_getsize_key_impl (struct getsize_state *st, const uint32_t *ops, const void *src, uint16_t key_offset_count, const uint32_t * key_offset_insn)
{
  uint32_t insn = *ops;
  assert (DDS_OP (insn) == DDS_OP_ADR);
  assert (key_optimized_allowed (insn));
  void *addr = (char *) src + ops[1];

  if (op_type_external (insn) || DDS_OP_TYPE (insn) == DDS_OP_VAL_STR || DDS_OP_TYPE (insn) == DDS_OP_VAL_WSTR)
  {
    addr = *(char **) addr;
    if (addr == NULL && DDS_OP_TYPE (insn) != DDS_OP_VAL_STR && DDS_OP_TYPE (insn) != DDS_OP_VAL_WSTR)
      return;
  }

  switch (DDS_OP_TYPE (insn))
  {
    case DDS_OP_VAL_BLN:
    case DDS_OP_VAL_1BY: getsize_reserve (st, 1); break;
    case DDS_OP_VAL_WCHAR:
    case DDS_OP_VAL_2BY: getsize_reserve (st, 2); break;
    case DDS_OP_VAL_4BY: getsize_reserve (st, 4); break;
    case DDS_OP_VAL_8BY: getsize_reserve (st, 8); break;
    case DDS_OP_VAL_ENU:
    case DDS_OP_VAL_BMK: getsize_reserve (st, DDS_OP_TYPE_SZ (insn)); break;
    case DDS_OP_VAL_STR: dds_stream_getsize_string (st, addr); break;
    case DDS_OP_VAL_WSTR: dds_stream_getsize_wstring (st, addr); break;
    case DDS_OP_VAL_BST: dds_stream_getsize_string (st, addr); break;
    case DDS_OP_VAL_BWSTR: dds_stream_getsize_wstring (st, addr); break;
    case DDS_OP_VAL_ARR: {
      const uint32_t num = ops[2];
      switch (DDS_OP_SUBTYPE (insn))
      {
        case DDS_OP_VAL_BLN: case DDS_OP_VAL_1BY: case DDS_OP_VAL_2BY: case DDS_OP_VAL_4BY: case DDS_OP_VAL_8BY:
          getsize_reserve_many (st, get_primitive_size (DDS_OP_SUBTYPE (insn)), num);
          break;
        case DDS_OP_VAL_ENU: case DDS_OP_VAL_BMK:
          if (st->xcdr_version == DDSI_RTPS_CDR_ENC_VERSION_2)
          {
            /* getsize_reserve space for DHEADER */
            getsize_reserve (st, 4);
          }
          getsize_reserve_many (st, DDS_OP_TYPE_SZ (insn), num);
          break;
        default:
          abort ();
      }
      break;
    }
    case DDS_OP_VAL_EXT: {
      assert (key_offset_count > 0);
      const uint32_t *jsr_ops = ops + DDS_OP_ADR_JSR (ops[2]) + *key_offset_insn;
      dds_stream_getsize_key_impl (st, jsr_ops, addr, --key_offset_count, ++key_offset_insn);
      break;
    }
    case DDS_OP_VAL_SEQ: case DDS_OP_VAL_BSQ: {
      // When key contains a sequence, the non-optimized path is used
      abort ();
      break;
    }
    case DDS_OP_VAL_UNI: case DDS_OP_VAL_STU: {
      // FIXME: implement support for unions as part of the key
      abort ();
      break;
    }
  }
}

ddsrt_nonnull_all
size_t dds_stream_getsize_key (const char *sample, const struct dds_cdrstream_desc *desc, uint32_t xcdr_version)
{
  struct getsize_state st = {
    .pos = 0,
    .align_off = 0,
    .alignmask = (xcdr_version == DDSI_RTPS_CDR_ENC_VERSION_2 ? 3 : 7),
    .cdr_kind = CDR_KIND_KEY,
    .xcdr_version = xcdr_version
  };
  if (desc->flagset & (DDS_TOPIC_KEY_APPENDABLE | DDS_TOPIC_KEY_MUTABLE | DDS_TOPIC_KEY_SEQUENCE | DDS_TOPIC_KEY_ARRAY_NONPRIM))
  {
    /* For types with key fields in aggregated types with appendable or mutable
       extensibility, determine the key CDR size using the regular function */
    (void) dds_stream_getsize_impl (&st, sample, desc->ops.ops, false);
  }
  else
  {
    /* Optimized implementation to write key in case all key members are in an aggregated
       type with final extensibility: iterate over keys in key descriptor. */
    for (uint32_t i = 0; i < desc->keys.nkeys; i++)
    {
      const uint32_t *insnp = desc->ops.ops + desc->keys.keys_definition_order[i].ops_offs;
      switch (DDS_OP (*insnp))
      {
        case DDS_OP_KOF: {
          uint16_t n_offs = DDS_OP_LENGTH (*insnp);
          assert (n_offs > 0);
          dds_stream_getsize_key_impl (&st, desc->ops.ops + insnp[1], sample, --n_offs, insnp + 2);
          break;
        }
        case DDS_OP_ADR: {
          dds_stream_getsize_key_impl (&st, insnp, sample, 0, NULL);
          break;
        }
        default:
          abort ();
          break;
      }
    }
  }
  return st.pos;
}

ddsrt_nonnull_all
static void malloc_sequence_buffer (dds_sequence_t *seq, const struct dds_cdrstream_allocator *allocator, uint32_t num, uint32_t elem_size)
{
  const uint32_t size = num * elem_size;
  seq->_buffer = allocator->malloc (size);
  seq->_release = true;
  seq->_maximum = num;
}

ddsrt_nonnull_all
static void grow_sequence_buffer_initialize (dds_sequence_t *seq, const struct dds_cdrstream_allocator *allocator, uint32_t num, uint32_t elem_size)
{
  // valid input for seq:
  //  (_maximum == 0 && _buffer == NULL)
  //  (_maximum == 0 && _buffer != NULL)
  //  (_maximum >  0 && _buffer != NULL)
  // if _buffer is a non-null pointer, it must point to memory
  // obtained from "allocator"
  const uint32_t size = num * elem_size;
  const uint32_t off = seq->_maximum * elem_size;
  seq->_buffer = allocator->realloc (seq->_buffer, size);
  seq->_release = true; // usually already true
  seq->_maximum = num;
  memset (seq->_buffer + off, 0, size - off);
}

/**
 * Sequences of types that possibly contain pointers are maintained in initialized form for all
 * allocated entries (i.e., up to _maximum), not just the occupied ones (i.e., up to _length).
 * This way there is no need to free any previously allocated memory when reusing the buffer
 * for a shorter sequence.
 *
 * The argument is that when the samples/buffers do get reused from one call to read() to the
 * next for complex types, this should reduce the number of memory allocations/frees and save
 * the cost of freeing the elements. The downside is higher memory usage, which the application
 * can avoid by using its buffers in a slightly different way, and the additional memcpy/memset
 * on realloc, but those operations are usually cheaper than trying to free the sample.
 */
ddsrt_nonnull_all
static void adjust_sequence_buffer_initialize (dds_sequence_t *seq, const struct dds_cdrstream_allocator *allocator, uint32_t num, uint32_t elem_size, enum sample_data_state *sample_state)
{
  // If num == 0, dds_stream_read_seq short-circuits
  assert (num > 0);
  if (*sample_state != SAMPLE_DATA_INITIALIZED)
  {
    const uint32_t size = num * elem_size;
    malloc_sequence_buffer (seq, allocator, num, elem_size);
    memset (seq->_buffer, 0, size);
    *sample_state = SAMPLE_DATA_INITIALIZED;
  }
  else
  {
    // Maintain max sequence length for broken applications that provided
    // a pre-allocated buffer and only set _length.  (Would anyone really
    // expect that to work?)
    if (seq->_length > seq->_maximum)
      seq->_maximum = seq->_length;
    // We own the buffer if _release, in which case we realloc if we need
    // more memory. We *take* ownership if we need memory but _maximum is
    // 0, which is how we support initializing with all zeros.
    if (num > seq->_maximum && (seq->_release || seq->_maximum == 0))
      grow_sequence_buffer_initialize (seq, allocator, num, elem_size);
  }
}

ddsrt_nonnull_all
static void adjust_sequence_buffer (dds_sequence_t *seq, const struct dds_cdrstream_allocator *allocator, uint32_t num, uint32_t elem_size, enum sample_data_state *sample_state)
{
  // Reduced version of adjust_sequence_buffer_initialize that avoids
  // memsetting when we know it won't matter, e.g. a sequence of ints
  // won't cause any trouble if the bits between _length and _maximum
  // remain garbage.
  assert (num > 0);
  if (*sample_state != SAMPLE_DATA_INITIALIZED)
    malloc_sequence_buffer (seq, allocator, num, elem_size);
  else
  {
    if (seq->_length > seq->_maximum)
      seq->_maximum = seq->_length;
    if (num > seq->_maximum && (seq->_release || seq->_maximum == 0))
    {
      allocator->free (seq->_buffer);
      malloc_sequence_buffer (seq, allocator, num, elem_size);
      *sample_state = SAMPLE_DATA_UNINITIALIZED;
    }
  }
}

ddsrt_nonnull_all
static bool stream_is_member_present (dds_istream_t *is, uint32_t *param_len)
{
  if (is->m_xcdr_version == DDSI_RTPS_CDR_ENC_VERSION_1)
  {
    uint32_t phdr = dds_is_get4 (is);
    uint32_t plen;
    if ((phdr & DDS_XCDR1_PL_SHORT_PID_MASK) == DDS_XCDR1_PL_SHORT_PID_EXTENDED)
    {
      (void) dds_is_get4 (is); /* skip param ID (is checked in normalize) */
      plen = dds_is_get4 (is);
    }
    else
    {
      plen = (uint32_t) (phdr & DDS_XCDR1_PL_SHORT_LEN_MASK);
    }
    *param_len = plen;
    return plen > 0;
  }
  else
  {
    *param_len = 0;
    return dds_is_get1 (is);
  }
}

ddsrt_attribute_warn_unused_result ddsrt_nonnull_all
static const uint32_t *initialize_and_skip_sequence (dds_sequence_t *seq, uint32_t insn, const uint32_t *ops, enum sample_data_state sample_state)
{
  if (sample_state == SAMPLE_DATA_UNINITIALIZED)
  {
    seq->_buffer = NULL;
    seq->_maximum = 0;
    seq->_release = true;
  }
  seq->_length = 0;
  return skip_sequence_insns (insn, ops);
}

ddsrt_attribute_warn_unused_result ddsrt_nonnull_all
static const uint32_t *dds_stream_read_seq (dds_istream_t *is, char * restrict addr, const struct dds_cdrstream_allocator *allocator, const uint32_t *ops, uint32_t insn, enum cdr_data_kind cdr_kind, enum sample_data_state sample_state)
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
    return initialize_and_skip_sequence (seq, insn, ops, sample_state);

  switch (subtype)
  {
    case DDS_OP_VAL_BLN: case DDS_OP_VAL_1BY: case DDS_OP_VAL_2BY: case DDS_OP_VAL_4BY: case DDS_OP_VAL_8BY: {
      const uint32_t elem_size = get_primitive_size (subtype);
      adjust_sequence_buffer (seq, allocator, num, elem_size, &sample_state);
      seq->_length = (num <= seq->_maximum) ? num : seq->_maximum;
      dds_is_get_bytes (is, seq->_buffer, seq->_length, elem_size);
      if (seq->_length < num)
        dds_stream_skip_forward (is, num - seq->_length, elem_size);
      return ops + 2 + bound_op;
    }
    case DDS_OP_VAL_WCHAR: {
      adjust_sequence_buffer (seq, allocator, num, sizeof (wchar_t), &sample_state);
      seq->_length = (num <= seq->_maximum) ? num : seq->_maximum;
      for (uint32_t i = 0; i < seq->_length; i++)
        ((wchar_t *) seq->_buffer)[i] = (wchar_t) dds_is_get2 (is);
      if (seq->_length < num)
        dds_stream_skip_forward (is, num - seq->_length, 2);
      return ops + 2 + bound_op;
    }
    case DDS_OP_VAL_ENU: {
      const uint32_t elem_size = DDS_OP_TYPE_SZ (insn);
      adjust_sequence_buffer (seq, allocator, num, 4, &sample_state);
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
      adjust_sequence_buffer (seq, allocator, num, elem_size, &sample_state);
      seq->_length = (num <= seq->_maximum) ? num : seq->_maximum;
      dds_is_get_bytes (is, seq->_buffer, seq->_length, elem_size);
      if (seq->_length < num)
        dds_stream_skip_forward (is, num - seq->_length, elem_size);
      return ops + 4 + bound_op;
    }
    case DDS_OP_VAL_STR: {
      adjust_sequence_buffer_initialize (seq, allocator, num, sizeof (char *), &sample_state);
      seq->_length = (num <= seq->_maximum) ? num : seq->_maximum;
      char **ptr = (char **) seq->_buffer;
      for (uint32_t i = 0; i < seq->_length; i++)
        ptr[i] = dds_stream_reuse_string (is, ptr[i], allocator, sample_state);
      for (uint32_t i = seq->_length; i < num; i++)
        dds_stream_skip_string (is);
      return ops + 2 + bound_op;
    }
    case DDS_OP_VAL_WSTR: {
      adjust_sequence_buffer_initialize (seq, allocator, num, sizeof (wchar_t *), &sample_state);
      seq->_length = (num <= seq->_maximum) ? num : seq->_maximum;
      wchar_t **ptr = (wchar_t **) seq->_buffer;
      for (uint32_t i = 0; i < seq->_length; i++)
        ptr[i] = dds_stream_reuse_wstring (is, ptr[i], allocator, sample_state);
      for (uint32_t i = seq->_length; i < num; i++)
        dds_stream_skip_wstring (is);
      return ops + 2 + bound_op;
    }
    case DDS_OP_VAL_BST: {
      const uint32_t elem_size = ops[2 + bound_op];
      adjust_sequence_buffer (seq, allocator, num, elem_size, &sample_state);
      seq->_length = (num <= seq->_maximum) ? num : seq->_maximum;
      char *ptr = (char *) seq->_buffer;
      for (uint32_t i = 0; i < seq->_length; i++)
        (void) dds_stream_reuse_string_bound (is, ptr + i * elem_size, elem_size);
      for (uint32_t i = seq->_length; i < num; i++)
        dds_stream_skip_string (is);
      return ops + 3 + bound_op;
    }
    case DDS_OP_VAL_BWSTR: {
      const uint32_t elem_size = (uint32_t) sizeof (wchar_t) * ops[2 + bound_op];
      assert (elem_size > 0);
      const uint32_t bound = ops[2 + bound_op];
      adjust_sequence_buffer (seq, allocator, num, elem_size, &sample_state);
      seq->_length = (num <= seq->_maximum) ? num : seq->_maximum;
      wchar_t *ptr = (wchar_t *) seq->_buffer;
      for (uint32_t i = 0; i < seq->_length; i++)
        (void) dds_stream_reuse_wstring_bound (is, ptr + i * bound, bound);
      for (uint32_t i = seq->_length; i < num; i++)
        dds_stream_skip_wstring (is);
      return ops + 3 + bound_op;
    }
    case DDS_OP_VAL_SEQ: case DDS_OP_VAL_BSQ: case DDS_OP_VAL_ARR: case DDS_OP_VAL_UNI: case DDS_OP_VAL_STU: {
      const uint32_t elem_size = ops[2 + bound_op];
      const uint32_t jmp = DDS_OP_ADR_JMP (ops[3 + bound_op]);
      uint32_t const * const jsr_ops = ops + DDS_OP_ADR_JSR (ops[3 + bound_op]);
      adjust_sequence_buffer_initialize (seq, allocator, num, elem_size, &sample_state);
      seq->_length = (num <= seq->_maximum) ? num : seq->_maximum;
      char *ptr = (char *) seq->_buffer;
      for (uint32_t i = 0; i < num; i++)
        (void) dds_stream_read_impl (is, ptr + i * elem_size, allocator, jsr_ops, false, cdr_kind, sample_state);
      return ops + (jmp ? jmp : (4 + bound_op)); /* FIXME: why would jmp be 0? */
    }
    case DDS_OP_VAL_EXT: {
      abort (); /* not supported */
      break;
    }
  }
  return NULL;
}

ddsrt_attribute_warn_unused_result ddsrt_nonnull_all
static const uint32_t *dds_stream_read_arr (dds_istream_t *is, char * restrict addr, const struct dds_cdrstream_allocator *allocator, const uint32_t *ops, uint32_t insn, enum cdr_data_kind cdr_kind, enum sample_data_state sample_state)
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
    case DDS_OP_VAL_WCHAR: {
      for (uint32_t i = 0; i < num; i++)
         ((wchar_t *) addr)[i] = (wchar_t) dds_is_get2 (is);
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
        ptr[i] = dds_stream_reuse_string (is, ptr[i], allocator, sample_state);
      return ops + 3;
    }
    case DDS_OP_VAL_WSTR: {
      wchar_t **ptr = (wchar_t **) addr;
      for (uint32_t i = 0; i < num; i++)
        ptr[i] = dds_stream_reuse_wstring (is, ptr[i], allocator, sample_state);
      return ops + 3;
    }
    case DDS_OP_VAL_BST: {
      char *ptr = (char *) addr;
      const uint32_t elem_size = ops[4];
      for (uint32_t i = 0; i < num; i++)
        (void) dds_stream_reuse_string_bound (is, ptr + i * elem_size, elem_size);
      return ops + 5;
    }
    case DDS_OP_VAL_BWSTR: {
      wchar_t *ptr = (wchar_t *) addr;
      const uint32_t elem_size = ops[4];
      for (uint32_t i = 0; i < num; i++)
        (void) dds_stream_reuse_wstring_bound (is, ptr + i * elem_size, elem_size);
      return ops + 5;
    }
    case DDS_OP_VAL_SEQ: case DDS_OP_VAL_BSQ: case DDS_OP_VAL_ARR: case DDS_OP_VAL_UNI: case DDS_OP_VAL_STU: {
      const uint32_t *jsr_ops = ops + DDS_OP_ADR_JSR (ops[3]);
      const uint32_t jmp = DDS_OP_ADR_JMP (ops[3]);
      const uint32_t elem_size = ops[4];
      for (uint32_t i = 0; i < num; i++)
        (void) dds_stream_read_impl (is, addr + i * elem_size, allocator, jsr_ops, false, cdr_kind, sample_state);
      return ops + (jmp ? jmp : 5);
    }
    case DDS_OP_VAL_EXT: {
      abort (); /* not supported */
      break;
    }
  }
  return NULL;
}

ddsrt_attribute_warn_unused_result ddsrt_nonnull_all
static const uint32_t *dds_stream_read_uni (dds_istream_t *is, char * restrict discaddr, char * restrict baseaddr, const struct dds_cdrstream_allocator *allocator, const uint32_t *ops, uint32_t insn, enum cdr_data_kind cdr_kind, enum sample_data_state sample_state)
{
  const uint32_t disc = read_union_discriminant (is, insn);
  uint32_t const * const jeq_op = stream_union_switch_case (insn, disc, discaddr, baseaddr, allocator, ops, &sample_state);
  ops += DDS_OP_ADR_JMP (ops[3]);
  if (jeq_op)
  {
    const enum dds_stream_typecode valtype = DDS_JEQ_TYPE (jeq_op[0]);
    void *valaddr = baseaddr + jeq_op[2];

    if (op_type_external (jeq_op[0]))
      dds_stream_union_member_alloc_external (jeq_op, valtype, &valaddr, allocator, &sample_state);

    switch (valtype)
    {
      case DDS_OP_VAL_BLN: case DDS_OP_VAL_1BY: *((uint8_t *) valaddr) = dds_is_get1 (is); break;
      case DDS_OP_VAL_2BY: *((uint16_t *) valaddr) = dds_is_get2 (is); break;
      case DDS_OP_VAL_4BY: *((uint32_t *) valaddr) = dds_is_get4 (is); break;
      case DDS_OP_VAL_8BY: *((uint64_t *) valaddr) = dds_is_get8 (is); break;
      case DDS_OP_VAL_WCHAR: *((wchar_t *) valaddr) = (wchar_t) dds_is_get2 (is); break;
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
        *(char **) valaddr = dds_stream_reuse_string (is, *((char **) valaddr), allocator, sample_state);
        break;
      case DDS_OP_VAL_WSTR:
        *(wchar_t **) valaddr = dds_stream_reuse_wstring (is, *((wchar_t **) valaddr), allocator, sample_state);
        break;
      case DDS_OP_VAL_BST: case DDS_OP_VAL_BWSTR: case DDS_OP_VAL_SEQ: case DDS_OP_VAL_BSQ: case DDS_OP_VAL_ARR: case DDS_OP_VAL_BMK:
        (void) dds_stream_read_impl (is, valaddr, allocator, jeq_op + DDS_OP_ADR_JSR (jeq_op[0]), false, cdr_kind, sample_state);
        break;
      case DDS_OP_VAL_UNI: case DDS_OP_VAL_STU: {
        const uint32_t *jsr_ops = jeq_op + DDS_OP_ADR_JSR (jeq_op[0]);
        (void) dds_stream_read_impl (is, valaddr, allocator, jsr_ops, false, cdr_kind, sample_state);
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

ddsrt_nonnull_all
static void dds_stream_alloc_external (const uint32_t *ops, uint32_t insn, void ** addr, const struct dds_cdrstream_allocator *allocator, enum sample_data_state * sample_state)
{
  uint32_t sz = get_adr_type_size (insn, ops);
  if (*sample_state != SAMPLE_DATA_INITIALIZED || *((char **) *addr) == NULL)
  {
    *((char **) *addr) = allocator->malloc (sz);
    *sample_state = SAMPLE_DATA_UNINITIALIZED;
  }
  *addr = *((char **) *addr);
}

ddsrt_attribute_warn_unused_result ddsrt_nonnull_all
static inline const uint32_t *stream_skip_member (uint32_t insn, char * restrict data, void * restrict addr, const struct dds_cdrstream_allocator *allocator, const uint32_t *ops, enum sample_data_state sample_state)
{
  if (sample_state == SAMPLE_DATA_INITIALIZED)
    return stream_free_sample_adr (insn, data, allocator, ops);

  *((char **) addr) = NULL;
  return dds_stream_skip_adr (insn, ops);
}

ddsrt_attribute_warn_unused_result ddsrt_nonnull_all
static inline const uint32_t *dds_stream_read_adr (uint32_t insn, dds_istream_t *is, char * restrict data, const struct dds_cdrstream_allocator *allocator, const uint32_t *ops, bool is_mutable_member, enum cdr_data_kind cdr_kind, enum sample_data_state sample_state)
{
  void *addr = data + ops[1];

  // In case of reading key CDR, we don't initialize non-key members. This includes not
  // malloc'ing external members (which should always be non-NULL), because it would
  // not make sense to require allocations for non-key members in an invalid sample.
  const bool is_key = (insn & DDS_OP_FLAG_KEY);
  if (cdr_kind == CDR_KIND_KEY && !is_key)
    return dds_stream_skip_adr (insn, ops);

  dds_istream_t is1 = *is;
  uint32_t param_len = 0;
  if (op_type_optional (insn) && !is_mutable_member)
  {
    if (!stream_is_member_present (&is1, &param_len))
    {
      is->m_index = is1.m_index + param_len; // param_len is 0 for XCDR2
      return stream_skip_member (insn, data, addr, allocator, ops, sample_state);
    }
    if (is->m_xcdr_version == DDSI_RTPS_CDR_ENC_VERSION_1)
    {
      // increase istream index for member header
      is->m_index = is1.m_index;

      // Move buffer in temporary istream `is1` to start of parameter value and
      // set size to param length, so that alignment is reset to 0
      is1.m_buffer += is1.m_index;
      is1.m_index = 0;
      is1.m_size = param_len;
    }
  }

  if (op_type_external (insn))
    dds_stream_alloc_external (ops, insn, &addr, allocator, &sample_state);

  switch (DDS_OP_TYPE (insn))
  {
    case DDS_OP_VAL_BLN: case DDS_OP_VAL_1BY: *((uint8_t *) addr) = dds_is_get1 (&is1); ops += 2; break;
    case DDS_OP_VAL_2BY: *((uint16_t *) addr) = dds_is_get2 (&is1); ops += 2; break;
    case DDS_OP_VAL_4BY: *((uint32_t *) addr) = dds_is_get4 (&is1); ops += 2; break;
    case DDS_OP_VAL_8BY: *((uint64_t *) addr) = dds_is_get8 (&is1); ops += 2; break;
    case DDS_OP_VAL_WCHAR: *((wchar_t *) addr) = (wchar_t) dds_is_get2 (&is1); ops += 2; break;
    case DDS_OP_VAL_STR: *((char **) addr) = dds_stream_reuse_string (&is1, *((char **) addr), allocator, sample_state); ops += 2; break;
    case DDS_OP_VAL_WSTR: *((wchar_t **) addr) = dds_stream_reuse_wstring (&is1, *((wchar_t **) addr), allocator, sample_state); ops += 2; break;
    case DDS_OP_VAL_BST: (void) dds_stream_reuse_string_bound (&is1, (char *) addr, ops[2]); ops += 3; break;
    case DDS_OP_VAL_BWSTR: (void) dds_stream_reuse_wstring_bound (&is1, (wchar_t *) addr, ops[2]); ops += 3; break;
    case DDS_OP_VAL_SEQ: case DDS_OP_VAL_BSQ: ops = dds_stream_read_seq (&is1, addr, allocator, ops, insn, cdr_kind, sample_state); break;
    case DDS_OP_VAL_ARR: ops = dds_stream_read_arr (&is1, addr, allocator, ops, insn, cdr_kind, sample_state); break;
    case DDS_OP_VAL_UNI: ops = dds_stream_read_uni (&is1, addr, data, allocator, ops, insn, cdr_kind, sample_state); break;
    case DDS_OP_VAL_ENU: {
      switch (DDS_OP_TYPE_SZ (insn))
      {
        case 1: *((uint32_t *) addr) = dds_is_get1 (&is1); break;
        case 2: *((uint32_t *) addr) = dds_is_get2 (&is1); break;
        case 4: *((uint32_t *) addr) = dds_is_get4 (&is1); break;
        default: abort ();
      }
      ops += 3;
      break;
    }
    case DDS_OP_VAL_BMK: {
      switch (DDS_OP_TYPE_SZ (insn))
      {
        case 1: *((uint8_t *) addr) = dds_is_get1 (&is1); break;
        case 2: *((uint16_t *) addr) = dds_is_get2 (&is1); break;
        case 4: *((uint32_t *) addr) = dds_is_get4 (&is1); break;
        case 8: *((uint64_t *) addr) = dds_is_get8 (&is1); break;
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

      (void) dds_stream_read_impl (&is1, addr, allocator, jsr_ops, false, cdr_kind, sample_state);
      ops += jmp ? jmp : 3;
      break;
    }
    case DDS_OP_VAL_STU: abort(); break; /* op type STU only supported as subtype */
  }

  if (is->m_xcdr_version == DDSI_RTPS_CDR_ENC_VERSION_1 && op_type_optional (insn) && !is_mutable_member)
    is->m_index += param_len;
  else
    is->m_index = is1.m_index;

  return ops;
}

ddsrt_attribute_warn_unused_result ddsrt_nonnull_all
static const uint32_t *dds_stream_skip_adr (uint32_t insn, const uint32_t *ops)
{
  switch (DDS_OP_TYPE (insn))
  {
    case DDS_OP_VAL_BLN: case DDS_OP_VAL_1BY: case DDS_OP_VAL_2BY: case DDS_OP_VAL_4BY: case DDS_OP_VAL_8BY:
    case DDS_OP_VAL_STR: case DDS_OP_VAL_WSTR: case DDS_OP_VAL_WCHAR:
      return ops + 2;
    case DDS_OP_VAL_BST:
    case DDS_OP_VAL_BWSTR:
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

ddsrt_attribute_warn_unused_result ddsrt_nonnull_all
static const uint32_t *dds_stream_skip_adr_default (uint32_t insn, char * restrict data, const struct dds_cdrstream_allocator *allocator, const uint32_t *ops, enum sample_data_state sample_state)
{
  void *addr = data + ops[1];
  /* FIXME: currently only implicit default values are used, this code should be
     using default values that are specified in the type definition */

  /* Free memory in sample and set pointer to null in case of optional member. */
  if (op_type_optional (insn))
    return stream_skip_member (insn, data, addr, allocator, ops, sample_state);

  if (op_type_external (insn))
    dds_stream_alloc_external (ops, insn, &addr, allocator, &sample_state);

  switch (DDS_OP_TYPE (insn))
  {
    case DDS_OP_VAL_BLN: case DDS_OP_VAL_1BY: *(uint8_t *) addr = 0; return ops + 2;
    case DDS_OP_VAL_2BY: *(uint16_t *) addr = 0; return ops + 2;
    case DDS_OP_VAL_4BY: *(uint32_t *) addr = 0; return ops + 2;
    case DDS_OP_VAL_8BY: *(uint64_t *) addr = 0; return ops + 2;
    case DDS_OP_VAL_WCHAR: *(wchar_t *) addr = L'\0'; return ops + 2;
    case DDS_OP_VAL_STR: *(char **) addr = dds_stream_reuse_string_empty (*(char **) addr, allocator, sample_state); return ops + 2;
    case DDS_OP_VAL_WSTR: *(wchar_t **) addr = dds_stream_reuse_wstring_empty (*(wchar_t **) addr, allocator, sample_state); return ops + 2;
    case DDS_OP_VAL_BST: ((char *) addr)[0] = '\0'; return ops + 3;
    case DDS_OP_VAL_BWSTR: ((wchar_t *) addr)[0] = L'\0'; return ops + 3;
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
      return initialize_and_skip_sequence (seq, insn, ops, sample_state);
    }
    case DDS_OP_VAL_ARR: {
      return skip_array_default (insn, addr, allocator, ops, sample_state);
    }
    case DDS_OP_VAL_UNI: {
      return skip_union_default (insn, addr, data, allocator, ops, sample_state);
    }
    case DDS_OP_VAL_EXT: {
      const uint32_t *jsr_ops = ops + DDS_OP_ADR_JSR (ops[2]);
      const uint32_t jmp = DDS_OP_ADR_JMP (ops[2]);
      (void) dds_stream_skip_default (addr, allocator, jsr_ops, sample_state);
      return ops + (jmp ? jmp : 3);
    }
    case DDS_OP_VAL_STU: {
      abort(); /* op type STU only supported as subtype */
      break;
    }
  }

  return NULL;
}

ddsrt_attribute_warn_unused_result ddsrt_nonnull_all
static const uint32_t *dds_stream_skip_delimited_default (char * restrict data, const struct dds_cdrstream_allocator *allocator, const uint32_t *ops, enum sample_data_state sample_state)
{
  return dds_stream_skip_default (data, allocator, ++ops, sample_state);
}

ddsrt_nonnull_all
static void dds_stream_skip_xcdr2_pl_member_default (char * restrict data, const struct dds_cdrstream_allocator *allocator, const uint32_t *ops, enum sample_data_state sample_state)
{
  uint32_t insn;
  while ((insn = *ops) != DDS_OP_RTS)
  {
    switch (DDS_OP (insn))
    {
      case DDS_OP_ADR: {
        ops = dds_stream_skip_default (data, allocator, ops, sample_state);
        break;
      }
      case DDS_OP_JSR:
        dds_stream_skip_xcdr2_pl_member_default (data, allocator, ops + DDS_OP_JUMP (insn), sample_state);
        ops++;
        break;
      case DDS_OP_RTS: case DDS_OP_JEQ: case DDS_OP_JEQ4: case DDS_OP_KOF:
      case DDS_OP_DLC: case DDS_OP_PLC: case DDS_OP_PLM: case DDS_OP_MID:
        abort ();
        break;
    }
  }
}

ddsrt_nonnull_all
static const uint32_t *dds_stream_skip_xcdr2_pl_memberlist_default (char * restrict data, const struct dds_cdrstream_allocator *allocator, const uint32_t *ops0, enum sample_data_state sample_state)
{
  const uint32_t *ops = ops0;
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
          (void) dds_stream_skip_xcdr2_pl_memberlist_default (data, allocator, plm_ops, sample_state);
        }
        else
        {
          dds_stream_skip_xcdr2_pl_member_default (data, allocator, plm_ops, sample_state);
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

ddsrt_attribute_warn_unused_result ddsrt_nonnull_all
static const uint32_t *dds_stream_skip_xcdr2_pl_default (char * restrict data, const struct dds_cdrstream_allocator *allocator, const uint32_t *ops, enum sample_data_state sample_state)
{
  /* skip PLC op */
  return dds_stream_skip_xcdr2_pl_memberlist_default (data, allocator, ++ops, sample_state);
}

ddsrt_attribute_warn_unused_result ddsrt_nonnull_all
static const uint32_t *dds_stream_skip_default (char * restrict data, const struct dds_cdrstream_allocator *allocator, const uint32_t *ops, enum sample_data_state sample_state)
{
  uint32_t insn;
  while ((insn = *ops) != DDS_OP_RTS)
  {
    switch (DDS_OP (insn))
    {
      case DDS_OP_ADR: {
        ops = dds_stream_skip_adr_default (insn, data, allocator, ops, sample_state);
        break;
      }
      case DDS_OP_JSR: {
        (void) dds_stream_skip_default (data, allocator, ops + DDS_OP_JUMP (insn), sample_state);
        ops++;
        break;
      }
      case DDS_OP_RTS: case DDS_OP_JEQ: case DDS_OP_JEQ4: case DDS_OP_KOF: case DDS_OP_PLM: case DDS_OP_MID:
        abort ();
        break;
      case DDS_OP_DLC:
        ops = dds_stream_skip_delimited_default (data, allocator, ops, sample_state);
        break;
      case DDS_OP_PLC:
        ops = dds_stream_skip_xcdr2_pl_default (data, allocator, ops, sample_state);
        break;
    }
  }
  return ops;
}

ddsrt_attribute_warn_unused_result ddsrt_nonnull_all
static const uint32_t *dds_stream_read_delimited (dds_istream_t *is, char * restrict data, const struct dds_cdrstream_allocator *allocator, const uint32_t *ops, enum cdr_data_kind cdr_kind, enum sample_data_state sample_state)
{
  uint32_t delimited_offs = is->m_index, insn, delimited_sz = is->m_size - is->m_index;
  ops++; // skip DLC op
  while ((insn = *ops) != DDS_OP_RTS)
  {
    switch (DDS_OP (insn))
    {
      case DDS_OP_ADR: {
        /* skip fields that are not in serialized data for appendable type */
        ops = (is->m_index - delimited_offs < delimited_sz) ? dds_stream_read_adr (insn, is, data, allocator, ops, false, cdr_kind, sample_state) : dds_stream_skip_adr_default (insn, data, allocator, ops, sample_state);
        break;
      }
      case DDS_OP_JSR: {
        (void) dds_stream_read_impl (is, data, allocator, ops + DDS_OP_JUMP (insn), false, cdr_kind, sample_state);
        ops++;
        break;
      }
      case DDS_OP_RTS: case DDS_OP_JEQ: case DDS_OP_JEQ4: case DDS_OP_KOF: case DDS_OP_DLC: case DDS_OP_PLC: case DDS_OP_PLM: case DDS_OP_MID: {
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

ddsrt_attribute_warn_unused_result ddsrt_nonnull_all
static bool dds_stream_read_xcdr2_pl_member (dds_istream_t *is, char * restrict data, const struct dds_cdrstream_allocator *allocator, uint32_t m_id, const uint32_t *ops, enum cdr_data_kind cdr_kind, enum sample_data_state sample_state)
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
      found = dds_stream_read_xcdr2_pl_member (is, data, allocator, m_id, plm_ops, cdr_kind, sample_state);
    }
    else if (ops[ops_csr + 1] == m_id)
    {
      (void) dds_stream_read_impl (is, data, allocator, plm_ops, true, cdr_kind, sample_state);
      found = true;
      break;
    }
    ops_csr += 2;
  }
  return found;
}

ddsrt_attribute_warn_unused_result ddsrt_nonnull_all
static const uint32_t *dds_stream_read_xcdr2_pl (dds_istream_t *is, char * restrict data, const struct dds_cdrstream_allocator *allocator, const uint32_t *ops, enum cdr_data_kind cdr_kind, enum sample_data_state sample_state)
{
  /* skip PLC op */
  ops++;

  /* default-initialize all members
      FIXME: optimize so that only members not in received data are initialized */
  dds_stream_skip_xcdr2_pl_memberlist_default (data, allocator, ops, sample_state);

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
    if (!dds_stream_read_xcdr2_pl_member (is, data, allocator, m_id, ops, cdr_kind, sample_state))
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

ddsrt_attribute_warn_unused_result ddsrt_nonnull_all
static const uint32_t *dds_stream_read_impl (dds_istream_t *is, char * restrict data, const struct dds_cdrstream_allocator *allocator, const uint32_t *ops, bool is_mutable_member, enum cdr_data_kind cdr_kind, enum sample_data_state sample_state)
{
  uint32_t insn;
  while ((insn = *ops) != DDS_OP_RTS)
  {
    switch (DDS_OP (insn))
    {
      case DDS_OP_ADR:
        ops = dds_stream_read_adr (insn, is, data, allocator, ops, is_mutable_member, cdr_kind, sample_state);
        break;
      case DDS_OP_JSR:
        (void) dds_stream_read_impl (is, data, allocator, ops + DDS_OP_JUMP (insn), is_mutable_member, cdr_kind, sample_state);
        ops++;
        break;
      case DDS_OP_RTS: case DDS_OP_JEQ: case DDS_OP_JEQ4: case DDS_OP_KOF: case DDS_OP_PLM: case DDS_OP_MID:
        abort ();
        break;
      case DDS_OP_DLC: {
        dds_istream_t is1 = *is;
        if (is->m_xcdr_version == DDSI_RTPS_CDR_ENC_VERSION_2)
        {
          uint32_t delimited_sz = dds_is_get4 (is);
          is1.m_size = is->m_index + delimited_sz;
          is1.m_index = is->m_index;

          is->m_index += delimited_sz;
        }
        ops = dds_stream_read_delimited (&is1, data, allocator, ops, cdr_kind, sample_state);
        if (is->m_xcdr_version == DDSI_RTPS_CDR_ENC_VERSION_1)
          is->m_index = is1.m_index;
        break;
      }
      case DDS_OP_PLC:
        assert (is->m_xcdr_version == DDSI_RTPS_CDR_ENC_VERSION_2);
        ops = dds_stream_read_xcdr2_pl (is, data, allocator, ops, cdr_kind, sample_state);
        break;
    }
  }
  return ops;
}

const uint32_t *dds_stream_read (dds_istream_t *is, char * restrict data, const struct dds_cdrstream_allocator *allocator, const uint32_t *ops)
{
  return dds_stream_read_impl (is, data, allocator, ops, false, CDR_KIND_DATA, SAMPLE_DATA_INITIALIZED);
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

ddsrt_attribute_warn_unused_result ddsrt_nonnull_all
static bool normalize_uint8 (uint32_t *off, uint32_t size)
{
  if (*off == size)
    return normalize_error_bool ();
  (*off)++;
  return true;
}

ddsrt_attribute_warn_unused_result ddsrt_nonnull_all
static bool normalize_uint16 (char * restrict data, uint32_t * restrict off, uint32_t size, bool bswap)
{
  if ((*off = check_align_prim (*off, size, 1, 1)) == UINT32_MAX)
    return false;
  if (bswap)
    *((uint16_t *) (data + *off)) = ddsrt_bswap2u (*((uint16_t *) (data + *off)));
  (*off) += 2;
  return true;
}

ddsrt_attribute_warn_unused_result ddsrt_nonnull_all
static bool normalize_uint32 (char * restrict data, uint32_t * restrict off, uint32_t size, bool bswap)
{
  if ((*off = check_align_prim (*off, size, 2, 2)) == UINT32_MAX)
    return false;
  if (bswap)
    *((uint32_t *) (data + *off)) = ddsrt_bswap4u (*((uint32_t *) (data + *off)));
  (*off) += 4;
  return true;
}

ddsrt_attribute_warn_unused_result ddsrt_nonnull_all
static bool normalize_uint64 (char * restrict data, uint32_t * restrict off, uint32_t size, bool bswap, uint32_t xcdr_version)
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

ddsrt_nonnull_all
static bool normalize_bool (char * restrict data, uint32_t * restrict off, uint32_t size)
{
  if (*off == size)
    return normalize_error_bool ();
  uint8_t b = *((uint8_t *) (data + *off));
  if (b > 1) // correct the representation of true
    *((uint8_t *) (data + *off)) = 1;
  (*off)++;
  return true;
}

ddsrt_attribute_warn_unused_result ddsrt_nonnull_all
static bool read_and_normalize_bool (bool * restrict val, char * restrict data, uint32_t * restrict off, uint32_t size)
{
  if (*off == size)
    return normalize_error_bool ();
  uint8_t b = *((uint8_t *) (data + *off));
  if (b > 1) // correct the representation of true
    *((uint8_t *) (data + *off)) = 1;
  *val = b;
  (*off)++;
  return true;
}

ddsrt_attribute_warn_unused_result ddsrt_nonnull_all
static inline bool read_and_normalize_uint8 (uint8_t * restrict val, char * restrict data, uint32_t * restrict off, uint32_t size)
{
  if ((*off = check_align_prim (*off, size, 0, 0)) == UINT32_MAX)
    return false;
  *val = *((uint8_t *) (data + *off));
  (*off)++;
  return true;
}

ddsrt_attribute_warn_unused_result ddsrt_nonnull_all
static inline bool read_and_normalize_uint16 (uint16_t * restrict val, char * restrict data, uint32_t * restrict off, uint32_t size, bool bswap)
{
  if ((*off = check_align_prim (*off, size, 1, 1)) == UINT32_MAX)
    return false;
  if (bswap)
    *((uint16_t *) (data + *off)) = ddsrt_bswap2u (*((uint16_t *) (data + *off)));
  *val = *((uint16_t *) (data + *off));
  (*off) += 2;
  return true;
}

ddsrt_attribute_warn_unused_result ddsrt_nonnull_all
static inline bool read_and_normalize_uint32 (uint32_t * restrict val, char * restrict data, uint32_t * restrict off, uint32_t size, bool bswap)
{
  if ((*off = check_align_prim (*off, size, 2, 2)) == UINT32_MAX)
    return false;
  if (bswap)
    *((uint32_t *) (data + *off)) = ddsrt_bswap4u (*((uint32_t *) (data + *off)));
  *val = *((uint32_t *) (data + *off));
  (*off) += 4;
  return true;
}

ddsrt_attribute_warn_unused_result ddsrt_nonnull_all
static inline bool read_and_normalize_uint64 (uint64_t * restrict val, char * restrict data, uint32_t * restrict off, uint32_t size, bool bswap, uint32_t xcdr_version)
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

ddsrt_attribute_warn_unused_result ddsrt_nonnull_all
static bool peek_and_normalize_uint32 (uint32_t * restrict val, char * restrict data, uint32_t * restrict off, uint32_t size, bool bswap)
{
  if ((*off = check_align_prim (*off, size, 2, 2)) == UINT32_MAX)
    return false;
  if (bswap)
    *val = ddsrt_bswap4u (*((uint32_t *) (data + *off)));
  else
    *val = *((uint32_t *) (data + *off));
  return true;
}

ddsrt_attribute_warn_unused_result ddsrt_nonnull((1,2,3))
static bool read_normalize_enum (uint32_t * restrict val, char * restrict data, uint32_t * restrict off, uint32_t size, bool bswap, uint32_t insn, uint32_t max)
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

ddsrt_attribute_warn_unused_result ddsrt_nonnull_all
static bool normalize_enum (char * restrict data, uint32_t * restrict off, uint32_t size, bool bswap, uint32_t insn, uint32_t max)
{
  uint32_t val;
  return read_normalize_enum (&val, data, off, size, bswap, insn, max);
}

ddsrt_attribute_warn_unused_result ddsrt_nonnull((1,2,3))
static bool read_normalize_bitmask (uint64_t * restrict val, char * restrict data, uint32_t * restrict off, uint32_t size, bool bswap, uint32_t xcdr_version, uint32_t insn, uint32_t bits_h, uint32_t bits_l)
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

ddsrt_attribute_warn_unused_result ddsrt_nonnull_all
static bool normalize_bitmask (char * restrict data, uint32_t * restrict off, uint32_t size, bool bswap, uint32_t xcdr_version, uint32_t insn, uint32_t bits_h, uint32_t bits_l)
{
  uint64_t val;
  return read_normalize_bitmask (&val, data, off, size, bswap, xcdr_version, insn, bits_h, bits_l);
}

ddsrt_attribute_warn_unused_result ddsrt_nonnull_all
static bool normalize_string (char * restrict data, uint32_t * restrict off, uint32_t size, bool bswap, size_t maxsz)
{
  // maxsz = character count, includes terminating '\0' that is in-memory and on the wire
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

ddsrt_attribute_warn_unused_result ddsrt_nonnull_all
static inline bool normalize_wchar (char * restrict data, uint32_t * restrict off, uint32_t size, bool bswap)
{
  if ((*off = check_align_prim (*off, size, 1, 1)) == UINT32_MAX)
    return false;
  if (bswap)
    *((uint16_t *) (data + *off)) = ddsrt_bswap2u (*((uint16_t *) (data + *off)));
  const uint16_t val = *((uint16_t *) (data + *off));
  // surrogates are disallowed
  if (val >= 0xd800 && val < 0xe000)
    return false;
  (*off) += 2;
  return true;
}

ddsrt_attribute_warn_unused_result ddsrt_nonnull_all
static bool normalize_wstring (char * restrict data, uint32_t * restrict off, uint32_t size, bool bswap, size_t maxsz)
{
  // maxsz = character count, includes terminating L'\0' that is in-memory
  // CDR stream contains number of bytes (so must be even), excluding termating L'\0'
  // taking the maximum allowed count on the wire as number uint16_t, not as number of code points (which may
  // be lower if there are surrogate pairs in the input)
  uint32_t sz;
  assert (maxsz > 0);
  if (!read_and_normalize_uint32 (&sz, data, off, size, bswap))
    return false;
  if ((size % 2) != 0 || size - *off < sz || maxsz - 1 < sz / 2)
    return normalize_error_bool ();
  // even, fits in input and fits in bound
  if (bswap)
    dds_stream_swap (data + *off, 2, sz / 2);
  // verify surrogate pairs are used correctly
  {
    const uint16_t *str = (const uint16_t *) (data + *off);
    const uint32_t len = sz / 2;
    uint32_t i = 0;
    while (i < len)
    {
      if (str[i] < 0xd800 || str[i] >= 0xe000) {
        i++;
      } else if (str[i] >= 0xd800 && str[i] < 0xdc00) {
        // first half of surrogate pair, must have second half
        i++;
        if (i == len || str[i] < 0xdc00 || str[i] > 0xdfff)
          return normalize_error_bool ();
        i++;
      } else {
        // second half without first half
        return normalize_error_bool ();
      }
    }
  }
  *off += sz;
  return true;
}

ddsrt_attribute_warn_unused_result ddsrt_nonnull_all
static bool normalize_boolarray (char * restrict data, uint32_t * restrict off, uint32_t size, uint32_t num)
{
  if ((*off = check_align_prim_many (*off, size, 0, 0, num)) == UINT32_MAX)
    return false;
  uint8_t * const xs = (uint8_t *) (data + *off);
  for (uint32_t i = 0; i < num; i++)
    if (xs[i] > 1)
      xs[i] = 1;
  *off += num;
  return true;
}

ddsrt_attribute_warn_unused_result ddsrt_nonnull_all
static bool normalize_primarray (char * restrict data, uint32_t * restrict off, uint32_t size, bool bswap, uint32_t num, enum dds_stream_typecode type, uint32_t xcdr_version)
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

ddsrt_attribute_warn_unused_result ddsrt_nonnull_all
static bool normalize_enumarray (char * restrict data, uint32_t * restrict off, uint32_t size, bool bswap, uint32_t enum_sz, uint32_t num, uint32_t max)
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

ddsrt_attribute_warn_unused_result ddsrt_nonnull_all
static bool normalize_bitmaskarray (char * restrict data, uint32_t * restrict off, uint32_t size, bool bswap, uint32_t xcdr_version, uint32_t insn, uint32_t num, uint32_t bits_h, uint32_t bits_l)
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

ddsrt_attribute_warn_unused_result ddsrt_nonnull_all
static bool read_and_normalize_collection_dheader (bool * restrict has_dheader, uint32_t * restrict size1, char * restrict data, uint32_t * restrict off, uint32_t size, bool bswap, const enum dds_stream_typecode subtype, uint32_t xcdr_version)
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

ddsrt_attribute_warn_unused_result ddsrt_nonnull_all
static const uint32_t *normalize_seq (char * restrict data, uint32_t * restrict off, uint32_t size, bool bswap, uint32_t xcdr_version, const struct dds_cdrstream_desc_mid_table *mid_table, const uint32_t *ops, uint32_t insn, enum cdr_data_kind cdr_kind)
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
    case DDS_OP_VAL_WSTR: case DDS_OP_VAL_BWSTR: {
      const size_t maxsz = (subtype == DDS_OP_VAL_WSTR) ? SIZE_MAX : ops[2 + bound_op];
      for (uint32_t i = 0; i < num; i++)
        if (!normalize_wstring (data, off, size1, bswap, maxsz))
          return NULL;
      ops += (subtype == DDS_OP_VAL_WSTR ? 2 : 3) + bound_op;
      break;
    }
    case DDS_OP_VAL_WCHAR: {
      for (uint32_t i = 0; i < num; i++)
        if (!normalize_wchar (data, off, size1, bswap))
          return NULL;
      ops += 2 + bound_op;
      break;
    }
    case DDS_OP_VAL_SEQ: case DDS_OP_VAL_BSQ: case DDS_OP_VAL_ARR: case DDS_OP_VAL_UNI: case DDS_OP_VAL_STU: {
      const uint32_t jmp = DDS_OP_ADR_JMP (ops[3 + bound_op]);
      uint32_t const * const jsr_ops = ops + DDS_OP_ADR_JSR (ops[3 + bound_op]);
      for (uint32_t i = 0; i < num; i++)
        if (stream_normalize_data_impl (data, off, size1, bswap, xcdr_version, mid_table, jsr_ops, false, cdr_kind) == NULL)
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

ddsrt_attribute_warn_unused_result ddsrt_nonnull_all
static const uint32_t *normalize_arr (char * restrict data, uint32_t * restrict off, uint32_t size, bool bswap, uint32_t xcdr_version, const struct dds_cdrstream_desc_mid_table *mid_table, const uint32_t *ops, uint32_t insn, enum cdr_data_kind cdr_kind)
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
      if (!normalize_boolarray (data, off, size1, num))
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
    case DDS_OP_VAL_WSTR: case DDS_OP_VAL_BWSTR: {
      const size_t maxsz = (subtype == DDS_OP_VAL_WSTR) ? SIZE_MAX : ops[4];
      for (uint32_t i = 0; i < num; i++)
        if (!normalize_wstring (data, off, size1, bswap, maxsz))
          return NULL;
      ops += (subtype == DDS_OP_VAL_WSTR) ? 3 : 5;
      break;
    }
    case DDS_OP_VAL_WCHAR: {
      for (uint32_t i = 0; i < num; i++)
        if (!normalize_wchar (data, off, size1, bswap))
          return NULL;
      ops += 3;
      break;
    }
    case DDS_OP_VAL_SEQ: case DDS_OP_VAL_BSQ: case DDS_OP_VAL_ARR: case DDS_OP_VAL_UNI: case DDS_OP_VAL_STU: {
      const uint32_t jmp = DDS_OP_ADR_JMP (ops[3]);
      uint32_t const * const jsr_ops = ops + DDS_OP_ADR_JSR (ops[3]);
      for (uint32_t i = 0; i < num; i++)
        if (stream_normalize_data_impl (data, off, size1, bswap, xcdr_version, mid_table, jsr_ops, false, cdr_kind) == NULL)
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

ddsrt_attribute_warn_unused_result ddsrt_nonnull_all
static bool normalize_uni_disc (uint32_t * restrict val, char * restrict data, uint32_t * restrict off, uint32_t size, bool bswap, uint32_t insn, const uint32_t *ops)
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

ddsrt_attribute_warn_unused_result ddsrt_nonnull_all
static const uint32_t *normalize_uni (char * restrict data, uint32_t * restrict off, uint32_t size, bool bswap, uint32_t xcdr_version, const struct dds_cdrstream_desc_mid_table *mid_table, const uint32_t *ops, uint32_t insn, enum cdr_data_kind cdr_kind)
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
      case DDS_OP_VAL_WCHAR: if (!normalize_wchar (data, off, size, bswap)) return NULL; break;
      case DDS_OP_VAL_STR: if (!normalize_string (data, off, size, bswap, SIZE_MAX)) return NULL; break;
      case DDS_OP_VAL_WSTR: if (!normalize_wstring (data, off, size, bswap, SIZE_MAX)) return NULL; break;
      case DDS_OP_VAL_ENU: if (!normalize_enum (data, off, size, bswap, jeq_op[0], jeq_op[3])) return NULL; break;
      case DDS_OP_VAL_BST: case DDS_OP_VAL_BWSTR: case DDS_OP_VAL_SEQ: case DDS_OP_VAL_BSQ: case DDS_OP_VAL_ARR: case DDS_OP_VAL_UNI: case DDS_OP_VAL_STU: case DDS_OP_VAL_BMK:
        if (stream_normalize_data_impl (data, off, size, bswap, xcdr_version, mid_table, jeq_op + DDS_OP_ADR_JSR (jeq_op[0]), false, cdr_kind) == NULL)
          return NULL;
        break;
      case DDS_OP_VAL_EXT:
        abort (); /* not supported */
        break;
    }
  }
  return ops;
}

enum normalize_xcdr1_paramheader_result {
  NPHR1_NOT_FOUND,   // unknown memberid, param_length and must_understand set
  NPHR1_NOT_PRESENT, // known memberid, param_length = 0, must_understand set
  NPHR1_PRESENT,     // known memberid, param_length != 0, must_understand set
  NPHR1_ERROR        // normalization failed; param_length and must_understand undefined
};

ddsrt_nonnull_all
static enum normalize_xcdr1_paramheader_result stream_read_normalize_xcdr1_paramheader (char * restrict data, uint32_t * restrict off, uint32_t size, bool bswap, uint32_t *param_length, bool *must_understand, const struct dds_cdrstream_desc_mid_table *mid_table, const uint32_t *adr_op)
{
  uint32_t phdr, phdr_mid, plen;
  if (!read_and_normalize_uint32 (&phdr, data, off, size, bswap))
    return NPHR1_ERROR;
  if ((phdr & DDS_XCDR1_PL_SHORT_PID_MASK) == DDS_XCDR1_PL_SHORT_PID_EXTENDED)
  {
    // Extended header

    // Check length and must understand in short param header
    if ((phdr & DDS_XCDR1_PL_SHORT_LEN_MASK) != DDS_XCDR1_PL_SHORT_PID_EXT_LEN)
      return NPHR1_ERROR;
    if (!(phdr & DDS_XCDR1_PL_SHORT_FLAG_MU))
      return NPHR1_ERROR;

    // Read and check the extended parameter ID
    uint32_t pid;
    if (!read_and_normalize_uint32 (&pid, data, off, size, bswap))
      return NPHR1_ERROR;
    if (pid & DDS_XCDR1_PL_LONG_FLAG_IMPL_EXT)
      return NPHR1_ERROR;
    phdr_mid = pid & DDS_XCDR1_PL_LONG_MID_MASK;
    *must_understand = (pid & DDS_XCDR1_PL_LONG_FLAG_MU);

    // Read the extended parameter length
    if (!read_and_normalize_uint32 (&plen, data, off, size, bswap))
      return NPHR1_ERROR;
    *param_length = plen;
    // reject if fewer than plen bytes remain in the input
    if (plen > size - *off)
    {
      normalize_error ();
      return NPHR1_ERROR;
    }
  }
  else
  {
    // Short header
    *must_understand = (phdr & DDS_XCDR1_PL_SHORT_FLAG_MU);
    plen = (uint32_t) (phdr & DDS_XCDR1_PL_SHORT_LEN_MASK);
    *param_length = plen;
    // reject if fewer than plen bytes remain in the input
    if (plen > size - *off)
    {
      normalize_error ();
      return NPHR1_ERROR;
    }

    uint32_t pid = (phdr & DDS_XCDR1_PL_SHORT_PID_MASK);
    if (pid == DDS_XCDR1_PL_SHORT_PID_LIST_END)
      return NPHR1_ERROR;
    if (pid >= 0x3F04 && pid <= 0x3FFF) // reserved value
      return NPHR1_NOT_FOUND;
    phdr_mid = pid;
  }

  uint32_t adr_mid;
  if (!find_member_id (mid_table, adr_op, &adr_mid))
    return NPHR1_NOT_FOUND;
  if (adr_mid != phdr_mid)
    return NPHR1_ERROR;

  return plen > 0 ? NPHR1_PRESENT : NPHR1_NOT_PRESENT;
}

ddsrt_nonnull_all
static const uint32_t *stream_normalize_adr_impl (uint32_t insn, char * restrict data, uint32_t * restrict off, uint32_t size, bool bswap, uint32_t xcdr_version, const struct dds_cdrstream_desc_mid_table *mid_table, const uint32_t *ops, enum cdr_data_kind cdr_kind)
{
  switch (DDS_OP_TYPE (insn))
  {
    case DDS_OP_VAL_BLN: if (!normalize_bool (data, off, size)) return NULL; ops += 2; break;
    case DDS_OP_VAL_1BY: if (!normalize_uint8 (off, size)) return NULL; ops += 2; break;
    case DDS_OP_VAL_2BY: if (!normalize_uint16 (data, off, size, bswap)) return NULL; ops += 2; break;
    case DDS_OP_VAL_4BY: if (!normalize_uint32 (data, off, size, bswap)) return NULL; ops += 2; break;
    case DDS_OP_VAL_8BY: if (!normalize_uint64 (data, off, size, bswap, xcdr_version)) return NULL; ops += 2; break;
    case DDS_OP_VAL_STR: if (!normalize_string (data, off, size, bswap, SIZE_MAX)) return NULL; ops += 2; break;
    case DDS_OP_VAL_WSTR: if (!normalize_wstring (data, off, size, bswap, SIZE_MAX)) return NULL; ops += 2; break;
    case DDS_OP_VAL_BST: if (!normalize_string (data, off, size, bswap, ops[2])) return NULL; ops += 3; break;
    case DDS_OP_VAL_BWSTR: if (!normalize_wstring (data, off, size, bswap, ops[2])) return NULL; ops += 3; break;
    case DDS_OP_VAL_WCHAR: if (!normalize_wchar (data, off, size, bswap)) return NULL; ops += 2; break;
    case DDS_OP_VAL_SEQ: case DDS_OP_VAL_BSQ: ops = normalize_seq (data, off, size, bswap, xcdr_version, mid_table, ops, insn, cdr_kind); if (!ops) return NULL; break;
    case DDS_OP_VAL_ARR: ops = normalize_arr (data, off, size, bswap, xcdr_version, mid_table, ops, insn, cdr_kind); if (!ops) return NULL; break;
    case DDS_OP_VAL_UNI: ops = normalize_uni (data, off, size, bswap, xcdr_version, mid_table, ops, insn, cdr_kind); if (!ops) return NULL; break;
    case DDS_OP_VAL_ENU: if (!normalize_enum (data, off, size, bswap, insn, ops[2])) return NULL; ops += 3; break;
    case DDS_OP_VAL_BMK: if (!normalize_bitmask (data, off, size, bswap, xcdr_version, insn, ops[2], ops[3])) return NULL; ops += 4; break;
    case DDS_OP_VAL_EXT: {
      const uint32_t *jsr_ops = ops + DDS_OP_ADR_JSR (ops[2]);
      const uint32_t jmp = DDS_OP_ADR_JMP (ops[2]);

      /* skip DLC instruction for base type, the base type members are not preceded by a DHEADER */
      if (op_type_base (insn) && jsr_ops[0] == DDS_OP_DLC)
        jsr_ops++;

      if (stream_normalize_data_impl (data, off, size, bswap, xcdr_version, mid_table, jsr_ops, false, cdr_kind) == NULL)
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

ddsrt_attribute_warn_unused_result ddsrt_nonnull_all
static const uint32_t *stream_normalize_adr (uint32_t insn, char * restrict data, uint32_t * restrict off, uint32_t size, bool bswap, uint32_t xcdr_version, const struct dds_cdrstream_desc_mid_table *mid_table, const uint32_t *ops, bool is_mutable_member, enum cdr_data_kind cdr_kind)
{
  const bool is_key = (insn & DDS_OP_FLAG_KEY);
  if (cdr_kind == CDR_KIND_KEY && !is_key)
    return dds_stream_skip_adr (insn, ops);

  if (!op_type_optional (insn) || is_mutable_member)
  {
    return stream_normalize_adr_impl (insn, data, off, size, bswap, xcdr_version, mid_table, ops, cdr_kind);
  }
  else if (xcdr_version != DDSI_RTPS_CDR_ENC_VERSION_1)
  {
    bool present = true;
    if (!read_and_normalize_bool (&present, data, off, size))
      return NULL;
    if (!present)
      return dds_stream_skip_adr (insn, ops);
    else
      return stream_normalize_adr_impl (insn, data, off, size, bswap, xcdr_version, mid_table, ops, cdr_kind);
  }
  else // xcdr_version == DDSI_RTPS_CDR_ENC_VERSION_1
  {
    uint32_t param_length = 0;
    bool must_understand = false;
    switch (stream_read_normalize_xcdr1_paramheader (data, off, size, bswap, &param_length, &must_understand, mid_table, ops))
    {
      case NPHR1_ERROR:
        return NULL;
      case NPHR1_NOT_FOUND:
        if (must_understand) // must_understand and unknown means we have to reject the input
          return NULL;
        *off += param_length;
        /* fall through */
      case NPHR1_NOT_PRESENT:
        ops = dds_stream_skip_adr (insn, ops);
        break;
      case NPHR1_PRESENT: {
        // alignment rules madness: XCDR1 serialization of optionals/mutable shifts
        // reference position, so int64/float64 alignment is no longer always at
        // multiple of 8:
        //
        //    Member of mutable aggregated type (structure, union), version 1 encoding
        //    using short PL encoding when both M.id <= 2^14 and M.value.ssize <= 2^16
        //    (24) XCDR[1] << {M : MMEMBER} =
        //        XCDR
        //        << ALIGN(4)
        //        << { FLAG_I + FLAG_M + M.id : UInt16 }
        //        << { M.value.ssize : UInt16 }
        //        << PUSH( ORIGIN=0 )
        //        << { M.value : M.value.type }
        //
        //    Member of mutable aggregated type (structure, union), version 1 encoding
        //    using long PL encoding
        //    (25) XCDR[1] << {M : MMEMBER} =
        //        XCDR
        //        << ALIGN(4)
        //        << { FLAG_I + FLAG_M + PID_EXTENDED : UInt16 }
        //        << { slength=8 : UInt16 }
        //        << { M.id : UInt32 }
        //        << { M.value.ssize : UInt32 }
        //        << PUSH( ORIGIN=0 )
        //        << { M.value : M.value.type }
        //
        // (with a presumed-missing POP(ORIGIN) at the end of both)
        const uint32_t input_offset = *off;
        uint32_t off1 = 0;
        if ((ops = stream_normalize_adr_impl (insn, data + input_offset, &off1, param_length, bswap, xcdr_version, mid_table, ops, cdr_kind)) == NULL)
          return NULL;
        assert (off1 <= param_length);
        // move forward by parameter length, ignoring any extraneous bytes
        *off += param_length;
        break;
      }
    }
    return ops;
  }
}

ddsrt_attribute_warn_unused_result ddsrt_nonnull_all
static const uint32_t *stream_normalize_delimited (char * restrict data, uint32_t * restrict off, uint32_t size, bool bswap, uint32_t xcdr_version, const struct dds_cdrstream_desc_mid_table *mid_table, const uint32_t *ops, enum cdr_data_kind cdr_kind)
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
        if ((ops = stream_normalize_adr (insn, data, off, size1, bswap, xcdr_version, mid_table, ops, false, cdr_kind)) == NULL)
          return NULL;
        break;
      case DDS_OP_JSR:
        if (stream_normalize_data_impl (data, off, size1, bswap, xcdr_version, mid_table, ops + DDS_OP_JUMP (insn), false, cdr_kind) == NULL)
          return NULL;
        ops++;
        break;
      case DDS_OP_RTS: case DDS_OP_JEQ: case DDS_OP_JEQ4: case DDS_OP_KOF: case DDS_OP_DLC: case DDS_OP_PLC: case DDS_OP_PLM: case DDS_OP_MID:
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

enum normalize_xcdr2_pl_member_result {
  NPMR2_NOT_FOUND,
  NPMR2_FOUND,
  NPMR2_ERROR // found the data, but normalization failed
};

ddsrt_attribute_warn_unused_result ddsrt_nonnull_all
static enum normalize_xcdr2_pl_member_result dds_stream_normalize_xcdr2_pl_member (char * restrict data, uint32_t m_id, uint32_t * restrict off, uint32_t size, bool bswap, uint32_t xcdr_version, const struct dds_cdrstream_desc_mid_table *mid_table, const uint32_t *ops, enum cdr_data_kind cdr_kind)
{
  uint32_t insn, ops_csr = 0;
  enum normalize_xcdr2_pl_member_result result = NPMR2_NOT_FOUND;
  while (result == NPMR2_NOT_FOUND && (insn = ops[ops_csr]) != DDS_OP_RTS)
  {
    assert (DDS_OP (insn) == DDS_OP_PLM);
    uint32_t flags = DDS_PLM_FLAGS (insn);
    const uint32_t *plm_ops = ops + ops_csr + DDS_OP_ADR_PLM (insn);
    if (flags & DDS_OP_FLAG_BASE)
    {
      assert (DDS_OP (plm_ops[0]) == DDS_OP_PLC);
      plm_ops++; /* skip PLC to go to first PLM from base type */
      result = dds_stream_normalize_xcdr2_pl_member (data, m_id, off, size, bswap, xcdr_version, mid_table, plm_ops, cdr_kind);
    }
    else if (ops[ops_csr + 1] == m_id)
    {
      if (stream_normalize_data_impl (data, off, size, bswap, xcdr_version, mid_table, plm_ops, true, cdr_kind))
        result = NPMR2_FOUND;
      else
        result = NPMR2_ERROR;
      break;
    }
    ops_csr += 2;
  }
  return result;
}

ddsrt_attribute_warn_unused_result ddsrt_nonnull_all
static const uint32_t *stream_normalize_xcdr2_pl (char * restrict data, uint32_t * restrict off, uint32_t size, bool bswap, uint32_t xcdr_version, const struct dds_cdrstream_desc_mid_table *mid_table, const uint32_t *ops, enum cdr_data_kind cdr_kind)
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
    switch (dds_stream_normalize_xcdr2_pl_member (data, m_id, off, size2, bswap, xcdr_version, mid_table, ops, cdr_kind))
    {
      case NPMR2_NOT_FOUND:
        /* FIXME: the caller should be able to differentiate between a sample that
           is dropped because of an unknown member that has the must-understand flag
           and a sample that is dropped because the data is invalid. This requires
           changes in the cdrstream interface, but also in the serdata interface to
           pass the return value to ddsi_receive. */
        if (must_understand)
          return normalize_error_ops ();
        *off = size2;
        break;
      case NPMR2_FOUND:
        if (*off != size2)
          return normalize_error_ops ();
        break;
      case NPMR2_ERROR:
        return NULL;
    }
  }

  /* skip all PLM-memberid pairs */
  while (ops[0] != DDS_OP_RTS)
    ops += 2;

  return ops;
}

ddsrt_attribute_warn_unused_result ddsrt_nonnull_all
static const uint32_t *stream_normalize_data_impl (char * restrict data, uint32_t * restrict off, uint32_t size, bool bswap, uint32_t xcdr_version, const struct dds_cdrstream_desc_mid_table *mid_table, const uint32_t *ops, bool is_mutable_member, enum cdr_data_kind cdr_kind)
{
  uint32_t insn;
  while ((insn = *ops) != DDS_OP_RTS)
  {
    switch (DDS_OP (insn))
    {
      case DDS_OP_ADR: {
        if ((ops = stream_normalize_adr (insn, data, off, size, bswap, xcdr_version, mid_table, ops, is_mutable_member, cdr_kind)) == NULL)
          return NULL;
        break;
      }
      case DDS_OP_JSR: {
        if (stream_normalize_data_impl (data, off, size, bswap, xcdr_version, mid_table, ops + DDS_OP_JUMP (insn), is_mutable_member, cdr_kind) == NULL)
          return NULL;
        ops++;
        break;
      }
      case DDS_OP_RTS: case DDS_OP_JEQ: case DDS_OP_JEQ4: case DDS_OP_KOF: case DDS_OP_PLM: case DDS_OP_MID: {
        abort ();
        break;
      }
      case DDS_OP_DLC: {
        if (xcdr_version != DDSI_RTPS_CDR_ENC_VERSION_2)
          return normalize_error_ops ();
        if ((ops = stream_normalize_delimited (data, off, size, bswap, xcdr_version, mid_table, ops, cdr_kind)) == NULL)
          return NULL;
        break;
      }
      case DDS_OP_PLC: {
        if (xcdr_version != DDSI_RTPS_CDR_ENC_VERSION_2)
          return normalize_error_ops ();
        if ((ops = stream_normalize_xcdr2_pl (data, off, size, bswap, xcdr_version, mid_table, ops, cdr_kind)) == NULL)
          return NULL;
        break;
      }
    }
  }
  return ops;
}

const uint32_t *dds_stream_normalize_xcdr2_data (char * restrict data, uint32_t * restrict off, uint32_t size, bool bswap, const uint32_t *ops)
{
  const struct dds_cdrstream_desc_mid_table empty_mid_table = { .table = (struct ddsrt_hh *) &ddsrt_hh_empty, .op0 = ops };
  return stream_normalize_data_impl (data, off, size, bswap, DDSI_RTPS_CDR_ENC_VERSION_2, &empty_mid_table, ops, false, CDR_KIND_DATA);
}

ddsrt_attribute_warn_unused_result ddsrt_nonnull ((1, 3, 6, 7))
static bool stream_normalize_key_impl (void * restrict data, uint32_t size, uint32_t *offs, bool bswap, uint32_t xcdr_version, const struct dds_cdrstream_desc_mid_table *mid_table, const uint32_t *ops, uint16_t key_offset_count, const uint32_t * key_offset_insn)
{
  uint32_t insn = ops[0];
  assert (key_optimized_allowed (insn));
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
    case DDS_OP_VAL_WSTR: if (!normalize_wstring (data, offs, size, bswap, SIZE_MAX)) return false; break;
    case DDS_OP_VAL_BST: if (!normalize_string (data, offs, size, bswap, ops[2])) return false; break;
    case DDS_OP_VAL_BWSTR: if (!normalize_wstring (data, offs, size, bswap, ops[2])) return false; break;
    case DDS_OP_VAL_WCHAR: if (!normalize_wchar (data, offs, size, bswap)) return false; break;
    case DDS_OP_VAL_ARR: if (!normalize_arr (data, offs, size, bswap, xcdr_version, mid_table, ops, insn, true)) return false; break;
    case DDS_OP_VAL_EXT: {
      assert (key_offset_count > 0);
      const uint32_t *jsr_ops = ops + DDS_OP_ADR_JSR (ops[2]) + *key_offset_insn;
      if (!stream_normalize_key_impl (data, size, offs, bswap, xcdr_version, mid_table, jsr_ops, --key_offset_count, ++key_offset_insn))
        return false;
      break;
    }
    case DDS_OP_VAL_SEQ: case DDS_OP_VAL_BSQ: case DDS_OP_VAL_UNI: case DDS_OP_VAL_STU:
      abort ();
      break;
  }
  return true;
}

ddsrt_attribute_warn_unused_result ddsrt_nonnull_all
static bool stream_normalize_key (void * restrict data, uint32_t size, bool bswap, uint32_t xcdr_version, const struct dds_cdrstream_desc *desc, uint32_t *actual_size)
{
  uint32_t offs = 0;

  if (desc->flagset & (DDS_TOPIC_KEY_APPENDABLE | DDS_TOPIC_KEY_MUTABLE | DDS_TOPIC_KEY_SEQUENCE | DDS_TOPIC_KEY_ARRAY_NONPRIM))
  {
    /* For types with key fields in aggregated types with appendable or mutable
       extensibility, use the regular normalize functions */
    if (stream_normalize_data_impl (data, &offs, size, bswap, xcdr_version, &desc->member_ids, desc->ops.ops, false, CDR_KIND_KEY) == NULL)
      return false;
  }
  else
  {
    /* For types that only have key fields in aggregated types with final
       extensibility, iterate over the keys (in definition order) and jump
       to key offset in ops */
    for (uint32_t i = 0; i < desc->keys.nkeys; i++)
    {
      const uint32_t *op = desc->ops.ops + desc->keys.keys_definition_order[i].ops_offs;
      switch (DDS_OP (*op))
      {
        case DDS_OP_KOF: {
          uint16_t n_offs = DDS_OP_LENGTH (*op);
          if (!stream_normalize_key_impl (data, size, &offs, bswap, xcdr_version, &desc->member_ids, desc->ops.ops + op[1], --n_offs, op + 2))
            return false;
          break;
        }
        case DDS_OP_ADR: {
          if (!stream_normalize_key_impl (data, size, &offs, bswap, xcdr_version, &desc->member_ids, op, 0, NULL))
            return false;
          break;
        }
        default:
          abort ();
          break;
      }
    }
  }
  *actual_size = offs;
  return true;
}

bool dds_stream_normalize (void *data, uint32_t size, bool bswap, uint32_t xcdr_version, const struct dds_cdrstream_desc *desc, bool just_key, uint32_t *actual_size)
{
  uint32_t off = 0;
  if (size > CDR_SIZE_MAX)
    return normalize_error_bool ();
  else if (just_key)
    return stream_normalize_key (data, size, bswap, xcdr_version, desc, actual_size);
  else if (!stream_normalize_data_impl (data, &off, size, bswap, xcdr_version, &desc->member_ids, desc->ops.ops, false, CDR_KIND_DATA))
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

static const uint32_t *dds_stream_free_sample_seq (char * restrict addr, const struct dds_cdrstream_allocator *allocator, const uint32_t *ops, uint32_t insn)
{
  dds_sequence_t * const seq = (dds_sequence_t *) addr;
  uint32_t num = (seq->_buffer == NULL) ? 0 : (seq->_maximum > seq->_length) ? seq->_maximum : seq->_length;
  const enum dds_stream_typecode subtype = DDS_OP_SUBTYPE (insn);
  uint32_t bound_op = seq_is_bounded (DDS_OP_TYPE (insn)) ? 1 : 0;
  if ((seq->_release && num) || subtype > DDS_OP_VAL_STR)
  {
    switch (subtype)
    {
      case DDS_OP_VAL_BLN: case DDS_OP_VAL_1BY: case DDS_OP_VAL_2BY: case DDS_OP_VAL_4BY: case DDS_OP_VAL_8BY: case DDS_OP_VAL_WCHAR:
        ops += 2 + bound_op;
        break;
      case DDS_OP_VAL_BST: case DDS_OP_VAL_BWSTR: case DDS_OP_VAL_ENU:
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
      case DDS_OP_VAL_WSTR: {
        wchar_t **ptr = (wchar_t **) seq->_buffer;
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

static const uint32_t *dds_stream_free_sample_arr (char * restrict addr, const struct dds_cdrstream_allocator *allocator, const uint32_t *ops, uint32_t insn)
{
  ops += 2;
  const enum dds_stream_typecode subtype = DDS_OP_SUBTYPE (insn);
  uint32_t num = *ops++;
  switch (subtype)
  {
    case DDS_OP_VAL_BLN: case DDS_OP_VAL_1BY: case DDS_OP_VAL_2BY: case DDS_OP_VAL_4BY: case DDS_OP_VAL_8BY: case DDS_OP_VAL_WCHAR: break;
    case DDS_OP_VAL_ENU: ops++; break;
    case DDS_OP_VAL_BMK: case DDS_OP_VAL_BST: case DDS_OP_VAL_BWSTR: ops += 2; break;
    case DDS_OP_VAL_STR: {
      char **ptr = (char **) addr;
      while (num--)
        allocator->free (*ptr++);
      break;
    }
    case DDS_OP_VAL_WSTR: {
      wchar_t **ptr = (wchar_t **) addr;
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

static const uint32_t *dds_stream_free_sample_uni (char * restrict discaddr, char * restrict baseaddr, const struct dds_cdrstream_allocator *allocator, const uint32_t *ops, uint32_t insn)
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
      case DDS_OP_VAL_BLN: case DDS_OP_VAL_1BY: case DDS_OP_VAL_2BY: case DDS_OP_VAL_4BY: case DDS_OP_VAL_8BY: case DDS_OP_VAL_BST: case DDS_OP_VAL_BWSTR: case DDS_OP_VAL_ENU: case DDS_OP_VAL_WCHAR: break;
      case DDS_OP_VAL_STR:
        allocator->free (*((char **) valaddr));
        *((char **) valaddr) = NULL;
        break;
      case DDS_OP_VAL_WSTR:
        allocator->free (*((wchar_t **) valaddr));
        *((wchar_t **) valaddr) = NULL;
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

static const uint32_t *dds_stream_free_sample_xcdr2_pl (char * restrict addr, const struct dds_cdrstream_allocator *allocator, const uint32_t *ops)
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
          (void) dds_stream_free_sample_xcdr2_pl (addr, allocator, plm_ops);
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

static const uint32_t *stream_free_sample_adr_nonexternal (uint32_t insn, void * restrict addr, void * restrict data, const struct dds_cdrstream_allocator *allocator, const uint32_t *ops)
{
  assert (DDS_OP (insn) == DDS_OP_ADR);

  switch (DDS_OP_TYPE (insn))
  {
    case DDS_OP_VAL_BLN: case DDS_OP_VAL_1BY: case DDS_OP_VAL_2BY: case DDS_OP_VAL_4BY: case DDS_OP_VAL_8BY: case DDS_OP_VAL_WCHAR: ops += 2; break;
    case DDS_OP_VAL_STR: {
      allocator->free (*((char **) addr));
      *(char **) addr = NULL;
      ops += 2;
      break;
    }
    case DDS_OP_VAL_WSTR: {
      allocator->free (*((wchar_t **) addr));
      *(wchar_t **) addr = NULL;
      ops += 2;
      break;
    }
    case DDS_OP_VAL_BST: case DDS_OP_VAL_BWSTR: case DDS_OP_VAL_ENU: ops += 3; break;
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

static const uint32_t *stream_free_sample_adr (uint32_t insn, void * restrict data, const struct dds_cdrstream_allocator *allocator, const uint32_t *ops)
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

void dds_stream_free_sample (void *data, const struct dds_cdrstream_allocator *allocator, const uint32_t *ops)
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
      case DDS_OP_RTS: case DDS_OP_JEQ: case DDS_OP_JEQ4: case DDS_OP_KOF: case DDS_OP_PLM: case DDS_OP_MID:
        abort ();
        break;
      case DDS_OP_DLC:
        ops++;
        break;
      case DDS_OP_PLC:
        ops = dds_stream_free_sample_xcdr2_pl (data, allocator, ops);
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

ddsrt_nonnull ((1, 2, 3, 4, 5))
static void dds_stream_extract_key_from_key_prim_op (dds_istream_t *is, restrict_ostream_t *os, const struct dds_cdrstream_allocator *allocator, const struct dds_cdrstream_desc_mid_table *mid_table, const uint32_t *ops, uint16_t key_offset_count, const uint32_t * key_offset_insn)
{
  const uint32_t insn = *ops;
  assert ((insn & DDS_OP_FLAG_KEY) && ((DDS_OP (insn)) == DDS_OP_ADR));
  switch (DDS_OP_TYPE (insn))
  {
    case DDS_OP_VAL_BLN:
    case DDS_OP_VAL_1BY: dds_os_put1 (os, allocator, dds_is_get1 (is)); break;
    case DDS_OP_VAL_WCHAR:
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
    case DDS_OP_VAL_STR: case DDS_OP_VAL_BST:
    case DDS_OP_VAL_WSTR: case DDS_OP_VAL_BWSTR: {
      uint32_t sz = dds_is_get4 (is);
      dds_os_put4 (os, allocator, sz);
      dds_os_put_bytes_base (&os->x, allocator, is->m_buffer + is->m_index, sz);
      is->m_index += sz;
      break;
    }
    case DDS_OP_VAL_ARR: {
      const enum dds_stream_typecode subtype = DDS_OP_SUBTYPE (insn);
      uint32_t elem_size, offs = 0, xcdrv = os->x.m_xcdr_version;
      if (is_dheader_needed (subtype, xcdrv))
      {
        /* In case of non-primitive element type, reserve space for DHEADER in the
           output stream, and skip the DHEADER in the input */
        dds_os_reserve4 (os, allocator);
        offs = os->x.m_index;
        (void) dds_is_get4 (is);
      }
      if (is_primitive_type (subtype))
        elem_size = get_primitive_size (subtype);
      else if (subtype == DDS_OP_VAL_ENU || subtype == DDS_OP_VAL_BMK)
        elem_size = DDS_OP_TYPE_SZ (insn);
      else
        abort ();
      const align_t cdr_align = dds_cdr_get_align (os->x.m_xcdr_version, elem_size);
      const uint32_t num = ops[2];
      dds_cdr_alignto (is, cdr_align);
      dds_cdr_alignto_clear_and_resize_base (&os->x, allocator, cdr_align, num * elem_size);
      void * const dst = os->x.m_buffer + os->x.m_index;
      dds_is_get_bytes (is, dst, num, elem_size);
      os->x.m_index += num * elem_size;
      /* set DHEADER */
      if (is_dheader_needed (subtype, xcdrv))
        *((uint32_t *) (os->x.m_buffer + offs - 4)) = os->x.m_index - offs;
      break;
    }
    case DDS_OP_VAL_EXT: {
      assert (key_offset_count > 0);
      const uint32_t *jsr_ops = ops + DDS_OP_ADR_JSR (ops[2]) + *key_offset_insn;
      dds_stream_extract_key_from_key_prim_op (is, os, allocator, mid_table, jsr_ops, --key_offset_count, ++key_offset_insn);
      break;
    }
    case DDS_OP_VAL_SEQ: case DDS_OP_VAL_BSQ: case DDS_OP_VAL_UNI: case DDS_OP_VAL_STU: {
      abort ();
      break;
    }
  }
}

#if DDSRT_ENDIAN == DDSRT_LITTLE_ENDIAN
static void dds_stream_swap_copy (void * restrict vdst, const void *vsrc, uint32_t size, uint32_t num)
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

ddsrt_nonnull ((1, 2, 3, 4, 5))
static void dds_stream_extract_keyBE_from_key_prim_op (dds_istream_t *is, restrict_ostreamBE_t *os, const struct dds_cdrstream_allocator *allocator, const struct dds_cdrstream_desc_mid_table *mid_table, const uint32_t *ops, uint16_t key_offset_count, const uint32_t * key_offset_insn)
{
  const uint32_t insn = *ops;
  assert ((insn & DDS_OP_FLAG_KEY) && ((DDS_OP (insn)) == DDS_OP_ADR));
  switch (DDS_OP_TYPE (insn))
  {
    case DDS_OP_VAL_BLN:
    case DDS_OP_VAL_1BY: dds_os_put1BE (os, allocator, dds_is_get1 (is)); break;
    case DDS_OP_VAL_WCHAR:
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
    case DDS_OP_VAL_STR: case DDS_OP_VAL_BST:
    case DDS_OP_VAL_WSTR: case DDS_OP_VAL_BWSTR: {
      uint32_t sz = dds_is_get4 (is);
      dds_os_put4BE (os, allocator, sz);
      dds_os_put_bytes_base (&os->x, allocator, is->m_buffer + is->m_index, sz);
      is->m_index += sz;
      break;
    }
    case DDS_OP_VAL_ARR: {
      const enum dds_stream_typecode subtype = DDS_OP_SUBTYPE (insn);
      uint32_t elem_size, offs = 0, xcdrv = os->x.m_xcdr_version;
      if (is_dheader_needed (subtype, xcdrv))
      {
        /* In case of non-primitive element type, reserve space for DHEADER in the
           output stream, and skip the DHEADER in the input */
        dds_os_reserve4BE (os, allocator);
        offs = os->x.m_index;
        (void) dds_is_get4 (is);
      }
      if (is_primitive_type (subtype))
        elem_size = get_primitive_size (subtype);
      else if (subtype == DDS_OP_VAL_ENU || subtype == DDS_OP_VAL_BMK)
        elem_size = DDS_OP_TYPE_SZ (insn);
      else
        abort ();
      const align_t cdr_align = dds_cdr_get_align (os->x.m_xcdr_version, elem_size);
      const uint32_t num = ops[2];
      dds_cdr_alignto (is, cdr_align);
      dds_cdr_alignto_clear_and_resize_base (&os->x, allocator, cdr_align, num * elem_size);
      void const * const src = is->m_buffer + is->m_index;
      void * const dst = os->x.m_buffer + os->x.m_index;
      dds_stream_swap_copy (dst, src, elem_size, num);
      os->x.m_index += num * elem_size;
      is->m_index += num * elem_size;

      /* set DHEADER */
      if (is_dheader_needed (subtype, xcdrv))
        *((uint32_t *) (os->x.m_buffer + offs - 4)) = ddsrt_toBE4u(os->x.m_index - offs);
      break;
    }
    case DDS_OP_VAL_EXT: {
      assert (key_offset_count > 0);
      const uint32_t *jsr_ops = ops + DDS_OP_ADR_JSR (ops[2]) + *key_offset_insn;
      dds_stream_extract_keyBE_from_key_prim_op (is, os, allocator, mid_table, jsr_ops, --key_offset_count, ++key_offset_insn);
      break;
    }
    case DDS_OP_VAL_SEQ: case DDS_OP_VAL_BSQ: case DDS_OP_VAL_UNI: case DDS_OP_VAL_STU: {
      abort ();
      break;
    }
  }
}
#endif

static void dds_stream_extract_key_from_data_skip_subtype (dds_istream_t *is, uint32_t num, uint32_t insn, enum dds_stream_typecode subtype, const uint32_t *subops)
{
  assert (subops != NULL || DDS_OP (insn) == DDS_OP_ADR);
  switch (subtype)
  {
    case DDS_OP_VAL_BLN: case DDS_OP_VAL_1BY: case DDS_OP_VAL_2BY: case DDS_OP_VAL_4BY: case DDS_OP_VAL_8BY: case DDS_OP_VAL_WCHAR: {
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
    case DDS_OP_VAL_STR: case DDS_OP_VAL_BST:
    case DDS_OP_VAL_WSTR: case DDS_OP_VAL_BWSTR: {
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
        dds_stream_extract_key_from_data1 (is, NULL, &dds_cdrstream_default_allocator, &static_empty_mid_table, subops, false, false, remain, &remain);
      break;
    }
    case DDS_OP_VAL_EXT: {
      abort (); /* not supported */
      break;
    }
  }
}

static const uint32_t *dds_stream_extract_key_from_data_skip_array (dds_istream_t *is, const uint32_t *ops)
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

static const uint32_t *dds_stream_extract_key_from_data_skip_sequence (dds_istream_t *is, const uint32_t *ops)
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

static const uint32_t *dds_stream_extract_key_from_data_skip_union (dds_istream_t *is, const uint32_t *ops)
{
  const uint32_t insn = *ops;
  assert (DDS_OP_TYPE (insn) == DDS_OP_VAL_UNI);
  const uint32_t disc = read_union_discriminant (is, insn);
  uint32_t const * const jeq_op = find_union_case (ops, disc);
  if (jeq_op)
    dds_stream_extract_key_from_data_skip_subtype (is, 1, jeq_op[0], DDS_JEQ_TYPE (jeq_op[0]), jeq_op + DDS_OP_ADR_JSR (jeq_op[0]));
  return ops + DDS_OP_ADR_JMP (ops[3]);
}

static const uint32_t *dds_stream_extract_key_from_data_skip_adr (dds_istream_t *is, const uint32_t *ops, enum dds_stream_typecode type)
{
  switch (type)
  {
    case DDS_OP_VAL_BLN: case DDS_OP_VAL_1BY: case DDS_OP_VAL_2BY: case DDS_OP_VAL_4BY: case DDS_OP_VAL_8BY: case DDS_OP_VAL_WCHAR:
    case DDS_OP_VAL_STR: case DDS_OP_VAL_BST:
    case DDS_OP_VAL_WSTR: case DDS_OP_VAL_BWSTR:
    case DDS_OP_VAL_ENU: case DDS_OP_VAL_BMK:
      dds_stream_extract_key_from_data_skip_subtype (is, 1, ops[0], type, NULL);
      if (type == DDS_OP_VAL_BST || type == DDS_OP_VAL_BWSTR || type == DDS_OP_VAL_ARR || type == DDS_OP_VAL_ENU)
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
    case DDS_OP_VAL_EXT:
      abort (); /* handled by caller */
      break;
  }
  return ops;
}

/*******************************************************************************************
 **
 **  Read/write of samples and keys -- i.e., DDSI payloads.
 **
 *******************************************************************************************/

void dds_stream_read_sample (dds_istream_t *is, void *data, const struct dds_cdrstream_allocator *allocator, const struct dds_cdrstream_desc *desc)
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
    (void) dds_stream_read_impl (is, data, allocator, desc->ops.ops, false, CDR_KIND_DATA, SAMPLE_DATA_INITIALIZED);
  }
}

static void dds_stream_read_key_impl (dds_istream_t *is, char *sample, const struct dds_cdrstream_allocator *allocator, const uint32_t *ops, uint16_t key_offset_count, const uint32_t * key_offset_insn, enum sample_data_state sample_state)
{
  void *dst = sample + ops[1];
  uint32_t insn = ops[0];
  assert (key_optimized_allowed (insn));

  if (op_type_external (insn))
    dds_stream_alloc_external (ops, insn, &dst, allocator, &sample_state);

  switch (DDS_OP_TYPE (insn))
  {
    case DDS_OP_VAL_BLN:
    case DDS_OP_VAL_1BY: *((uint8_t *) dst) = dds_is_get1 (is); break;
    case DDS_OP_VAL_2BY: *((uint16_t *) dst) = dds_is_get2 (is); break;
    case DDS_OP_VAL_4BY: *((uint32_t *) dst) = dds_is_get4 (is); break;
    case DDS_OP_VAL_8BY: *((uint64_t *) dst) = dds_is_get8 (is); break;
    case DDS_OP_VAL_WCHAR: *((wchar_t *) dst) = (wchar_t) dds_is_get2 (is); break;
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
    case DDS_OP_VAL_STR: *((char **) dst) = dds_stream_reuse_string (is, *((char **) dst), allocator, sample_state); break;
    case DDS_OP_VAL_WSTR: *((wchar_t **) dst) = dds_stream_reuse_wstring (is, *((wchar_t **) dst), allocator, sample_state); break;
    case DDS_OP_VAL_BST: (void) dds_stream_reuse_string_bound (is, dst, ops[2]); break;
    case DDS_OP_VAL_BWSTR: (void) dds_stream_reuse_wstring_bound (is, dst, ops[2]); break;
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
      dds_stream_read_key_impl (is, dst, allocator, jsr_ops, --key_offset_count, ++key_offset_insn, sample_state);
      break;
    }
  }
}

void dds_stream_read_key (dds_istream_t *is, char *sample, const struct dds_cdrstream_allocator *allocator, const struct dds_cdrstream_desc *desc)
{
  if (desc->flagset & (DDS_TOPIC_KEY_APPENDABLE | DDS_TOPIC_KEY_MUTABLE | DDS_TOPIC_KEY_SEQUENCE | DDS_TOPIC_KEY_ARRAY_NONPRIM))
  {
    /* For types with key fields in aggregated types with appendable or mutable
       extensibility, use the regular read functions to read the key fields */
    (void) dds_stream_read_impl (is, sample, allocator, desc->ops.ops, false, CDR_KIND_KEY, SAMPLE_DATA_INITIALIZED);
  }
  else
  {
    /* For types that only have key fields in aggregated types with final
       extensibility, iterate over the keys (in definition order) and jump
       to key offset in ops */
    for (uint32_t i = 0; i < desc->keys.nkeys; i++)
    {
      const uint32_t *op = desc->ops.ops + desc->keys.keys_definition_order[i].ops_offs;
      switch (DDS_OP (*op))
      {
        case DDS_OP_KOF: {
          uint16_t n_offs = DDS_OP_LENGTH (*op);
          dds_stream_read_key_impl (is, sample, allocator, desc->ops.ops + op[1], --n_offs, op + 2, SAMPLE_DATA_INITIALIZED);
          break;
        }
        case DDS_OP_ADR: {
          dds_stream_read_key_impl (is, sample, allocator, op, 0, NULL, SAMPLE_DATA_INITIALIZED);
          break;
        }
        default:
          abort ();
          break;
      }
    }
  }
}

// Native endianness
#define NAME_BYTE_ORDER_EXT
#include "dds_cdrstream_keys.part.h"
#undef NAME_BYTE_ORDER_EXT

#if DDSRT_ENDIAN == DDSRT_LITTLE_ENDIAN

// Big-endian implementation
#define NAME_BYTE_ORDER_EXT BE
#include "dds_cdrstream_keys.part.h"
#undef NAME_BYTE_ORDER_EXT

#else /* if DDSRT_ENDIAN == DDSRT_LITTLE_ENDIAN */

bool dds_stream_write_keyBE (dds_ostreamBE_t *os, enum dds_cdr_key_serialization_kind ser_kind, const struct dds_cdrstream_allocator *allocator, const char *sample, const struct dds_cdrstream_desc *desc)
{
  return dds_stream_write_key (&os->x, ser_kind, allocator, sample, desc);
}

bool dds_stream_extract_keyBE_from_data (dds_istream_t *is, dds_ostreamBE_t *os, const struct dds_cdrstream_allocator *allocator, const struct dds_cdrstream_desc *desc)
{
  return dds_stream_extract_key_from_data (is, &os->x, allocator, desc);
}

void dds_stream_extract_keyBE_from_key (dds_istream_t *is, dds_ostreamBE_t *os, enum dds_cdr_key_serialization_kind ser_kind, const struct dds_cdrstream_allocator *allocator, const struct dds_cdrstream_desc *desc)
{
  dds_stream_extract_key_from_key (is, &os->x, ser_kind, allocator, desc);
}

#endif /* if DDSRT_ENDIAN == DDSRT_LITTLE_ENDIAN */

/*******************************************************************************************
 **
 **  Pretty-printing
 **
 *******************************************************************************************/

/* Returns true if buffer not yet exhausted, false otherwise */
static bool prtf (char **buf, size_t *bufsize, const char *fmt, ...)
  ddsrt_attribute_format_printf(3, 4);

static bool prtf (char **buf, size_t *bufsize, const char *fmt, ...)
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

static bool prtf_str (char **buf, size_t *bufsize, dds_istream_t *is)
{
  size_t sz = dds_is_get4 (is);
  bool ret = prtf (buf, bufsize, "\"%s\"", is->m_buffer + is->m_index);
  is->m_index += (uint32_t) sz;
  return ret;
}

static bool prtf_utf32 (char **buf, size_t *bufsize, uint32_t utf32)
{
  unsigned char utf8[5], lead;
  int len;
  if (utf32 <= 0x7f) { lead = 0; len = 1; }
  else if (utf32 <= 0x7ff) { lead = 0xc0; len = 2; }
  else if (utf32 <= 0xffff) { lead = 0xe0; len = 3; }
  else { lead = 0xf0; len = 4; }
  utf8[len] = 0;
  for (int j = len - 1; j > 0; j--) {
    utf8[j] = (unsigned char) ((utf32 & 0x3f) | 0x80);
    utf32 >>= 6;
  }
  utf8[0] = (unsigned char) ((utf32 & 0xff) | lead);
  return prtf (buf, bufsize, "%s", utf8);
}

static bool prtf_wstr (char **buf, size_t *bufsize, dds_istream_t *is)
{
  size_t sz = dds_is_get4 (is);
  bool ret = prtf (buf, bufsize, "\"");
  const uint16_t *src = (const uint16_t *) (is->m_buffer + is->m_index);
  uint16_t w1 = 0;
  uint32_t utf32 = 0;
  for (size_t i = 0; ret && i < sz / 2; i++)
  {
    if (src[i] < 0xd800 || src[i] >= 0xe000)
      utf32 = src[i];
    else if (src[i] >= 0xd800 && src[i] < 0xdc00)
      w1 = src[i];
    else
      utf32 = 0x10000 + (((uint32_t) (w1 - 0xd800) << 10) | (uint32_t) (src[i] - 0xdc00));
    if (utf32)
      ret = prtf_utf32 (buf, bufsize, utf32);
  }
  ret = prtf (buf, bufsize, "\"");
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

static bool prtf_enum_bitmask (char **buf, size_t *bufsize, dds_istream_t *is, uint32_t flags)
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

static bool prtf_simple (char **buf, size_t *bufsize, dds_istream_t *is, enum dds_stream_typecode type, uint32_t flags)
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
    case DDS_OP_VAL_WCHAR: {
      const uint16_t x = dds_is_get2 (is);
      return prtf (buf, bufsize, "L'") && prtf_utf32 (buf, bufsize, x) && prtf (buf, bufsize, "'");
    }
    case DDS_OP_VAL_ENU: case DDS_OP_VAL_BMK:
      return prtf_enum_bitmask (buf, bufsize, is, flags);
    case DDS_OP_VAL_STR: case DDS_OP_VAL_BST: return prtf_str (buf, bufsize, is);
    case DDS_OP_VAL_WSTR: case DDS_OP_VAL_BWSTR: return prtf_wstr (buf, bufsize, is);
    case DDS_OP_VAL_ARR: case DDS_OP_VAL_SEQ: case DDS_OP_VAL_BSQ: case DDS_OP_VAL_UNI: case DDS_OP_VAL_STU: case DDS_OP_VAL_EXT:
      abort ();
  }
  return false;
}

static bool prtf_simple_array (char **buf, size_t *bufsize, dds_istream_t *is, uint32_t num, enum dds_stream_typecode type, uint32_t flags)
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
    case DDS_OP_VAL_BLN: case DDS_OP_VAL_2BY: case DDS_OP_VAL_4BY: case DDS_OP_VAL_8BY: case DDS_OP_VAL_WCHAR:
    case DDS_OP_VAL_STR: case DDS_OP_VAL_BST:
    case DDS_OP_VAL_WSTR: case DDS_OP_VAL_BWSTR:
      for (size_t i = 0; cont && i < num; i++)
      {
        if (i != 0)
          (void) prtf (buf, bufsize, ",");
        cont = prtf_simple (buf, bufsize, is, type, flags);
      }
      break;
    case DDS_OP_VAL_SEQ:
    case DDS_OP_VAL_ARR:
    case DDS_OP_VAL_UNI:
    case DDS_OP_VAL_STU:
    case DDS_OP_VAL_BSQ:
    case DDS_OP_VAL_EXT:
      abort ();
      break;
  }
  return prtf (buf, bufsize, "}");
}

static const uint32_t *dds_stream_print_sample1 (char **buf, size_t *bufsize, dds_istream_t *is, const uint32_t *ops, bool add_braces, bool is_mutable_member, enum cdr_data_kind cdr_kind);

static const uint32_t *prtf_seq (char **buf, size_t *bufsize, dds_istream_t *is, const uint32_t *ops, uint32_t insn, enum cdr_data_kind cdr_kind)
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
    case DDS_OP_VAL_BLN: case DDS_OP_VAL_1BY: case DDS_OP_VAL_2BY: case DDS_OP_VAL_4BY: case DDS_OP_VAL_8BY: case DDS_OP_VAL_WCHAR:
      (void) prtf_simple_array (buf, bufsize, is, num, subtype, DDS_OP_FLAGS (insn));
      return ops + 2 + bound_op;
    case DDS_OP_VAL_STR: case DDS_OP_VAL_BST:
    case DDS_OP_VAL_WSTR: case DDS_OP_VAL_BWSTR:
    case DDS_OP_VAL_ENU: case DDS_OP_VAL_BMK: {
      (void) prtf_simple_array (buf, bufsize, is, num, subtype, DDS_OP_FLAGS (insn));
      const uint32_t *ret_ops = ops + 2 + bound_op;
      if (subtype == DDS_OP_VAL_BMK)
        ret_ops += 2;
      else if (subtype == DDS_OP_VAL_BST || subtype == DDS_OP_VAL_BWSTR || subtype == DDS_OP_VAL_ENU)
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
        cont = dds_stream_print_sample1 (buf, bufsize, is, jsr_ops, subtype == DDS_OP_VAL_STU, false, cdr_kind) != NULL;
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

static const uint32_t *prtf_arr (char **buf, size_t *bufsize, dds_istream_t *is, const uint32_t *ops, uint32_t insn, enum cdr_data_kind cdr_kind)
{
  const enum dds_stream_typecode subtype = DDS_OP_SUBTYPE (insn);
  if (is_dheader_needed (subtype, is->m_xcdr_version))
    (void) dds_is_get4 (is);
  const uint32_t num = ops[2];
  switch (subtype)
  {
    case DDS_OP_VAL_BLN: case DDS_OP_VAL_1BY: case DDS_OP_VAL_2BY: case DDS_OP_VAL_4BY: case DDS_OP_VAL_8BY:
    case DDS_OP_VAL_STR: case DDS_OP_VAL_BST: case DDS_OP_VAL_WSTR: case DDS_OP_VAL_BWSTR: case DDS_OP_VAL_WCHAR:
    case DDS_OP_VAL_ENU: case DDS_OP_VAL_BMK: {
      (void) prtf_simple_array (buf, bufsize, is, num, subtype, DDS_OP_FLAGS (insn));
      const uint32_t *ret_ops = ops + 3;
      if (subtype == DDS_OP_VAL_BST || subtype == DDS_OP_VAL_BWSTR || subtype == DDS_OP_VAL_BMK)
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
        cont = dds_stream_print_sample1 (buf, bufsize, is, jsr_ops, subtype == DDS_OP_VAL_STU, false, cdr_kind) != NULL;
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

static const uint32_t *prtf_uni (char **buf, size_t *bufsize, dds_istream_t *is, const uint32_t *ops, uint32_t insn, enum cdr_data_kind cdr_kind)
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
      case DDS_OP_VAL_STR: case DDS_OP_VAL_BST: case DDS_OP_VAL_WSTR: case DDS_OP_VAL_BWSTR: case DDS_OP_VAL_WCHAR:
        (void) prtf_simple (buf, bufsize, is, valtype, DDS_OP_FLAGS (jeq_op[0]));
        break;
      case DDS_OP_VAL_SEQ: case DDS_OP_VAL_BSQ: case DDS_OP_VAL_ARR: case DDS_OP_VAL_UNI: case DDS_OP_VAL_STU: case DDS_OP_VAL_BMK:
        (void) dds_stream_print_sample1 (buf, bufsize, is, jeq_op + DDS_OP_ADR_JSR (jeq_op[0]), valtype == DDS_OP_VAL_STU, false, cdr_kind);
        break;
      case DDS_OP_VAL_EXT: {
        abort (); /* not supported, use UNI instead */
        break;
      }
    }
  }
  return ops;
}

static const uint32_t * dds_stream_print_adr (char **buf, size_t *bufsize, uint32_t insn, dds_istream_t *is, const uint32_t *ops, bool is_mutable_member, enum cdr_data_kind cdr_kind)
{
  const bool is_key = (insn & DDS_OP_FLAG_KEY);
  if (cdr_kind == CDR_KIND_KEY && !is_key)
    return dds_stream_skip_adr (insn, ops);

  dds_istream_t is1 = *is;
  uint32_t param_len = 0;
  if (op_type_optional (insn) && !is_mutable_member)
  {
    if (!stream_is_member_present (&is1, &param_len))
    {
      (void) prtf (buf, bufsize, "NULL");
      is->m_index = is1.m_index + param_len; // param_len is 0 for XCDR2
      return dds_stream_skip_adr (insn, ops);
    }
    if (is->m_xcdr_version == DDSI_RTPS_CDR_ENC_VERSION_1)
    {
      // increase istream index for member header
      is->m_index = is1.m_index;

      // Move buffer in temporary istream `is1` to start of parameter value and
      // set size to param length, so that alignment is reset to 0
      is1.m_buffer += is1.m_index;
      is1.m_index = 0;
      is1.m_size = param_len;
    }
  }

  switch (DDS_OP_TYPE (insn))
  {
    case DDS_OP_VAL_BLN: case DDS_OP_VAL_1BY: case DDS_OP_VAL_2BY: case DDS_OP_VAL_4BY: case DDS_OP_VAL_8BY:
    case DDS_OP_VAL_STR: case DDS_OP_VAL_WSTR: case DDS_OP_VAL_WCHAR:
      if (!prtf_simple (buf, bufsize, &is1, DDS_OP_TYPE (insn), DDS_OP_FLAGS (insn)))
        return NULL;
      ops += 2;
      break;
    case DDS_OP_VAL_BST: case DDS_OP_VAL_BWSTR: case DDS_OP_VAL_ENU: case DDS_OP_VAL_BMK:
      if (!prtf_simple (buf, bufsize, &is1, DDS_OP_TYPE (insn), DDS_OP_FLAGS (insn)))
        return NULL;
      ops += 3 + (DDS_OP_TYPE (insn) == DDS_OP_VAL_BMK ? 1 : 0);
      break;
    case DDS_OP_VAL_SEQ: case DDS_OP_VAL_BSQ:
      ops = prtf_seq (buf, bufsize, &is1, ops, insn, cdr_kind);
      break;
    case DDS_OP_VAL_ARR:
      ops = prtf_arr (buf, bufsize, &is1, ops, insn, cdr_kind);
      break;
    case DDS_OP_VAL_UNI:
      ops = prtf_uni (buf, bufsize, &is1, ops, insn, cdr_kind);
      break;
    case DDS_OP_VAL_EXT: {
      const uint32_t *jsr_ops = ops + DDS_OP_ADR_JSR (ops[2]);
      const uint32_t jmp = DDS_OP_ADR_JMP (ops[2]);
      /* skip DLC instruction for base type, DHEADER is not in the data for base types */
      if (op_type_base (insn) && jsr_ops[0] == DDS_OP_DLC)
        jsr_ops++;
      if (dds_stream_print_sample1 (buf, bufsize, &is1, jsr_ops, true, false, cdr_kind) == NULL)
        return NULL;
      ops += jmp ? jmp : 3;
      break;
    }
    case DDS_OP_VAL_STU:
      abort (); /* op type STU only supported as subtype */
      break;
  }

  if (is->m_xcdr_version == DDSI_RTPS_CDR_ENC_VERSION_1 && op_type_optional (insn) && !is_mutable_member)
    is->m_index += param_len;
  else
    is->m_index = is1.m_index;

  return ops;
}

static const uint32_t *prtf_delimited (char **buf, size_t *bufsize, dds_istream_t *is, const uint32_t *ops, enum cdr_data_kind cdr_kind)
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
        if ((ops = (is->m_index - delimited_offs < delimited_sz) ? dds_stream_print_adr (buf, bufsize, insn, is, ops, false, cdr_kind) : dds_stream_skip_adr (insn, ops)) == NULL)
          return NULL;
        break;
      case DDS_OP_JSR:
        if (dds_stream_print_sample1 (buf, bufsize, is, ops + DDS_OP_JUMP (insn), false, false, cdr_kind) == NULL)
          return NULL;
        ops++;
        break;
      case DDS_OP_RTS: case DDS_OP_JEQ: case DDS_OP_JEQ4: case DDS_OP_KOF: case DDS_OP_DLC: case DDS_OP_PLC: case DDS_OP_PLM: case DDS_OP_MID: {
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

static bool prtf_xcdr2_plm (char **buf, size_t *bufsize, dds_istream_t *is, uint32_t m_id, const uint32_t *ops, enum cdr_data_kind cdr_kind)
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
      found = prtf_xcdr2_plm (buf, bufsize, is, m_id, plm_ops, cdr_kind);
    }
    else if (ops[ops_csr + 1] == m_id)
    {
      (void) dds_stream_print_sample1 (buf, bufsize, is, plm_ops, true, true, cdr_kind);
      found = true;
      break;
    }
    ops_csr += 2;
  }
  return found;
}

static const uint32_t *prtf_xcdr2_pl (char **buf, size_t *bufsize, dds_istream_t *is, const uint32_t *ops, enum cdr_data_kind cdr_kind)
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
    if (!prtf_xcdr2_plm (buf, bufsize, is, m_id, ops, cdr_kind))
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

static const uint32_t * dds_stream_print_sample1 (char **buf, size_t *bufsize, dds_istream_t *is, const uint32_t *ops, bool add_braces, bool is_mutable_member, enum cdr_data_kind cdr_kind)
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
        ops = dds_stream_print_adr (buf, bufsize, insn, is, ops, is_mutable_member, cdr_kind);
        break;
      case DDS_OP_JSR:
        cont = dds_stream_print_sample1 (buf, bufsize, is, ops + DDS_OP_JUMP (insn), true, is_mutable_member, cdr_kind) != NULL;
        ops++;
        break;
      case DDS_OP_RTS: case DDS_OP_JEQ: case DDS_OP_JEQ4: case DDS_OP_KOF: case DDS_OP_PLM: case DDS_OP_MID:
        abort ();
        break;
      case DDS_OP_DLC:
        assert (is->m_xcdr_version == DDSI_RTPS_CDR_ENC_VERSION_2);
        ops = prtf_delimited (buf, bufsize, is, ops, cdr_kind);
        break;
      case DDS_OP_PLC:
        assert (is->m_xcdr_version == DDSI_RTPS_CDR_ENC_VERSION_2);
        ops = prtf_xcdr2_pl (buf, bufsize, is, ops, cdr_kind);
        break;
    }
  }
  if (add_braces)
    (void) prtf (buf, bufsize, "}");
  return ops;
}

size_t dds_stream_print_sample (dds_istream_t *is, const struct dds_cdrstream_desc *desc, char *buf, size_t size)
{
  (void) dds_stream_print_sample1 (&buf, &size, is, desc->ops.ops, true, false, CDR_KIND_DATA);
  return size;
}

size_t dds_stream_print_key (dds_istream_t *is, const struct dds_cdrstream_desc *desc, char *buf, size_t size)
{
  (void) prtf (&buf, &size, ":k:{");
  (void) dds_stream_print_sample1 (&buf, &size, is, desc->ops.ops, true, false, CDR_KIND_KEY);
  (void) prtf (&buf, &size, "}");
  return size;
}

uint32_t dds_stream_countops (const uint32_t *ops, uint32_t nkeys, const dds_key_descriptor_t *keys)
{
  struct dds_cdrstream_ops_info info;
  dds_stream_get_ops_info (ops, &info);
  for (uint32_t n = 0; n < nkeys; n++)
    dds_stream_countops_keyoffset (ops, &keys[n], &info.ops_end);
  return (uint32_t) (info.ops_end - ops);
}

/* Gets the (minimum) extensibility of the types used for this topic, and returns the XCDR
   version that is required for (de)serializing the type for this topic descriptor */
uint16_t dds_stream_minimum_xcdr_version (const uint32_t *ops)
{
  struct dds_cdrstream_ops_info info;
  dds_stream_get_ops_info (ops, &info);
  return info.min_xcdrv;
}

/* Gets the extensibility of the top-level type for a topic, by inspecting the serializer ops */
bool dds_stream_extensibility (const uint32_t *ops, enum dds_cdr_type_extensibility *ext)
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
      case DDS_OP_RTS: case DDS_OP_JEQ: case DDS_OP_JEQ4: case DDS_OP_KOF: case DDS_OP_PLM: case DDS_OP_MID:
        abort ();
        break;
    }
  }
  return false;
}

uint32_t dds_stream_type_nesting_depth (const uint32_t *ops)
{
  struct dds_cdrstream_ops_info info;
  dds_stream_get_ops_info (ops, &info);
  return info.nesting_max;
}

static bool data_type_contains_indirections (dds_data_type_properties_t props)
{
  return props & (DDS_DATA_TYPE_CONTAINS_OPTIONAL
                  | DDS_DATA_TYPE_CONTAINS_STRING
                  | DDS_DATA_TYPE_CONTAINS_WSTRING
                  | DDS_DATA_TYPE_CONTAINS_SEQUENCE
                  | DDS_DATA_TYPE_CONTAINS_BSEQUENCE
                  | DDS_DATA_TYPE_CONTAINS_EXTERNAL);
}

dds_data_type_properties_t dds_stream_data_types (const uint32_t *ops)
{
  struct dds_cdrstream_ops_info info;
  dds_stream_get_ops_info (ops, &info);
  if (!data_type_contains_indirections (info.data_types))
    info.data_types |= DDS_DATA_TYPE_IS_MEMCPY_SAFE;
  return info.data_types;
}




// Calculate key size

static uint32_t add_to_key_size_impl (uint32_t keysize, uint32_t field_size, uint32_t field_dims, uint32_t field_align, uint32_t max_align)
{
  uint32_t sz = keysize;
  if (field_align > max_align)
    field_align = max_align;
  if (sz % field_align)
    sz += field_align - (sz % field_align);
  sz += field_size * field_dims;
  if (sz > DDS_FIXED_KEY_MAX_SIZE)
    sz = DDS_FIXED_KEY_MAX_SIZE + 1;
  return sz;
}

static void add_to_key_size_xcdrv1 (struct key_props *k, uint32_t field_size, uint32_t field_dims, uint32_t field_align)
{
  k->sz_xcdrv1 = add_to_key_size_impl (k->sz_xcdrv1, field_size, field_dims, field_align, XCDR1_MAX_ALIGN);
}

static void add_to_key_size_xcdrv2 (struct key_props *k, uint32_t field_size, uint32_t field_dims, uint32_t field_align)
{
  k->sz_xcdrv2 = add_to_key_size_impl (k->sz_xcdrv2, field_size, field_dims, field_align, XCDR2_MAX_ALIGN);
}

static void add_to_key_size (struct key_props *k, uint32_t field_size, uint32_t field_dims, uint32_t field_align)
{
  if (k->min_xcdrv == DDSI_RTPS_CDR_ENC_VERSION_1)
    add_to_key_size_xcdrv1 (k, field_size, field_dims, field_align);
  add_to_key_size_xcdrv2 (k, field_size, field_dims, field_align);
}

static void set_key_size_unbounded (struct key_props *k)
{
  if (k->min_xcdrv == DDSI_RTPS_CDR_ENC_VERSION_1)
    k->sz_xcdrv1 = DDS_FIXED_KEY_MAX_SIZE + 1;
  k->sz_xcdrv2 = DDS_FIXED_KEY_MAX_SIZE + 1;
}

#ifndef NDEBUG
static bool key_size_is_unbounded (const struct key_props *k)
{
  return (k->min_xcdrv == DDSI_RTPS_CDR_ENC_VERSION_1) ?
      (k->sz_xcdrv1 == DDS_FIXED_KEY_MAX_SIZE + 1) : (k->sz_xcdrv2 == DDS_FIXED_KEY_MAX_SIZE + 1);
}
#endif

static const uint32_t *dds_stream_key_size_arr_bseq (const uint32_t *ops, uint32_t insn, struct key_props *k)
{
  const enum dds_stream_typecode type = DDS_OP_TYPE (insn);
  const enum dds_stream_typecode subtype = DDS_OP_SUBTYPE (insn);
  assert (type == DDS_OP_VAL_ARR || type == DDS_OP_VAL_BSQ);
  (void) type;
  uint32_t bound = ops[2];
  bool is_bseq = DDS_OP_TYPE (insn) == DDS_OP_VAL_BSQ;

  // both array and bseq get a dheader in case of non-primitive element type
  if (is_dheader_needed (subtype, DDSI_RTPS_CDR_ENC_VERSION_2))
    add_to_key_size_xcdrv2 (k, 4, 1, 4);

  // seq length for bseq
  if (is_bseq)
    add_to_key_size (k, 4, 1, 4);

  switch (subtype)
  {
    case DDS_OP_VAL_BLN: case DDS_OP_VAL_1BY: case DDS_OP_VAL_2BY: case DDS_OP_VAL_4BY: case DDS_OP_VAL_8BY: case DDS_OP_VAL_WCHAR: {
      const uint32_t elem_size = get_primitive_size (subtype);
      add_to_key_size (k, elem_size, bound, elem_size);
      return ops + 3;
    }
    case DDS_OP_VAL_ENU: {
      const uint32_t elem_size = DDS_OP_TYPE_SZ (insn);
      add_to_key_size (k, elem_size, bound, elem_size);
      return ops + 4;
    }
    case DDS_OP_VAL_BMK: {
      const uint32_t elem_size = DDS_OP_TYPE_SZ (insn);
      add_to_key_size (k, elem_size, bound, elem_size);
      return ops + 5;
    }
    case DDS_OP_VAL_STR: case DDS_OP_VAL_WSTR: {
      set_key_size_unbounded (k);
      return ops + 3;
    }
    case DDS_OP_VAL_BST: {
      const uint32_t sz = ops[3 + (is_bseq ? 0 : 1)];
      if (bound > DDS_FIXED_KEY_MAX_SIZE)
        set_key_size_unbounded (k);
      else
      {
        for (uint32_t i = 0; i < bound; i++)
        {
          add_to_key_size (k, 4, 1, 4);
          add_to_key_size (k, 1, sz, 1);
        }
      }
      return ops + 4 + (is_bseq ? 0 : 1);
    }
    case DDS_OP_VAL_BWSTR: {
      const uint32_t sz = ops[3 + (is_bseq ? 0 : 1)];
      assert (sz > 0); // terminating L'\0' not on wire
      if (bound > DDS_FIXED_KEY_MAX_SIZE)
        set_key_size_unbounded (k);
      else
      {
        for (uint32_t i = 0; i < bound; i++)
        {
          add_to_key_size (k, 4, 1, 4);
          add_to_key_size (k, 2, sz - 1, 2);
        }
      }
      return ops + 4 + (is_bseq ? 0 : 1);
    }
    case DDS_OP_VAL_SEQ: case DDS_OP_VAL_BSQ: case DDS_OP_VAL_ARR: case DDS_OP_VAL_UNI: case DDS_OP_VAL_STU: {
      const uint32_t insn_op = (is_bseq ? 4 : 3);
      const uint32_t *jsr_ops = ops + DDS_OP_ADR_JSR (ops[insn_op]);
      const uint32_t jmp = DDS_OP_ADR_JMP (ops[insn_op]);
      if (bound > DDS_FIXED_KEY_MAX_SIZE)
        set_key_size_unbounded (k);
      else
      {
        for (uint32_t i = 0; i < bound; i++)
          (void) dds_stream_key_size (jsr_ops, k);
      }
      return ops + (jmp ? jmp : 5);
    }
    case DDS_OP_VAL_EXT: {
      abort (); /* not supported */
      break;
    }
  }
  return NULL;
}

static const uint32_t *dds_stream_key_size_seq (const uint32_t *ops, uint32_t insn, struct key_props *k)
{
  assert (key_size_is_unbounded (k));
  const enum dds_stream_typecode subtype = DDS_OP_SUBTYPE (insn);
  switch (subtype)
  {
    case DDS_OP_VAL_BLN: case DDS_OP_VAL_1BY: case DDS_OP_VAL_2BY: case DDS_OP_VAL_4BY: case DDS_OP_VAL_8BY: case DDS_OP_VAL_WCHAR:
    case DDS_OP_VAL_STR: case DDS_OP_VAL_WSTR:
      ops += 2;
      break;
    case DDS_OP_VAL_BST: case DDS_OP_VAL_BWSTR:
    case DDS_OP_VAL_ENU:
      ops += 3;
      break;
    case DDS_OP_VAL_BMK:
      ops += 4;
      break;
    case DDS_OP_VAL_SEQ: case DDS_OP_VAL_BSQ: case DDS_OP_VAL_ARR: case DDS_OP_VAL_UNI: case DDS_OP_VAL_STU: {
      const uint32_t *jsr_ops = ops + DDS_OP_ADR_JSR (ops[3]);
      const uint32_t jmp = DDS_OP_ADR_JMP (ops[3]);
      (void) dds_stream_key_size (jsr_ops, k); // only for settings flags
      ops += (jmp ? jmp : 5);
      break;
    }
    case DDS_OP_VAL_EXT: {
      abort (); /* not supported */
      break;
    }
  }
  return ops;
}

static const uint32_t *dds_stream_key_size_adr (const uint32_t *ops, uint32_t insn, struct key_props *k)
{
  if (!(insn & DDS_OP_FLAG_KEY))
    return dds_stream_skip_adr (insn, ops);

  switch (DDS_OP_TYPE (insn))
  {
    case DDS_OP_VAL_BLN: case DDS_OP_VAL_1BY: case DDS_OP_VAL_2BY: case DDS_OP_VAL_4BY: case DDS_OP_VAL_8BY: case DDS_OP_VAL_WCHAR: {
      const uint32_t sz = get_primitive_size (DDS_OP_TYPE (insn));
      add_to_key_size (k, sz, 1, sz);
      ops += 2;
      break;
    }
    case DDS_OP_VAL_STR: case DDS_OP_VAL_WSTR:
      set_key_size_unbounded (k);
      ops += 2;
      break;
    case DDS_OP_VAL_BST: {
      const uint32_t sz = ops[2];
      add_to_key_size (k, 4, 1, 4);
      add_to_key_size (k, 1, sz, 1);
      ops += 3;
      break;
    }
    case DDS_OP_VAL_BWSTR: {
      const uint32_t sz = ops[2];
      assert (sz > 0); // terminating L'\0' not on wire
      add_to_key_size (k, 4, 1, 4);
      add_to_key_size (k, 2, sz - 1, 2);
      ops += 3;
      break;
    }
    case DDS_OP_VAL_SEQ:
      k->is_sequence = true;
      set_key_size_unbounded (k);
      ops = dds_stream_key_size_seq (ops, insn, k); // key size is unbounded, but look into element type to set flags
      break;
    case DDS_OP_VAL_BSQ:
      k->is_sequence = true;
      ops = dds_stream_key_size_arr_bseq (ops, insn, k);
      break;
    case DDS_OP_VAL_ARR: {
      const enum dds_stream_typecode subtype = DDS_OP_SUBTYPE (insn);
      k->is_array_nonprim = !(is_primitive_or_enum_type (subtype) || subtype == DDS_OP_VAL_BMK);
      ops = dds_stream_key_size_arr_bseq (ops, insn, k);
      break;
    }
    case DDS_OP_VAL_UNI:
      // TODO: support union as part of key
      set_key_size_unbounded (k);
      ops = dds_stream_skip_adr (insn, ops);
      break;
    case DDS_OP_VAL_ENU: {
      const uint32_t sz = DDS_OP_TYPE_SZ (insn);
      add_to_key_size (k, sz, 1, sz);
      ops += 3;
      break;
    }
    case DDS_OP_VAL_BMK: {
      const uint32_t sz = DDS_OP_TYPE_SZ (insn);
      add_to_key_size (k, sz, 1, sz);
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

      (void) dds_stream_key_size (jsr_ops, k);
      ops += jmp ? jmp : 3;
      break;
    }
    case DDS_OP_VAL_STU: abort(); break; /* op type STU only supported as subtype */
  }
  return ops;
}

static const uint32_t *dds_stream_key_size_delimited (const uint32_t *ops, struct key_props *k)
{
  // DLC op
  ops++;

  // dheader
  add_to_key_size (k, 4, 1, 4);

  uint32_t insn;
  while ((insn = *ops) != DDS_OP_RTS)
  {
    switch (DDS_OP (insn))
    {
      case DDS_OP_ADR: {
        ops = dds_stream_key_size_adr (ops, insn, k);
        break;
      }
      case DDS_OP_JSR: {
        (void) dds_stream_key_size (ops + DDS_OP_JUMP (insn), k);
        ops++;
        break;
      }
      case DDS_OP_RTS: case DDS_OP_JEQ: case DDS_OP_JEQ4: case DDS_OP_KOF: case DDS_OP_DLC: case DDS_OP_PLC: case DDS_OP_PLM: case DDS_OP_MID: {
        abort ();
        break;
      }
    }
  }
  return ops;
}

static bool dds_stream_key_size_xcdr2_pl_member (const uint32_t *ops, struct key_props *k)
{
  uint32_t flags = DDS_OP_FLAGS (ops[0]);
  bool is_key = flags & (DDS_OP_FLAG_MU | DDS_OP_FLAG_KEY);
  if (!is_key)
    return true;

  uint32_t lc = get_length_code (ops);
  assert (lc <= LENGTH_CODE_ALSO_NEXTINT8);
  add_to_key_size (k, 4, 1, 4);
  if (lc == LENGTH_CODE_NEXTINT)
    add_to_key_size (k, 4, 1, 4);
  return dds_stream_key_size (ops, k);
}

static const uint32_t *dds_stream_key_size_xcdr2_pl_memberlist (const uint32_t *ops, struct key_props *k)
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
          if (!dds_stream_key_size_xcdr2_pl_memberlist (plm_ops, k))
            return NULL;
        }
        else
        {
          if (!dds_stream_key_size_xcdr2_pl_member (plm_ops, k))
            return NULL;
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

static const uint32_t *dds_stream_key_size_xcdr2_pl (const uint32_t *ops, struct key_props *k)
{
  // skip PLC op
  ops++;

  // add dheader size
  add_to_key_size (k, 4, 1, 4);

  // add members
  return dds_stream_key_size_xcdr2_pl_memberlist (ops, k);
}

static void dds_stream_key_size_prim_op (const uint32_t *ops, uint16_t key_offset_count, const uint32_t * key_offset_insn, struct key_props *k)
{
  const uint32_t insn = *ops;
  assert ((insn & DDS_OP_FLAG_KEY) && ((DDS_OP (insn)) == DDS_OP_ADR));
  switch (DDS_OP_TYPE (insn))
  {
    case DDS_OP_VAL_BLN: case DDS_OP_VAL_1BY: case DDS_OP_VAL_2BY: case DDS_OP_VAL_4BY: case DDS_OP_VAL_8BY: case DDS_OP_VAL_WCHAR:
    case DDS_OP_VAL_STR: case DDS_OP_VAL_BST:
    case DDS_OP_VAL_WSTR: case DDS_OP_VAL_BWSTR:
    case DDS_OP_VAL_SEQ: case DDS_OP_VAL_BSQ:
    case DDS_OP_VAL_ARR: case DDS_OP_VAL_UNI:
    case DDS_OP_VAL_ENU: case DDS_OP_VAL_BMK:
      (void) dds_stream_key_size_adr (ops, insn, k);
      break;
    case DDS_OP_VAL_EXT: {
      assert (key_offset_count > 0);
      const uint32_t *jsr_ops = ops + DDS_OP_ADR_JSR (ops[2]) + *key_offset_insn;
      dds_stream_key_size_prim_op (jsr_ops, --key_offset_count, ++key_offset_insn, k);
      break;
    }
    case DDS_OP_VAL_STU: abort(); break; /* op type STU only supported as subtype */
  }
}

static void dds_stream_key_size_keyhash (struct dds_cdrstream_desc *desc, struct key_props *k)
{
  /* Serialized key for getting a key-hash has fields in member-id order and forces all
     aggregated types to final extensibility, so we can skip to the dheaders and member
     headers */
  for (uint32_t i = 0; i < desc->keys.nkeys; i++)
  {
    uint32_t const * const op = desc->ops.ops + desc->keys.keys[i].ops_offs; // use keys in member-id order
    switch (DDS_OP (*op))
    {
      case DDS_OP_KOF: {
        uint16_t n_offs = DDS_OP_LENGTH (*op);
        assert (n_offs > 0);
        dds_stream_key_size_prim_op (desc->ops.ops + op[1], --n_offs, op + 2, k);
        break;
      }
      case DDS_OP_ADR: {
        dds_stream_key_size_prim_op (op, 0, NULL, k);
        break;
      }
      default:
        abort ();
        break;
    }
  }
}

static const uint32_t *dds_stream_key_size (const uint32_t *ops, struct key_props *k)
{
  uint32_t insn;
  while ((insn = *ops) != DDS_OP_RTS)
  {
    switch (DDS_OP (insn))
    {
      case DDS_OP_ADR:
        ops = dds_stream_key_size_adr (ops, insn, k);
        break;
      case DDS_OP_JSR:
        (void) dds_stream_key_size (ops + DDS_OP_JUMP (insn), k);
        ops++;
        break;
      case DDS_OP_RTS: case DDS_OP_JEQ: case DDS_OP_JEQ4: case DDS_OP_KOF: case DDS_OP_PLM: case DDS_OP_MID:
        abort ();
        break;
      case DDS_OP_DLC:
        k->is_appendable = true;
        ops = dds_stream_key_size_delimited (ops, k);
        break;
      case DDS_OP_PLC:
        k->is_mutable = true;
        ops = dds_stream_key_size_xcdr2_pl (ops, k);
        break;
    }
  }
  return ops;
}

uint32_t dds_stream_key_flags (struct dds_cdrstream_desc *desc, uint32_t *keysz_xcdrv1, uint32_t *keysz_xcdrv2)
{
  uint32_t key_flags = 0u;
  if (desc->keys.nkeys > 0)
  {
    {
      struct key_props key_properties = { 0 };
      key_properties.min_xcdrv = dds_stream_minimum_xcdr_version (desc->ops.ops);
      (void) dds_stream_key_size (desc->ops.ops, &key_properties);

      if (key_properties.min_xcdrv == DDSI_RTPS_CDR_ENC_VERSION_1 && key_properties.sz_xcdrv1 <= DDS_FIXED_KEY_MAX_SIZE)
        key_flags |= DDS_TOPIC_FIXED_KEY;
      if (key_properties.sz_xcdrv2 <= DDS_FIXED_KEY_MAX_SIZE)
        key_flags |= DDS_TOPIC_FIXED_KEY_XCDR2;

      if (key_properties.is_mutable)
        key_flags |= DDS_TOPIC_KEY_MUTABLE;
      if (key_properties.is_appendable)
        key_flags |= DDS_TOPIC_KEY_APPENDABLE;
      if (key_properties.is_sequence)
        key_flags |= DDS_TOPIC_KEY_SEQUENCE;
      if (key_properties.is_array_nonprim)
        key_flags |= DDS_TOPIC_KEY_ARRAY_NONPRIM;

      if (keysz_xcdrv1 != NULL)
        *keysz_xcdrv1 = key_properties.min_xcdrv == DDSI_RTPS_CDR_ENC_VERSION_1 ? key_properties.sz_xcdrv1 : 0;
      if (keysz_xcdrv2 != NULL)
        *keysz_xcdrv2 = key_properties.sz_xcdrv2;
    }

    {
      struct key_props hash_key_properties = { 0 };
      dds_stream_key_size_keyhash (desc, &hash_key_properties);

      if (hash_key_properties.sz_xcdrv2 <= DDS_FIXED_KEY_MAX_SIZE)
        key_flags |= DDS_TOPIC_FIXED_KEY_XCDR2_KEYHASH;
    }
  }

  assert (!(key_flags & ~DDS_CDR_CALCULATED_FLAGS));
  return key_flags;
}

static int key_cmp_idx (const void *va, const void *vb)
{
  const struct dds_cdrstream_desc_key *a = va;
  const struct dds_cdrstream_desc_key *b = vb;
  if (a->idx != b->idx)
    return a->idx < b->idx ? -1 : 1;
  return 0;
}

static void copy_desc_keys (dds_cdrstream_desc_key_t **dst, const struct dds_cdrstream_allocator *allocator, const dds_key_descriptor_t *keys, uint32_t nkeys)
{
  if (nkeys > 0)
  {
    *dst = allocator->malloc (nkeys  * sizeof (**dst));
    for (uint32_t i = 0; i < nkeys; i++)
    {
      (*dst)[i].ops_offs = keys[i].m_offset;
      (*dst)[i].idx = keys[i].m_idx;
    }
  }
  else
  {
    *dst = NULL;
  }
}

static uint32_t mid_hash (const void *va)
{
  const struct dds_cdrstream_desc_mid *m = va;
  return ddsrt_mh3 (&m->adr_offs, sizeof (m->adr_offs), 0);
}

static bool mid_equal (const void *va, const void *vb)
{
  const struct dds_cdrstream_desc_mid *a = va;
  const struct dds_cdrstream_desc_mid *b = vb;
  return a->adr_offs == b->adr_offs;
}

static struct ddsrt_hh *dds_stream_get_memberid_table (const struct dds_cdrstream_allocator *allocator, const uint32_t *ops, uint32_t mid_table_offs)
{
  if (mid_table_offs == 0)
    return NULL;

  struct ddsrt_hh *table = ddsrt_hh_new (1, mid_hash, mid_equal);
  uint32_t insn;
  ops += mid_table_offs;
  while ((insn = *ops) != DDS_OP_RTS)
  {
    switch (DDS_OP (insn))
    {
      case DDS_OP_MID: {
        struct dds_cdrstream_desc_mid *m = allocator->malloc (sizeof (*m));
        m->adr_offs = (uint32_t) (insn & DDS_MID_OFFSET_MASK);
        m->mid = ops[1];
        ddsrt_hh_add (table, m);
        ops += 2;
        break;
      }
      default:
        abort ();
    }
  }
  return table;
}

void dds_cdrstream_desc_init (struct dds_cdrstream_desc *desc, const struct dds_cdrstream_allocator *allocator,
    uint32_t size, uint32_t align, uint32_t flagset, const uint32_t *ops, const dds_key_descriptor_t *keys, uint32_t nkeys, uint32_t mid_table_offs)
{
  desc->size = size;
  desc->align = align;

  /* Copy keys from topic descriptor, which are ordered by member-id (scoped to their containing
     type. Additionally a copy of the key list in definition order is stored. */
  desc->keys.nkeys = nkeys;
  copy_desc_keys (&desc->keys.keys, allocator, keys, nkeys);
  copy_desc_keys (&desc->keys.keys_definition_order, allocator, keys, nkeys);
  if (desc->keys.nkeys > 0)
    qsort (desc->keys.keys_definition_order, nkeys, sizeof (*desc->keys.keys_definition_order), key_cmp_idx);

  /* Get the actual number of ops, excluding the member ID table ops */
  desc->ops.nops = dds_stream_countops (ops, nkeys, keys);
  desc->ops.ops = allocator->malloc (desc->ops.nops * sizeof (*desc->ops.ops));
  memcpy (desc->ops.ops, ops, desc->ops.nops * sizeof (*desc->ops.ops));

  /* Get the flagset from the descriptor, except for the key related flags that are calculated
     using the CDR stream serializer */
  desc->flagset = flagset & ~DDS_CDR_CALCULATED_FLAGS;
  desc->flagset |= dds_stream_key_flags (desc, NULL, NULL);

  /* Read the member ID table from the ops and store it in a hash table
     (member IDs for non-mutable types, used in XCDR1 member header) */
  if (mid_table_offs > 0)
  {
    desc->member_ids.op0 = desc->ops.ops;
    desc->member_ids.table = dds_stream_get_memberid_table (allocator, ops, mid_table_offs);
  }
  else
  {
    desc->member_ids.table = NULL;
  }
}

static void free_member_id (void *vinfo, void *varg)
{
  const struct dds_cdrstream_allocator *allocator = (const struct dds_cdrstream_allocator *) varg;
  allocator->free (vinfo);
}

void dds_cdrstream_desc_fini (struct dds_cdrstream_desc *desc, const struct dds_cdrstream_allocator *allocator)
{
  if (desc->keys.nkeys > 0)
  {
    if (desc->keys.keys != NULL)
      allocator->free (desc->keys.keys);
    if (desc->keys.keys_definition_order != NULL)
      allocator->free (desc->keys.keys_definition_order);
  }
  if (desc->member_ids.table != NULL)
  {
    ddsrt_hh_enum (desc->member_ids.table, free_member_id, (void *) allocator);
    ddsrt_hh_free (desc->member_ids.table);
  }
  allocator->free (desc->ops.ops);
}

