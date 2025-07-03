// Copyright(c) 2021 to 2022 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "dds/ddsi/ddsi_protocol.h"
#include "dds/ddsi/ddsi_xqos.h"
#include "dds/ddsi/ddsi_typelib.h"
#include "dds/ddsi/ddsi_typewrap.h"
#include "dds/ddsc/dds_opcodes.h"
#include "dds/cdr/dds_cdrstream.h"
#include "idl/string.h"
#include "libidlc/libidlc__descriptor.h"
#include "idlc/generator.h"
#include "test_common.h"

#include "CUnit/Theory.h"

#ifdef DDS_HAS_TYPELIB
#include "idl/descriptor_type_meta.h"

static void *calloc_no_fail (size_t count, size_t size)
{
  assert (count > 0 && size > 0);
  void *p = calloc (count, size);
  if (p == NULL)
    abort ();
  return p;
}

/* In a type object, case label is stored as 32 bits signed integer,
   so this test should be enabled when type object generation is enabled */
CU_Test(idlc_type_meta, union_max_label_value)
{
  idl_retcode_t ret;
  char idl[256];
  const char *fmt = "union u switch(%s) { case %lld: long l; };";

  static const struct {
    bool type_info;
    const char *switch_type;
    int64_t label_value;
    idl_retcode_t result_parse;
    idl_retcode_t result_meta;
  } tests[] = {
    { true, "int32",  INT32_MAX,               IDL_RETCODE_OK,            IDL_RETCODE_OK },
    { true, "int32",  INT32_MIN,               IDL_RETCODE_OK,            IDL_RETCODE_OK },
    { true, "int32",  (int64_t) INT32_MAX + 1, IDL_RETCODE_OUT_OF_RANGE,  0 },
    { true, "uint32", INT32_MAX,               IDL_RETCODE_OK,            IDL_RETCODE_OK },
    { true, "uint32", (int64_t) INT32_MAX + 1, IDL_RETCODE_OK,            IDL_RETCODE_OUT_OF_RANGE },
    { true, "int64",  (int64_t) INT64_MAX,     IDL_RETCODE_OK,            IDL_RETCODE_OUT_OF_RANGE },
    { true, "int64",  (int64_t) INT64_MIN,     IDL_RETCODE_OK,            IDL_RETCODE_OUT_OF_RANGE },

    { false, "uint32", (int64_t) INT32_MAX + 1, IDL_RETCODE_OK,           IDL_RETCODE_OK },
    { false, "int64",  (int64_t) INT64_MAX,     IDL_RETCODE_OK,           IDL_RETCODE_OK },
    { false, "int64",  (int64_t) INT64_MIN,     IDL_RETCODE_OK,           IDL_RETCODE_OK }
  };

  uint32_t flags = IDL_FLAG_EXTENDED_DATA_TYPES |
                   IDL_FLAG_ANONYMOUS_TYPES |
                   IDL_FLAG_ANNOTATIONS;

  for (size_t i=0, n = sizeof (tests) / sizeof (tests[0]); i < n; i++) {
    static idl_pstate_t *pstate = NULL;
    struct descriptor descriptor;
    struct descriptor_type_meta dtm;

    idl_snprintf (idl, sizeof (idl), fmt, tests[i].switch_type, tests[i].label_value);

    printf ("running test for idl: %s\n", idl);

    ret = idl_create_pstate (flags, NULL, &pstate);
    CU_ASSERT_EQUAL_FATAL (ret, IDL_RETCODE_OK);

    memset (&descriptor, 0, sizeof (descriptor)); /* static analyzer */
    ret = generate_test_descriptor (pstate, idl, &descriptor);
    CU_ASSERT_EQUAL_FATAL (ret, tests[i].result_parse);

    if (ret == IDL_RETCODE_OK && tests[i].type_info)
    {
      ret = generate_descriptor_type_meta (pstate, descriptor.topic, &dtm);
      CU_ASSERT_EQUAL_FATAL (ret, tests[i].result_meta);
      descriptor_type_meta_fini (&dtm);
    }

    descriptor_fini (&descriptor);
    idl_delete_pstate (pstate);
  }
}


static void xcdr2_ser (const void *obj, const struct dds_cdrstream_desc *desc, dds_ostreamLE_t *os)
{
  os->x.m_buffer = NULL;
  os->x.m_index = 0;
  os->x.m_size = 0;
  os->x.m_xcdr_version = DDSI_RTPS_CDR_ENC_VERSION_2;
  bool ret = dds_stream_write_sampleLE (os, &dds_cdrstream_default_allocator, obj, desc);
  CU_ASSERT_FATAL (ret);
}

static void xcdr2_deser (unsigned char *buf, uint32_t sz, void **obj, const struct dds_cdrstream_desc *desc)
{
  unsigned char *data;
  uint32_t srcoff = 0;
  DDSRT_WARNING_MSVC_OFF(6326)
  bool bswap = (DDSRT_ENDIAN != DDSRT_LITTLE_ENDIAN);
  DDSRT_WARNING_MSVC_ON(6326)
  if (bswap)
  {
    data = malloc (sz);
    memcpy (data, buf, sz);
    const uint32_t *ret = dds_stream_normalize_xcdr2_data ((char *) data, &srcoff, sz, bswap, desc->ops.ops);
    CU_ASSERT_NOT_EQUAL_FATAL (ret, NULL);
  }
  else
    data = buf;

  dds_istream_t is = { .m_buffer = data, .m_index = 0, .m_size = sz, .m_xcdr_version = DDSI_RTPS_CDR_ENC_VERSION_2 };
  *obj = calloc_no_fail (1, desc->size);
  dds_stream_read (&is, (void *) *obj, &dds_cdrstream_default_allocator, desc->ops.ops);
  if (bswap)
    free (data);
}


typedef struct smember { uint32_t id; uint16_t flags; DDS_XTypes_TypeIdentifier ti; DDS_XTypes_MemberName name; } smember_t;
static struct DDS_XTypes_CompleteStructMember *get_typeobj_struct_member_seq(uint32_t cnt, struct smember *m)
{
  struct DDS_XTypes_CompleteStructMember *member_seq = calloc_no_fail (cnt, sizeof (*member_seq));
  for (uint32_t n = 0; n < cnt; n++)
  {
    member_seq[n] = (DDS_XTypes_CompleteStructMember) { .common = { .member_id = m[n].id, .member_flags = m[n].flags, .member_type_id = m[n].ti } };
    assert (strlen (m[n].name) < sizeof (member_seq[n].detail.name));
    (void) idl_strlcpy (member_seq[n].detail.name, m[n].name, sizeof (member_seq[n].detail.name));
  }
  return member_seq;
}

typedef struct umember { uint32_t id; uint16_t flags; DDS_XTypes_TypeIdentifier ti; DDS_XTypes_MemberName name; uint32_t num_case_labels; int32_t *case_labels; } umember_t;
static struct DDS_XTypes_CompleteUnionMember *get_typeobj_union_member_seq(uint32_t cnt, struct umember *m)
{
  struct DDS_XTypes_CompleteUnionMember *member_seq = calloc_no_fail (cnt, sizeof (*member_seq));
  for (uint32_t n = 0; n < cnt; n++)
  {
    member_seq[n] = (DDS_XTypes_CompleteUnionMember) { .common = { .member_id = m[n].id, .member_flags = m[n].flags, .type_id = m[n].ti } };
    member_seq[n].common.label_seq._maximum = m[n].num_case_labels;
    member_seq[n].common.label_seq._length = m[n].num_case_labels;
    if (m[n].num_case_labels > 0)
    {
      member_seq[n].common.label_seq._buffer = calloc_no_fail (m[n].num_case_labels, sizeof (*member_seq[n].common.label_seq._buffer));
      for (uint32_t cl = 0; cl < m[n].num_case_labels; cl++)
        member_seq[n].common.label_seq._buffer[cl] = m[n].case_labels[cl];
      member_seq[n].common.label_seq._release = true;
    }
    assert (strlen(m[n].name) < sizeof (member_seq[n].detail.name));
    (void) idl_strlcpy (member_seq[n].detail.name, m[n].name, sizeof (member_seq[n].detail.name));
  }
  return member_seq;
}

static DDS_XTypes_TypeObject *get_typeobj_struct(const char *name, uint16_t flags, DDS_XTypes_TypeIdentifier base, uint32_t member_cnt, struct smember *members)
{
  DDS_XTypes_TypeObject *to = calloc_no_fail (1, sizeof (*to));
  to->_d = DDS_XTypes_EK_COMPLETE;
  to->_u.complete = (DDS_XTypes_CompleteTypeObject) {
    ._d = DDS_XTypes_TK_STRUCTURE,
    ._u.struct_type = (DDS_XTypes_CompleteStructType) {
      .struct_flags = flags,
      .header = { .base_type = base },
      .member_seq = {
        ._maximum = member_cnt,
        ._length = member_cnt,
        ._buffer = get_typeobj_struct_member_seq (member_cnt, members),
        ._release = true
      }
    }
  };
  assert (strlen (name) < sizeof (to->_u.complete._u.struct_type.header.detail.type_name));
  (void) idl_strlcpy (to->_u.complete._u.struct_type.header.detail.type_name, name, sizeof (to->_u.complete._u.struct_type.header.detail.type_name));
  return to;
}

static DDS_XTypes_TypeObject *get_typeobj_union(const char *name, uint16_t flags, DDS_XTypes_TypeIdentifier disc_type, uint32_t member_cnt, struct umember *members)
{
  DDS_XTypes_TypeObject *to = calloc_no_fail (1, sizeof (*to));
  to->_d = DDS_XTypes_EK_COMPLETE;
  to->_u.complete = (DDS_XTypes_CompleteTypeObject) {
    ._d = DDS_XTypes_TK_UNION,
    ._u.union_type = (DDS_XTypes_CompleteUnionType) {
      .union_flags = flags,
      .discriminator = { .common = { .member_flags = DDS_XTypes_IS_MUST_UNDERSTAND | DDS_XTypes_TRY_CONSTRUCT_DISCARD, .type_id = disc_type } },
      .member_seq = {
        ._maximum = member_cnt,
        ._length = member_cnt,
        ._buffer = get_typeobj_union_member_seq (member_cnt, members),
        ._release = true
      }
    }
  };
  assert (strlen(name) < sizeof (to->_u.complete._u.union_type.header.detail.type_name));
  (void) idl_strlcpy (to->_u.complete._u.union_type.header.detail.type_name, name, sizeof (to->_u.complete._u.union_type.header.detail.type_name));
  return to;
}


static DDS_XTypes_TypeObject *get_typeobj1 (void)
{
  return get_typeobj_struct (
    "t1::test_struct",
    DDS_XTypes_IS_APPENDABLE,
    (DDS_XTypes_TypeIdentifier) { ._d = DDS_XTypes_TK_NONE },
    8, (smember_t[]) {
      { 0, DDS_XTypes_IS_KEY | DDS_XTypes_IS_MUST_UNDERSTAND | DDS_XTypes_TRY_CONSTRUCT_DISCARD, { ._d = DDS_XTypes_TK_INT64 }, "f1" },
      { 1, DDS_XTypes_IS_OPTIONAL | DDS_XTypes_TRY_CONSTRUCT_DISCARD, { ._d = DDS_XTypes_TI_STRING8_SMALL, ._u.string_sdefn.bound = 0 }, "f2" },
      { 4, DDS_XTypes_IS_EXTERNAL | DDS_XTypes_TRY_CONSTRUCT_DISCARD, { ._d = DDS_XTypes_TK_CHAR8 }, "f3" },
      { 3, DDS_XTypes_TRY_CONSTRUCT_DISCARD, { ._d = DDS_XTypes_TK_INT8 }, "f4" },
      { 8, DDS_XTypes_TRY_CONSTRUCT_DISCARD, { ._d = DDS_XTypes_TK_UINT32 }, "f5" },
      { 7, DDS_XTypes_TRY_CONSTRUCT_DISCARD, { ._d = DDS_XTypes_TK_INT64 }, "f6" },
      { 10, DDS_XTypes_TRY_CONSTRUCT_DISCARD, { ._d = DDS_XTypes_TK_UINT8 }, "f7" },
      { 11, DDS_XTypes_TRY_CONSTRUCT_DISCARD, { ._d = DDS_XTypes_TK_BYTE }, "f8" }
    });
}

static DDS_XTypes_TypeObject *get_typeobj2 (void)
{
  return get_typeobj_struct (
    "t2::test_struct",
    DDS_XTypes_IS_MUTABLE,
    (DDS_XTypes_TypeIdentifier) { ._d = DDS_XTypes_TK_NONE },
    2, (smember_t[]) {
      { 0, DDS_XTypes_IS_OPTIONAL | DDS_XTypes_IS_EXTERNAL | DDS_XTypes_TRY_CONSTRUCT_DISCARD, { ._d = DDS_XTypes_TK_UINT32 }, "f1" },
      { 1, DDS_XTypes_IS_OPTIONAL | DDS_XTypes_IS_EXTERNAL | DDS_XTypes_TRY_CONSTRUCT_DISCARD, { ._d = DDS_XTypes_TK_UINT32 }, "f2" }
    });
}

static DDS_XTypes_TypeObject *get_typeobj3 (void)
{
  return get_typeobj_union (
    "t3::test_union",
    DDS_XTypes_IS_FINAL,
    (DDS_XTypes_TypeIdentifier) { ._d = DDS_XTypes_TK_INT16 },
    2, (umember_t[]) {
      { 0, DDS_XTypes_TRY_CONSTRUCT_DISCARD, { ._d = DDS_XTypes_TK_INT32 }, "f1", 1, (int32_t[]) { 1 } },
      { 1, DDS_XTypes_IS_EXTERNAL | DDS_XTypes_IS_DEFAULT | DDS_XTypes_TRY_CONSTRUCT_DISCARD, { ._d = DDS_XTypes_TI_STRING8_SMALL, ._u.string_sdefn.bound = 0 }, "f2", 2, (int32_t[]) { 2, 3 } }
    });
}

static void get_typeid (DDS_XTypes_TypeIdentifier *ti, DDS_XTypes_TypeObject *to)
{
  memset (ti, 0, sizeof (*ti));
  ti->_d = DDS_XTypes_EK_COMPLETE;
  idl_retcode_t ret = get_type_hash (ti->_u.equivalence_hash, to);
  CU_ASSERT_EQUAL_FATAL (ret, IDL_RETCODE_OK);
  dds_stream_free_sample (to, &dds_cdrstream_default_allocator, DDS_XTypes_TypeObject_desc.m_ops);
  free (to);
}

static DDS_XTypes_TypeObject *get_typeobj4 (void)
{
  /* get type identifier for base type */
  DDS_XTypes_TypeIdentifier ti_base;
  get_typeid (&ti_base, get_typeobj_struct (
    "t4::a",
    DDS_XTypes_IS_MUTABLE | DDS_XTypes_IS_NESTED,
    (DDS_XTypes_TypeIdentifier) { ._d = DDS_XTypes_TK_NONE },
    1, (smember_t[]) {
      { 5, DDS_XTypes_TRY_CONSTRUCT_DISCARD, { ._d = DDS_XTypes_TK_INT32 }, "a1" }
    }));
  return get_typeobj_struct (
    "t4::test_struct",
    DDS_XTypes_IS_MUTABLE,
    ti_base,
    1, (smember_t[]) {
      { 10, DDS_XTypes_TRY_CONSTRUCT_DISCARD, { ._d = DDS_XTypes_TK_INT32 }, "f1" }
    });
}

static DDS_XTypes_TypeObject *get_typeobj5 (void)
{
  DDS_XTypes_TypeIdentifier *ti_long = calloc_no_fail (1, sizeof (*ti_long));
  ti_long->_d = DDS_XTypes_TK_INT32;

  /* get type identifier for typedef */
  DDS_XTypes_TypeIdentifier ti_alias;
  DDS_XTypes_TypeObject *to_alias = calloc_no_fail (1, sizeof (*to_alias));
  to_alias->_d = DDS_XTypes_EK_COMPLETE;
  to_alias->_u.complete = (DDS_XTypes_CompleteTypeObject) {
    ._d = DDS_XTypes_TK_ALIAS,
    ._u.alias_type = (DDS_XTypes_CompleteAliasType) {
      .alias_flags = 0,
      .header = { .detail = { .type_name = "t5::seqshort_t" } },
      .body = { .common = { .related_flags = 0, .related_type = (DDS_XTypes_TypeIdentifier) {
        ._d = DDS_XTypes_TI_PLAIN_SEQUENCE_SMALL,
        ._u.seq_sdefn = {
          .header = { .equiv_kind = DDS_XTypes_EK_BOTH, .element_flags = DDS_XTypes_TRY_CONSTRUCT_DISCARD },
          .bound = 0,
          .element_identifier = ti_long
        }
      } } }
    }
  };
  get_typeid (&ti_alias, to_alias);

  /* get type identifier for sequence<seqshort_t> */
  DDS_XTypes_TypeIdentifier ti_seq = {
    ._d = DDS_XTypes_TI_PLAIN_SEQUENCE_SMALL,
    ._u.seq_sdefn = {
      .header = { .equiv_kind = DDS_XTypes_EK_COMPLETE, .element_flags = DDS_XTypes_TRY_CONSTRUCT_DISCARD },
      .bound = 0
    }
  };
  ti_seq._u.seq_sdefn.element_identifier = calloc_no_fail (1, sizeof (*ti_seq._u.seq_sdefn.element_identifier));
  memcpy (ti_seq._u.seq_sdefn.element_identifier, &ti_alias, sizeof (ti_alias));

  return get_typeobj_struct (
    "t5::test_struct",
    DDS_XTypes_IS_FINAL,
    (DDS_XTypes_TypeIdentifier) { ._d = DDS_XTypes_TK_NONE },
    2, (smember_t[]) {
      { 0, DDS_XTypes_TRY_CONSTRUCT_DISCARD, ti_seq, "f1" },
      { 1, DDS_XTypes_TRY_CONSTRUCT_DISCARD, ti_alias, "f2" }
    });
}

static DDS_XTypes_TypeObject *get_typeobj6 (void)
{
  DDS_XTypes_TypeIdentifier ti_f1, ti_f2, ti_f3;

  /* f1 type identifier: long f1[5] */
  {
    DDS_XTypes_TypeIdentifier *ti_f1_el = calloc_no_fail (1, sizeof (*ti_f1_el));
    ti_f1_el->_d = DDS_XTypes_TK_INT32;

    uint8_t *f1bound_seq = calloc_no_fail (1, sizeof (*f1bound_seq));
    f1bound_seq[0] = 5;
    ti_f1 = (DDS_XTypes_TypeIdentifier) {
      ._d = DDS_XTypes_TI_PLAIN_ARRAY_SMALL,
      ._u.array_sdefn = {
        .header = { .equiv_kind = DDS_XTypes_EK_BOTH, .element_flags = DDS_XTypes_TRY_CONSTRUCT_DISCARD },
        .array_bound_seq = { ._maximum = 1, ._length = 1, ._buffer = f1bound_seq, ._release = true },
        .element_identifier = ti_f1_el
      }
    };
    // These two superfluous lines silence Clang's static analyzer warning of leaking
    // (in the case of this field) f1bound_seq, ti_f1_el if they are only assigned via
    // the compound literal, but it doesn't when assigned like this. Same thing for
    // the other 2 fields.
    ti_f1._u.array_sdefn.array_bound_seq._buffer = f1bound_seq;
    ti_f1._u.array_sdefn.element_identifier = ti_f1_el;
    ti_f1._u.array_sdefn.header.element_flags = DDS_XTypes_TRY_CONSTRUCT_DISCARD;
  }

  /* f2 type identifier: string<555> f2[999][3] */
  {
    DDS_XTypes_TypeIdentifier *ti_f2_el = calloc_no_fail (1, sizeof (*ti_f2_el));
    ti_f2_el->_d = DDS_XTypes_TI_STRING8_LARGE;
    ti_f2_el->_u.string_ldefn.bound = 555;

    uint32_t *f2bound_seq = calloc_no_fail (2, sizeof (*f2bound_seq));
    f2bound_seq[0] = 999;
    f2bound_seq[1] = 3;
    ti_f2 = (DDS_XTypes_TypeIdentifier) {
      ._d = DDS_XTypes_TI_PLAIN_ARRAY_LARGE,
      ._u.array_ldefn = {
        .header = { .equiv_kind = DDS_XTypes_EK_BOTH, .element_flags = DDS_XTypes_TRY_CONSTRUCT_DISCARD },
        .array_bound_seq = { ._maximum = 2, ._length = 2, ._buffer = f2bound_seq, ._release = true },
        .element_identifier = ti_f2_el
      }
    };
    // Clang's static analyzer ...
    ti_f2._u.array_ldefn.array_bound_seq._buffer = f2bound_seq;
    ti_f2._u.array_ldefn.element_identifier = ti_f2_el;
    ti_f2._u.array_ldefn.header.element_flags = DDS_XTypes_TRY_CONSTRUCT_DISCARD;
  }

  /* f3 type identifier: a[3] f3 */
  {
    /* type a identifier */
    DDS_XTypes_TypeIdentifier *ti_a = calloc_no_fail (1, sizeof (*ti_a));
    get_typeid (ti_a, get_typeobj_struct (
      "t6::a",
      DDS_XTypes_IS_FINAL | DDS_XTypes_IS_NESTED,
      (DDS_XTypes_TypeIdentifier) { ._d = DDS_XTypes_TK_NONE },
      1, (smember_t[]) {
        { 0, DDS_XTypes_TRY_CONSTRUCT_DISCARD, { ._d = DDS_XTypes_TK_INT32 }, "a1" }
      }));

    uint8_t *f3bound_seq = calloc_no_fail (1, sizeof (*f3bound_seq));
    f3bound_seq[0] = 3;
    ti_f3 = (DDS_XTypes_TypeIdentifier) {
      ._d = DDS_XTypes_TI_PLAIN_ARRAY_SMALL,
      ._u.array_sdefn = {
        .header = { .equiv_kind = DDS_XTypes_EK_COMPLETE, .element_flags = DDS_XTypes_TRY_CONSTRUCT_DISCARD },
        .array_bound_seq = { ._maximum = 1, ._length = 1, ._buffer = f3bound_seq, ._release = true },
        .element_identifier = ti_a
      }
    };
    // Clang's static analyzer ...
    ti_f3._u.array_sdefn.array_bound_seq._buffer = f3bound_seq;
    ti_f3._u.array_sdefn.element_identifier = ti_a;
    ti_f3._u.array_sdefn.header.element_flags = DDS_XTypes_TRY_CONSTRUCT_DISCARD;
  }

  return get_typeobj_struct (
    "t6::test_struct",
    DDS_XTypes_IS_FINAL,
    (DDS_XTypes_TypeIdentifier) { ._d = DDS_XTypes_TK_NONE },
    3, (smember_t[]) {
      { 0, DDS_XTypes_TRY_CONSTRUCT_DISCARD, ti_f1, "f1" },
      { 1, DDS_XTypes_TRY_CONSTRUCT_DISCARD, ti_f2, "f2" },
      { 2, DDS_XTypes_TRY_CONSTRUCT_DISCARD, ti_f3, "f3" }
    });
}

static DDS_XTypes_TypeObject *get_typeobj7 (void)
{
  DDS_XTypes_TypeIdentifier ti_f1, ti_f2;

  /* f1 type identifier */
  {
    struct DDS_XTypes_CompleteBitflag *flag_seq = calloc_no_fail (2, sizeof (*flag_seq));
    flag_seq[0] = (DDS_XTypes_CompleteBitflag) { .common = { .position = 0, .flags = 0 }, .detail.name = "bm0" };
    flag_seq[1] = (DDS_XTypes_CompleteBitflag) { .common = { .position = 5, .flags = 0 }, .detail.name = "bm5" };

    DDS_XTypes_TypeObject *to_bitmask = calloc_no_fail (1, sizeof (*to_bitmask));
    to_bitmask->_d = DDS_XTypes_EK_COMPLETE;
    to_bitmask->_u.complete = (DDS_XTypes_CompleteTypeObject) {
      ._d = DDS_XTypes_TK_BITMASK,
      ._u.bitmask_type = (DDS_XTypes_CompleteBitmaskType) {
        .bitmask_flags = DDS_XTypes_IS_FINAL,
        .header = { .common.bit_bound = 6, .detail.type_name = "t7::x::bm" },
        .flag_seq = { ._maximum = 2, ._length = 2, ._buffer = flag_seq, ._release = true },
      }
    };
    get_typeid (&ti_f1, to_bitmask);
  }

  /* f2 type identifier */
  {
    struct DDS_XTypes_CompleteEnumeratedLiteral *literal_seq = calloc_no_fail (2, sizeof (*literal_seq));
    literal_seq[0] = (DDS_XTypes_CompleteEnumeratedLiteral) { .common = { .value = 0, .flags = 0 }, .detail.name = "en0" };
    literal_seq[1] = (DDS_XTypes_CompleteEnumeratedLiteral) { .common = { .value = 3, .flags = DDS_XTypes_IS_DEFAULT }, .detail.name = "en3" };

    DDS_XTypes_TypeObject *to_enum = calloc_no_fail (1, sizeof (*to_enum));
    to_enum->_d = DDS_XTypes_EK_COMPLETE;
    to_enum->_u.complete = (DDS_XTypes_CompleteTypeObject) {
      ._d = DDS_XTypes_TK_ENUM,
      ._u.enumerated_type = (DDS_XTypes_CompleteEnumeratedType) {
        .enum_flags = DDS_XTypes_IS_FINAL,
        .header = { .common.bit_bound = 32, .detail.type_name = "t7::x::en" },
        .literal_seq = { ._maximum = 2, ._length = 2, ._buffer = literal_seq, ._release = true },
      }
    };
    get_typeid (&ti_f2, to_enum);
  }

  return get_typeobj_struct (
    "t7::x::test_struct",
    DDS_XTypes_IS_FINAL,
    (DDS_XTypes_TypeIdentifier) { ._d = DDS_XTypes_TK_NONE },
    2, (smember_t[]) {
      { 0, DDS_XTypes_TRY_CONSTRUCT_DISCARD, ti_f1, "f1" },
      { 1, DDS_XTypes_TRY_CONSTRUCT_DISCARD, ti_f2, "f2" }
    });
}

static DDS_XTypes_TypeObject *get_typeobj8 (void)
{
  DDS_XTypes_TypeIdentifier ti_f1;

  /* f1 type identifier: unsigned long long f1[1][1] */
  {
    DDS_XTypes_TypeIdentifier *ti_f1_el = calloc_no_fail (1, sizeof (*ti_f1_el));
    ti_f1_el->_d = DDS_XTypes_TK_UINT64;

    uint8_t *f1bound_seq = calloc_no_fail (2, sizeof (*f1bound_seq));
    f1bound_seq[0] = 1;
    f1bound_seq[1] = 1;
    ti_f1 = (DDS_XTypes_TypeIdentifier) {
      ._d = DDS_XTypes_TI_PLAIN_ARRAY_SMALL,
      ._u.array_sdefn = {
        .header = { .equiv_kind = DDS_XTypes_EK_BOTH, .element_flags = DDS_XTypes_TRY_CONSTRUCT_DISCARD },
        .array_bound_seq = { ._maximum = 2, ._length = 2, ._buffer = f1bound_seq, ._release = true },
        .element_identifier = ti_f1_el
      }
    };
  }

  return get_typeobj_struct (
    "t8::test_struct",
    DDS_XTypes_IS_FINAL,
    (DDS_XTypes_TypeIdentifier) { ._d = DDS_XTypes_TK_NONE },
    1, (smember_t[]) {
      { 0, DDS_XTypes_TRY_CONSTRUCT_DISCARD, ti_f1, "f1" }
    });
}

static DDS_XTypes_TypeObject *get_typeobj9 (void)
{
  DDS_XTypes_TypeIdentifier ti_f;

  /* f1 and f2 type identifier */
  {
    struct DDS_XTypes_CompleteBitflag *flag_seq = calloc_no_fail (2, sizeof (*flag_seq));
    flag_seq[0] = (DDS_XTypes_CompleteBitflag) { .common = { .position = 0, .flags = 0 }, .detail.name = "bm0" };
    flag_seq[1] = (DDS_XTypes_CompleteBitflag) { .common = { .position = 1, .flags = 0 }, .detail.name = "bm1" };

    DDS_XTypes_TypeObject *to_bitmask = calloc_no_fail (1, sizeof (*to_bitmask));
    to_bitmask->_d = DDS_XTypes_EK_COMPLETE;
    to_bitmask->_u.complete = (DDS_XTypes_CompleteTypeObject) {
      ._d = DDS_XTypes_TK_BITMASK,
      ._u.bitmask_type = (DDS_XTypes_CompleteBitmaskType) {
        .bitmask_flags = DDS_XTypes_IS_FINAL,
        .header = { .common.bit_bound = 2, .detail.type_name = "t9::bm" },
        .flag_seq = { ._maximum = 2, ._length = 2, ._buffer = flag_seq, ._release = true },
      }
    };
    get_typeid (&ti_f, to_bitmask);
  }

  return get_typeobj_struct (
    "t9::test_struct",
    DDS_XTypes_IS_FINAL,
    (DDS_XTypes_TypeIdentifier) { ._d = DDS_XTypes_TK_NONE },
    2, (smember_t[]) {
      { 0, DDS_XTypes_TRY_CONSTRUCT_DISCARD, ti_f, "f1" },
      { 1, DDS_XTypes_TRY_CONSTRUCT_DISCARD, ti_f, "f2" }
    });
}

static DDS_XTypes_TypeObject *get_typeobj10 (void)
{
  DDS_XTypes_TypeIdentifier ti_f;

  /* f1 and f2 type identifier */
  {
    struct DDS_XTypes_CompleteEnumeratedLiteral *literal_seq = calloc_no_fail (2, sizeof (*literal_seq));
    literal_seq[0] = (DDS_XTypes_CompleteEnumeratedLiteral) { .common = { .value = 0, .flags = 0 }, .detail.name = "en0" };
    literal_seq[1] = (DDS_XTypes_CompleteEnumeratedLiteral) { .common = { .value = 1, .flags = DDS_XTypes_IS_DEFAULT }, .detail.name = "en1" };

    DDS_XTypes_TypeObject *to_enum = calloc_no_fail (1, sizeof (*to_enum));
    to_enum->_d = DDS_XTypes_EK_COMPLETE;
    to_enum->_u.complete = (DDS_XTypes_CompleteTypeObject) {
      ._d = DDS_XTypes_TK_ENUM,
      ._u.enumerated_type = (DDS_XTypes_CompleteEnumeratedType) {
        .enum_flags = DDS_XTypes_IS_FINAL,
        .header = { .common.bit_bound = 32, .detail.type_name = "t10::en" },
        .literal_seq = { ._maximum = 2, ._length = 2, ._buffer = literal_seq, ._release = true },
      }
    };
    get_typeid (&ti_f, to_enum);
  }

  return get_typeobj_struct (
    "t10::test_struct",
    DDS_XTypes_IS_FINAL,
    (DDS_XTypes_TypeIdentifier) { ._d = DDS_XTypes_TK_NONE },
    2, (smember_t[]) {
      { 0, DDS_XTypes_TRY_CONSTRUCT_DISCARD, ti_f, "f1" },
      { 1, DDS_XTypes_TRY_CONSTRUCT_DISCARD, ti_f, "f2" }
    });
}

static DDS_XTypes_TypeObject *get_typeobj11 (void)
{
  return get_typeobj_union (
    "t11::test_union",
    DDS_XTypes_IS_FINAL,
    (DDS_XTypes_TypeIdentifier) { ._d = DDS_XTypes_TK_CHAR8 },
    2, (umember_t[]) {
      { 99, DDS_XTypes_TRY_CONSTRUCT_DISCARD, { ._d = DDS_XTypes_TK_INT32 }, "f1", 1, (int32_t[]) { 'a' } },
      { 5, DDS_XTypes_IS_DEFAULT | DDS_XTypes_TRY_CONSTRUCT_DISCARD, { ._d = DDS_XTypes_TK_UINT16 }, "f2", 0, (int32_t[]) { 0 } }
    });
}


static DDS_XTypes_TypeObject *get_typeobj12 (void)
{
  DDS_XTypes_TypeIdentifier ti_f1;

  /* f1 type identifier */
  {
    DDS_XTypes_TypeIdentifier *ti_long = calloc_no_fail (1, sizeof (*ti_long));
    ti_long->_d = DDS_XTypes_TK_INT32;

    /* typedef sequence<long> td_seq */
    DDS_XTypes_TypeIdentifier *ti_alias_seq = calloc_no_fail (1, sizeof (*ti_alias_seq));

    DDS_XTypes_TypeObject *to_alias_seq = calloc_no_fail (1, sizeof (*to_alias_seq));
    to_alias_seq->_d = DDS_XTypes_EK_COMPLETE;
    to_alias_seq->_u.complete = (DDS_XTypes_CompleteTypeObject) {
      ._d = DDS_XTypes_TK_ALIAS,
      ._u.alias_type = (DDS_XTypes_CompleteAliasType) {
        .alias_flags = 0,
        .header = { .detail = { .type_name = "t12::td_seq" } },
        .body = { .common = { .related_flags = 0, .related_type = (DDS_XTypes_TypeIdentifier) {
          ._d = DDS_XTypes_TI_PLAIN_SEQUENCE_SMALL,
          ._u.seq_sdefn = {
            .header = { .equiv_kind = DDS_XTypes_EK_BOTH, .element_flags = DDS_XTypes_TRY_CONSTRUCT_DISCARD },
            .bound = 0,
            .element_identifier = ti_long
          }
        } } }
      }
    };
    get_typeid (ti_alias_seq, to_alias_seq);

    /* typedef td_seq td_array[2] */
    uint8_t *bound_seq = calloc_no_fail (1, sizeof (*bound_seq));
    bound_seq[0] = 2;
    DDS_XTypes_TypeObject *to_alias_arr = calloc_no_fail (1, sizeof (*to_alias_arr));
    to_alias_arr->_d = DDS_XTypes_EK_COMPLETE;
    to_alias_arr->_u.complete = (DDS_XTypes_CompleteTypeObject) {
      ._d = DDS_XTypes_TK_ALIAS,
      ._u.alias_type = (DDS_XTypes_CompleteAliasType) {
        .alias_flags = 0,
        .header = { .detail = { .type_name = "t12::td_array" } },
        .body = { .common = { .related_flags = 0, .related_type = (DDS_XTypes_TypeIdentifier) {
          ._d = DDS_XTypes_TI_PLAIN_ARRAY_SMALL,
          ._u.array_sdefn = {
            .header = { .equiv_kind = DDS_XTypes_EK_COMPLETE, .element_flags = DDS_XTypes_TRY_CONSTRUCT_DISCARD },
            .array_bound_seq = { ._maximum = 1, ._length = 1, ._buffer = bound_seq, ._release = true },
            .element_identifier = ti_alias_seq
          }
        } } }
      }
    };
    get_typeid (&ti_f1, to_alias_arr);
  }

  return get_typeobj_struct (
    "t12::test_struct",
    DDS_XTypes_IS_FINAL,
    (DDS_XTypes_TypeIdentifier) { ._d = DDS_XTypes_TK_NONE },
    1, (smember_t[]) {
      { 0, DDS_XTypes_TRY_CONSTRUCT_DISCARD, ti_f1, "f1" },
    });
}

static DDS_XTypes_TypeObject *get_typeobj13 (void)
{
  DDS_XTypes_TypeIdentifier ti_f1;

  /* f1 type identifier */
  {
    DDS_XTypes_TypeIdentifier *ti_long = calloc_no_fail (1, sizeof (*ti_long));
    ti_long->_d = DDS_XTypes_TK_INT32;

    /* typedef long td_arr[3] */
    DDS_XTypes_TypeObject *to_alias_arr = calloc_no_fail (1, sizeof (*to_alias_arr));
    uint8_t *bound_seq = calloc_no_fail (1, sizeof (*bound_seq));
    bound_seq[0] = 3;
    to_alias_arr->_d = DDS_XTypes_EK_COMPLETE;
    to_alias_arr->_u.complete = (DDS_XTypes_CompleteTypeObject) {
      ._d = DDS_XTypes_TK_ALIAS,
      ._u.alias_type = (DDS_XTypes_CompleteAliasType) {
        .alias_flags = 0,
        .header = { .detail = { .type_name = "t13::td_arr" } },
        .body = { .common = { .related_flags = 0, .related_type = (DDS_XTypes_TypeIdentifier) {
          ._d = DDS_XTypes_TI_PLAIN_ARRAY_SMALL,
          ._u.array_sdefn = {
            .header = { .equiv_kind = DDS_XTypes_EK_BOTH, .element_flags = DDS_XTypes_TRY_CONSTRUCT_DISCARD },
            .array_bound_seq = { ._maximum = 1, ._length = 1, ._buffer = bound_seq, ._release = true },
            .element_identifier = ti_long
          }
        } } }
      }
    };

    /* typedef td_seq td */
    DDS_XTypes_TypeObject *to_alias = calloc_no_fail (1, sizeof (*to_alias));
    to_alias->_d = DDS_XTypes_EK_COMPLETE;
    to_alias->_u.complete = (DDS_XTypes_CompleteTypeObject) {
      ._d = DDS_XTypes_TK_ALIAS,
      ._u.alias_type = (DDS_XTypes_CompleteAliasType) {
        .alias_flags = 0,
        .header = { .detail = { .type_name = "t13::td" } },
        .body = { .common = { .related_flags = 0 } }
      }
    };
    get_typeid (&to_alias->_u.complete._u.alias_type.body.common.related_type, to_alias_arr);
    get_typeid (&ti_f1, to_alias);
  }


  return get_typeobj_struct (
    "t13::test_struct",
    DDS_XTypes_IS_FINAL,
    (DDS_XTypes_TypeIdentifier) { ._d = DDS_XTypes_TK_NONE },
    1, (smember_t[]) {
      { 0, DDS_XTypes_TRY_CONSTRUCT_DISCARD, ti_f1, "f1" },
    });
}

static DDS_XTypes_TypeObject *get_typeobj14 (void)
{
  DDS_XTypes_TypeIdentifier ti_f1;

  /* f1 type identifier */
  {
    DDS_XTypes_TypeIdentifier *ti_long = calloc_no_fail (1, sizeof (*ti_long));
    ti_long->_d = DDS_XTypes_TK_INT32;

    DDS_XTypes_TypeIdentifier *ti_seq = calloc_no_fail (1, sizeof (*ti_seq));
    ti_seq->_d = DDS_XTypes_TI_PLAIN_SEQUENCE_SMALL;
    ti_seq->_u.seq_sdefn = (struct DDS_XTypes_PlainSequenceSElemDefn) {
      .header = { .equiv_kind = DDS_XTypes_EK_BOTH, .element_flags = DDS_XTypes_TRY_CONSTRUCT_DISCARD },
      .bound = 0,
      .element_identifier = ti_long
    };

    /* typedef sequence<long> td_seq_arr[2] */
    uint8_t *bound_seq = calloc_no_fail (1, sizeof (*bound_seq));
    bound_seq[0] = 3;
    DDS_XTypes_TypeObject *to_alias_seq_arr = calloc_no_fail (1, sizeof (*to_alias_seq_arr));
    to_alias_seq_arr->_d = DDS_XTypes_EK_COMPLETE;
    to_alias_seq_arr->_u.complete = (DDS_XTypes_CompleteTypeObject) {
      ._d = DDS_XTypes_TK_ALIAS,
      ._u.alias_type = (DDS_XTypes_CompleteAliasType) {
        .alias_flags = 0,
        .header = { .detail = { .type_name = "t14::td_seq_arr" } },
        .body = { .common = { .related_flags = 0, .related_type = (DDS_XTypes_TypeIdentifier) {
          ._d = DDS_XTypes_TI_PLAIN_ARRAY_SMALL,
          ._u.array_sdefn = {
            .header = { .equiv_kind = DDS_XTypes_EK_BOTH, .element_flags = DDS_XTypes_TRY_CONSTRUCT_DISCARD },
            .array_bound_seq = { ._maximum = 1, ._length = 1, ._buffer = bound_seq, ._release = true },
            .element_identifier = ti_seq
          }
        } } }
      }
    };
    get_typeid (&ti_f1, to_alias_seq_arr);
  }

  return get_typeobj_struct (
    "t14::test_struct",
    DDS_XTypes_IS_FINAL,
    (DDS_XTypes_TypeIdentifier) { ._d = DDS_XTypes_TK_NONE },
    1, (smember_t[]) {
      { 0, DDS_XTypes_TRY_CONSTRUCT_DISCARD, ti_f1, "f1" },
    });
}

static DDS_XTypes_TypeObject *get_typeobj15 (void)
{
  DDS_XTypes_TypeIdentifier ti_f1;

  /* f1 type identifier */
  {
    DDS_XTypes_TypeObject *to_s = get_typeobj_struct (
      "t15::s",
      DDS_XTypes_IS_FINAL | DDS_XTypes_IS_NESTED,
      (DDS_XTypes_TypeIdentifier) { ._d = DDS_XTypes_TK_NONE },
      1, (smember_t[]) {
        { 0, DDS_XTypes_TRY_CONSTRUCT_DISCARD, { ._d = DDS_XTypes_TK_INT32 }, "s1" }
      }
    );

    /* typedef s x */
    DDS_XTypes_TypeObject *to_alias = calloc_no_fail (1, sizeof (*to_alias));
    to_alias->_d = DDS_XTypes_EK_COMPLETE;
    to_alias->_u.complete = (DDS_XTypes_CompleteTypeObject) {
      ._d = DDS_XTypes_TK_ALIAS,
      ._u.alias_type = (DDS_XTypes_CompleteAliasType) {
        .alias_flags = 0,
        .header = { .detail = { .type_name = "t15::td_s" } },
        .body = { .common = { .related_flags = 0 } }
      }
    };
    get_typeid (&to_alias->_u.complete._u.alias_type.body.common.related_type, to_s);
    get_typeid (&ti_f1, to_alias);
  }

  return get_typeobj_struct (
    "t15::test_struct",
    DDS_XTypes_IS_FINAL,
    (DDS_XTypes_TypeIdentifier) { ._d = DDS_XTypes_TK_NONE },
    1, (smember_t[]) {
      { 0, DDS_XTypes_IS_EXTERNAL | DDS_XTypes_TRY_CONSTRUCT_DISCARD, ti_f1, "f1" },
    });
}

typedef DDS_XTypes_TypeObject * (*get_typeobj_t) (void);

CU_Test(idlc_type_meta, type_obj_serdes)
{
  idl_retcode_t ret;
  static const struct {
    char idl[256];
    get_typeobj_t get_typeobj_fn;
  } tests[] = {
    { "module t1 { @appendable struct test_struct { @key long long f1; @optional string f2; @external @id(4) char f3; @id(3) int8 f4; @id(8) uint32 f5; @id(7) int64 f6; @id(10) uint8 f7; octet f8; }; };", get_typeobj1 },
    { "module t2 { @mutable struct test_struct { @optional @external unsigned long f1, f2; }; };", get_typeobj2 },
    { "module t3 { @final union test_union switch (short) { case 1: long f1; case 2: case 3: default: @external string f2; }; };", get_typeobj3 },
    { "module t4 { @mutable @nested struct a { @id(5) long a1; }; @mutable @topic struct test_struct : a { @id(10) long f1; }; };", get_typeobj4 },
    { "module t5 { typedef sequence<long> seqshort_t; @final struct test_struct { sequence<seqshort_t> f1; seqshort_t f2; }; };", get_typeobj5 },
    { "module t6 { @final @nested struct a { long a1; }; @final struct test_struct { long f1[5]; string<555> f2[999][3]; a f3[3]; }; };", get_typeobj6 },
    { "module t7 { module x { @bit_bound(6) bitmask bm { @position(5) bm5, @position(0) bm0 }; enum en { @value(3) en3, @value(0) en0 }; @topic @final struct test_struct { bm f1; en f2; }; }; };", get_typeobj7 },
    { "module t8 { @topic struct test_struct { unsigned long long f1[1][1]; }; };", get_typeobj8 },
    { "module t9 { @bit_bound(2) bitmask bm { bm0, bm1 }; @topic @final struct test_struct { bm f1; bm f2; }; };", get_typeobj9 },
    { "module t10 { enum en { en0, @default_literal en1 }; @topic @final struct test_struct { en f1; en f2; }; };", get_typeobj10 },
    { "module t11 { @final union test_union switch (char) { case 'a': @id(99) long f1; default: @id(5) unsigned short f2; }; };", get_typeobj11 },
    { "module t12 { typedef sequence<long> td_seq; typedef td_seq td_array[2]; struct test_struct { td_array f1; }; };", get_typeobj12 },
    { "module t13 { typedef long td_arr[3]; typedef td_arr td; @topic @final struct test_struct { td f1; }; };", get_typeobj13 },
    { "module t14 { typedef sequence<long> td_seq_arr[3]; @final struct test_struct { td_seq_arr f1; }; };", get_typeobj14 },
    { "module t15 { struct s; typedef s td_s; @final struct test_struct { @external td_s f1; }; @final @nested struct s { long s1; }; };", get_typeobj15 }
  };

  uint32_t flags = IDL_FLAG_EXTENDED_DATA_TYPES |
                   IDL_FLAG_ANONYMOUS_TYPES |
                   IDL_FLAG_ANNOTATIONS;

  for (size_t i = 0, n = sizeof (tests) / sizeof (tests[0]); i < n; i++) {
    static idl_pstate_t *pstate = NULL;
    struct descriptor descriptor;
    struct descriptor_type_meta dtm;

    printf ("running test for idl: %s\n", tests[i].idl);

    ret = idl_create_pstate (flags, NULL, &pstate);
    CU_ASSERT_EQUAL_FATAL (ret, IDL_RETCODE_OK);

    memset (&descriptor, 0, sizeof (descriptor)); /* static analyzer */
    ret = generate_test_descriptor (pstate, tests[i].idl, &descriptor);
    CU_ASSERT_EQUAL_FATAL (ret, IDL_RETCODE_OK);

    ret = generate_descriptor_type_meta (pstate, descriptor.topic, &dtm);
    CU_ASSERT_EQUAL_FATAL (ret, IDL_RETCODE_OK);

    for (struct type_meta *tm = dtm.admin; tm; tm = tm->admin_next)
    {
      struct ddsi_typeid_str tidstr;
      const char *type_name = idl_identifier(tm->node);
      printf ("test type %s %s\n", type_name ? type_name : "<anonymous>", ddsi_make_typeid_str_impl (&tidstr, tm->ti_complete));

      // serialize the generated type object
      dds_ostreamLE_t os;
      xcdr2_ser (tm->to_complete, &DDS_XTypes_TypeObject_cdrstream_desc, &os);

      if (tm->node == descriptor.topic)
      {
        dds_ostreamLE_t os_test;
        // serializer the reference type object
        DDS_XTypes_TypeObject *to_test = tests[i].get_typeobj_fn();
        xcdr2_ser (to_test, &DDS_XTypes_TypeObject_cdrstream_desc, &os_test);

        // compare serialized blobs
        CU_ASSERT_EQUAL_FATAL (os.x.m_index, os_test.x.m_index);
        assert (os.x.m_index == os_test.x.m_index);
        int cmp = memcmp (os.x.m_buffer, os_test.x.m_buffer, os.x.m_index);
        CU_ASSERT_EQUAL_FATAL (cmp, 0);

        dds_stream_free_sample (to_test, &dds_cdrstream_default_allocator, DDS_XTypes_TypeObject_desc.m_ops);
        free (to_test);
        dds_ostreamLE_fini (&os_test, &dds_cdrstream_default_allocator);
      }

      // test that generated type object can be serialized
      DDS_XTypes_TypeObject *to;
      xcdr2_deser (os.x.m_buffer, os.x.m_index, (void **)&to, &DDS_XTypes_TypeObject_cdrstream_desc);

      // cleanup
      dds_stream_free_sample (to, &dds_cdrstream_default_allocator, DDS_XTypes_TypeObject_desc.m_ops);
      free (to);
      dds_ostreamLE_fini (&os, &dds_cdrstream_default_allocator);
    }

    descriptor_type_meta_fini (&dtm);
    descriptor_fini (&descriptor);
    idl_delete_pstate (pstate);
  }
}

typedef struct member_annotation_test {
  char *hash_id;
  bool min_present;
  int min;
  bool max_present;
  int max;
  char *unit;
} m_a_t;

typedef struct struct_annotation_test {
  char *idl;
  m_a_t members[8];
} s_a_t;

static void test_annotation_meta_info(const s_a_t *test)
{
  uint32_t flags = IDL_FLAG_EXTENDED_DATA_TYPES |
                   IDL_FLAG_ANONYMOUS_TYPES |
                   IDL_FLAG_ANNOTATIONS;

  static idl_pstate_t *pstate = NULL;
  struct descriptor descriptor;
  struct descriptor_type_meta dtm;

  idl_retcode_t ret = idl_create_pstate (flags, NULL, &pstate);
  CU_ASSERT_EQUAL_FATAL (ret, IDL_RETCODE_OK);

  memset (&descriptor, 0, sizeof (descriptor)); /* static analyzer */
  ret = generate_test_descriptor (pstate, test->idl, &descriptor);
  CU_ASSERT_EQUAL_FATAL (ret, IDL_RETCODE_OK);

  ret = generate_descriptor_type_meta (pstate, descriptor.topic, &dtm);
  CU_ASSERT_EQUAL_FATAL (ret, IDL_RETCODE_OK);

  assert(NULL == dtm.admin->admin_next);  /*only one struct allowed*/
  for (struct type_meta *tm = dtm.admin; tm; tm = tm->admin_next)
  {
    DDS_XTypes_CompleteStructMemberSeq mseq = tm->to_complete->_u.complete._u.struct_type.member_seq;
    for (size_t i = 0; i < mseq._length; i++)
    {
      //CU_ASSERT_FATAL(i < sizeof(test->members)/sizeof(test->members[0]));
      m_a_t m = test->members[i];
      CU_ASSERT_PTR_NOT_NULL_FATAL(mseq._buffer);
      if (!mseq._buffer)
        continue;
      struct DDS_XTypes_CompleteStructMember csm = mseq._buffer[i];
      DDS_XTypes_AppliedBuiltinMemberAnnotations *annptr = csm.detail.ann_builtin;
      CU_ASSERT_PTR_NOT_NULL_FATAL(annptr);
      if (!annptr)
        continue;

      //check hashid (if any)
      if (m.hash_id) {
        CU_ASSERT_PTR_NOT_NULL_FATAL(annptr->hash_id);
        if (annptr->hash_id)
          CU_ASSERT_STRING_EQUAL(m.hash_id, annptr->hash_id);
      } else {
        CU_ASSERT_PTR_NULL(annptr->hash_id);
      }

      //check max (if any)
      if (m.max_present) {
        CU_ASSERT_PTR_NOT_NULL_FATAL(annptr->max);
        if (annptr->max)
          CU_ASSERT_EQUAL(annptr->max->_u.int32_value, m.max);
      } else {
        CU_ASSERT_PTR_NULL(annptr->max);
      }

      //check min (if any)
      if (m.min_present) {
        CU_ASSERT_PTR_NOT_NULL_FATAL(annptr->min);
        if (annptr->min)
          CU_ASSERT_EQUAL(annptr->min->_u.int32_value, m.min);
      } else {
        CU_ASSERT_PTR_NULL(annptr->min);
      }

      //check unit (if any)
      if (m.unit) {
        CU_ASSERT_PTR_NOT_NULL_FATAL(annptr->unit);
        if (annptr->unit)
          CU_ASSERT_STRING_EQUAL(m.unit, annptr->unit);
      } else {
        CU_ASSERT_PTR_NULL(annptr->unit);
      }
    }
  }

  descriptor_type_meta_fini (&dtm);
  descriptor_fini (&descriptor);
  idl_delete_pstate (pstate);
}

CU_Test(idlc_type_meta, type_obj_annotations)
{
  s_a_t tests[] = {
    //min/max/range
    {"struct test_struct {\n"
      "@max(123456) long f1;\n"
      "@min(654321) long f2;\n"
      "@range(min = 123456, max = 654321) long f3;\n"
      "};",
      {{ NULL, false, 0, true, 123456, NULL},
       { NULL, true, 654321, false, 0, NULL},
       { NULL, true, 123456, true, 654321, NULL},
       }},
    //hashid
    {"struct test_struct {\n"
      "@hashid long f1;\n"
      "@hashid(\"abc\") long f2;\n"
      "@hashid(\"def\") long f3;\n"
      "};",
      {{ "" },
       { "abc" },
       { "def" },
        }},
    //unit
    {"struct test_struct {\n"
      "@unit(\"Cubits\") long f1;\n"
      "@unit(\"Heqats\") long f2;\n"
      "@unit(\"Zolotniks\") long f3;\n"
      "};",
      {{ NULL, false, 0, false, 0, "Cubits" },
       { NULL, false, 0, false, 0, "Heqats" },
       { NULL, false, 0, false, 0, "Zolotniks" },
        }},
    //mixing annotations
    {"struct test_struct {\n"
      "@max(123456) @hashid @unit(\"Cubits\") long f1;\n"
      "@min(654321) @hashid(\"abc\") @unit(\"Heqats\") long f2;\n"
      "@range(min = 123456, max = 654321) @hashid(\"def\") @unit(\"Zolotniks\") long f3;\n"
      "};",
      {{ "", false, 0, true, 123456, "Cubits"},
       { "abc", true, 654321, false, 0, "Heqats"},
       { "def", true, 123456, true, 654321, "Zolotniks"},
       }},
    };

  for (size_t i = 0; i < sizeof(tests)/sizeof(tests[0]); i++)
    test_annotation_meta_info(tests + i);
}



#endif /* DDS_HAS_TYPELIB */
