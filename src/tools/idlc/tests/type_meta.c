/*
 * Copyright(c) 2021 ADLINK Technology Limited and others
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
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "dds/ddsc/dds_opcodes.h"
#include "dds/ddsi/q_protocol.h"
#include "dds/ddsi/ddsi_cdrstream.h"
#include "idl/string.h"
#include "descriptor.h"
#include "descriptor_type_meta.h"
#include "plugin.h"
#include "test_common.h"

#include "CUnit/Theory.h"


#ifdef DDS_HAS_TYPE_DISCOVERY

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


static void xcdr2_ser (const void *obj, const dds_topic_descriptor_t *desc, dds_ostream_t *os)
{
  struct ddsi_sertype_default sertype;
  memset (&sertype, 0, sizeof (sertype));
  sertype.type = (struct ddsi_sertype_default_desc) {
    .size = desc->m_size,
    .align = desc->m_align,
    .flagset = desc->m_flagset,
    .keys.nkeys = 0,
    .keys.keys = NULL,
    .ops.nops = dds_stream_countops (desc->m_ops, desc->m_nkeys, desc->m_keys),
    .ops.ops = (uint32_t *) desc->m_ops
  };

  os->m_buffer = NULL;
  os->m_index = 0;
  os->m_size = 0;
  os->m_xcdr_version = CDR_ENC_VERSION_2;
  dds_stream_write_sampleLE ((dds_ostreamLE_t *) os, obj, &sertype);
}

static void xcdr2_deser (unsigned char *buf, uint32_t sz, void **obj, const dds_topic_descriptor_t *desc)
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
    const uint32_t *ret = dds_stream_normalize_data ((char *) data, &srcoff, sz, bswap, CDR_ENC_VERSION_2, desc->m_ops);
    CU_ASSERT_NOT_EQUAL_FATAL (ret, NULL);
  }
  else
    data = buf;

  dds_istream_t is = { .m_buffer = data, .m_index = 0, .m_size = sz, .m_xcdr_version = CDR_ENC_VERSION_2 };
  *obj = calloc (1, desc->m_size);
  dds_stream_read (&is, (void *) *obj, desc->m_ops);
  if (bswap)
    free (data);
}


typedef struct smember { uint32_t id; uint16_t flags; DDS_XTypes_TypeIdentifier ti; DDS_XTypes_MemberName name; } smember_t;
static struct DDS_XTypes_CompleteStructMember *get_typeobj_struct_member_seq(uint32_t cnt, struct smember *m)
{
  struct DDS_XTypes_CompleteStructMember *member_seq = calloc (cnt, sizeof (*member_seq));
  for (uint32_t n = 0; n < cnt; n++)
  {
    member_seq[n] = (DDS_XTypes_CompleteStructMember) { .common = { .member_id = m[n].id, .member_flags = m[n].flags, .member_type_id = m[n].ti } };
    strcpy (member_seq[n].detail.name, m[n].name);
  }
  return member_seq;
}

typedef struct umember { uint32_t id; uint16_t flags; DDS_XTypes_TypeIdentifier ti; DDS_XTypes_MemberName name; uint32_t num_case_labels; int32_t *case_labels; } umember_t;
static struct DDS_XTypes_CompleteUnionMember *get_typeobj_union_member_seq(uint32_t cnt, struct umember *m)
{
  struct DDS_XTypes_CompleteUnionMember *member_seq = calloc (cnt, sizeof (*member_seq));
  for (uint32_t n = 0; n < cnt; n++)
  {
    member_seq[n] = (DDS_XTypes_CompleteUnionMember) { .common = { .member_id = m[n].id, .member_flags = m[n].flags, .type_id = m[n].ti } };
    member_seq[n].common.label_seq._maximum = m[n].num_case_labels;
    member_seq[n].common.label_seq._length = m[n].num_case_labels;
    member_seq[n].common.label_seq._buffer = calloc (m[n].num_case_labels, sizeof (*member_seq[n].common.label_seq._buffer));
    for (uint32_t cl = 0; cl < m[n].num_case_labels; cl++)
      member_seq[n].common.label_seq._buffer[cl] = m[n].case_labels[cl];
    member_seq[n].common.label_seq._release = true;
    strcpy (member_seq[n].detail.name, m[n].name);
  }
  return member_seq;
}

static DDS_XTypes_TypeObject *get_typeobj_struct(const char *name, uint16_t flags, DDS_XTypes_TypeIdentifier base, uint32_t member_cnt, struct smember *members)
{
  DDS_XTypes_TypeObject *to = calloc (1, sizeof (*to));
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
  strcpy (to->_u.complete._u.struct_type.header.detail.type_name, name);
  return to;
}

static DDS_XTypes_TypeObject *get_typeobj_union(const char *name, uint16_t flags, DDS_XTypes_TypeIdentifier disc_type, uint32_t member_cnt, struct umember *members)
{
  DDS_XTypes_TypeObject *to = calloc (1, sizeof (*to));
  to->_d = DDS_XTypes_EK_COMPLETE;
  to->_u.complete = (DDS_XTypes_CompleteTypeObject) {
    ._d = DDS_XTypes_TK_UNION,
    ._u.union_type = (DDS_XTypes_CompleteUnionType) {
      .union_flags = flags,
      .discriminator = { .common = { .member_flags = 0, .type_id = disc_type } },
      .member_seq = {
        ._maximum = member_cnt,
        ._length = member_cnt,
        ._buffer = get_typeobj_union_member_seq (member_cnt, members),
        ._release = true
      }
    }
  };
  strcpy (to->_u.complete._u.union_type.header.detail.type_name, name);
  return to;
}


static DDS_XTypes_TypeObject *get_typeobj1 (void)
{
  return get_typeobj_struct (
    "t1::test_struct",
    DDS_XTypes_IS_APPENDABLE,
    (DDS_XTypes_TypeIdentifier) { ._d = DDS_XTypes_TK_NONE },
    3, (smember_t[]) {
      { 0, DDS_XTypes_IS_KEY, { ._d = DDS_XTypes_TK_INT64 }, "f1" },
      { 1, DDS_XTypes_IS_OPTIONAL, { ._d = DDS_XTypes_TI_STRING8_SMALL, ._u.string_sdefn.bound = 0 }, "f2" },
      { 4, DDS_XTypes_IS_EXTERNAL, { ._d = DDS_XTypes_TK_CHAR8 }, "f3" }
    });
}

static DDS_XTypes_TypeObject *get_typeobj2 (void)
{
  return get_typeobj_struct (
    "t2::test_struct",
    DDS_XTypes_IS_MUTABLE,
    (DDS_XTypes_TypeIdentifier) { ._d = DDS_XTypes_TK_NONE },
    2, (smember_t[]) {
      { 0, DDS_XTypes_IS_OPTIONAL | DDS_XTypes_IS_EXTERNAL, { ._d = DDS_XTypes_TK_UINT32 }, "f1" },
      { 1, DDS_XTypes_IS_OPTIONAL | DDS_XTypes_IS_EXTERNAL, { ._d = DDS_XTypes_TK_UINT32 }, "f2" }
    });
}

static DDS_XTypes_TypeObject *get_typeobj3 (void)
{
  return get_typeobj_union (
    "t3::test_union",
    DDS_XTypes_IS_FINAL,
    (DDS_XTypes_TypeIdentifier) { ._d = DDS_XTypes_TK_INT16 },
    2, (umember_t[]) {
      { 0, 0, { ._d = DDS_XTypes_TK_INT32 }, "f1", 1, (int32_t[]) { 1 } },
      { 1, DDS_XTypes_IS_EXTERNAL | DDS_XTypes_IS_DEFAULT, { ._d = DDS_XTypes_TI_STRING8_SMALL, ._u.string_sdefn.bound = 0 }, "f2", 2, (int32_t[]) { 2, 3 } }
    });
}

static void get_typeid (DDS_XTypes_TypeIdentifier *ti, DDS_XTypes_TypeObject *to)
{
  memset (ti, 0, sizeof (*ti));
  ti->_d = DDS_XTypes_EK_COMPLETE;
  get_type_hash (ti->_u.equivalence_hash, to);
  dds_stream_free_sample (to, DDS_XTypes_TypeObject_desc.m_ops);
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
      { 5, 0, { ._d = DDS_XTypes_TK_INT32 }, "a1" }
    }));
  return get_typeobj_struct (
    "t4::test_struct",
    DDS_XTypes_IS_MUTABLE,
    ti_base,
    1, (smember_t[]) {
      { 10, 0, { ._d = DDS_XTypes_TK_INT32 }, "f1" }
    });
}

static DDS_XTypes_TypeObject *get_typeobj5 (void)
{
  DDS_XTypes_TypeIdentifier *ti_long = calloc (1, sizeof (*ti_long));
  ti_long->_d = DDS_XTypes_TK_INT32;

  /* get type identifier for typedef */
  DDS_XTypes_TypeIdentifier ti_alias;
  DDS_XTypes_TypeObject *to_alias = calloc (1, sizeof (*to_alias));
  to_alias->_d = DDS_XTypes_EK_COMPLETE;
  to_alias->_u.complete = (DDS_XTypes_CompleteTypeObject) {
    ._d = DDS_XTypes_TK_ALIAS,
    ._u.alias_type = (DDS_XTypes_CompleteAliasType) {
      .alias_flags = 0,
      .header = { .detail = { .type_name = "t5::seqshort_t" } },
      .body = { .common = { .related_flags = 0, .related_type = (DDS_XTypes_TypeIdentifier) {
        ._d = DDS_XTypes_TI_PLAIN_SEQUENCE_SMALL,
        ._u.seq_sdefn = {
          .header = { .equiv_kind = DDS_XTypes_EK_BOTH, .element_flags = 0 },
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
      .header = { .equiv_kind = DDS_XTypes_EK_COMPLETE, .element_flags = 0 },
      .bound = 0
    }
  };
  ti_seq._u.seq_sdefn.element_identifier = calloc (1, sizeof (*ti_seq._u.seq_sdefn.element_identifier));
  memcpy (ti_seq._u.seq_sdefn.element_identifier, &ti_alias, sizeof (ti_alias));

  return get_typeobj_struct (
    "t5::test_struct",
    DDS_XTypes_IS_FINAL,
    (DDS_XTypes_TypeIdentifier) { ._d = DDS_XTypes_TK_NONE },
    2, (smember_t[]) {
      { 0, 0, ti_seq, "f1" },
      { 1, 0, ti_alias, "f2" }
    });
}

static DDS_XTypes_TypeObject *get_typeobj6 (void)
{
  DDS_XTypes_TypeIdentifier ti_f1, ti_f2, ti_f3;

  /* f1 type identifier: long f1[5] */
  {
    DDS_XTypes_TypeIdentifier *ti_f1_el = calloc (1, sizeof (*ti_f1_el));
    ti_f1_el->_d = DDS_XTypes_TK_INT32;

    uint8_t *f1bound_seq = calloc (1, sizeof (*f1bound_seq));
    f1bound_seq[0] = 5;
    ti_f1 = (DDS_XTypes_TypeIdentifier) {
      ._d = DDS_XTypes_TI_PLAIN_ARRAY_SMALL,
      ._u.array_sdefn = {
        .header = { .equiv_kind = DDS_XTypes_EK_BOTH, .element_flags = 0 },
        .array_bound_seq = { ._maximum = 1, ._length = 1, ._buffer = f1bound_seq, ._release = true },
        .element_identifier = ti_f1_el
      }
    };
  }

  /* f2 type identifier: string<555> f2[999][3] */
  {
    DDS_XTypes_TypeIdentifier *ti_f2_el = calloc (1, sizeof (*ti_f2_el));
    ti_f2_el->_d = DDS_XTypes_TI_STRING8_LARGE;
    ti_f2_el->_u.string_ldefn.bound = 555;

    uint32_t *f2bound_seq = calloc (2, sizeof (*f2bound_seq));
    f2bound_seq[0] = 999;
    f2bound_seq[1] = 3;
    ti_f2 = (DDS_XTypes_TypeIdentifier) {
      ._d = DDS_XTypes_TI_PLAIN_ARRAY_LARGE,
      ._u.array_ldefn = {
        .header = { .equiv_kind = DDS_XTypes_EK_BOTH, .element_flags = 0 },
        .array_bound_seq = { ._maximum = 2, ._length = 2, ._buffer = f2bound_seq, ._release = true },
        .element_identifier = ti_f2_el
      }
    };
  }

  /* f3 type identifier: a[3] f3 */
  {
    /* type a identifier */
    DDS_XTypes_TypeIdentifier *ti_a = calloc (1, sizeof (*ti_a));
    get_typeid (ti_a, get_typeobj_struct (
      "t6::a",
      DDS_XTypes_IS_FINAL | DDS_XTypes_IS_NESTED,
      (DDS_XTypes_TypeIdentifier) { ._d = DDS_XTypes_TK_NONE },
      1, (smember_t[]) {
        { 0, 0, { ._d = DDS_XTypes_TK_INT32 }, "a1" }
      }));

    uint8_t *f3bound_seq = calloc (1, sizeof (*f3bound_seq));
    f3bound_seq[0] = 3;
    ti_f3 = (DDS_XTypes_TypeIdentifier) {
      ._d = DDS_XTypes_TI_PLAIN_ARRAY_SMALL,
      ._u.array_sdefn = {
        .header = { .equiv_kind = DDS_XTypes_EK_COMPLETE, .element_flags = 0 },
        .array_bound_seq = { ._maximum = 1, ._length = 1, ._buffer = f3bound_seq, ._release = true },
        .element_identifier = ti_a
      }
    };
  }

  return get_typeobj_struct (
    "t6::test_struct",
    DDS_XTypes_IS_FINAL,
    (DDS_XTypes_TypeIdentifier) { ._d = DDS_XTypes_TK_NONE },
    3, (smember_t[]) {
      { 0, 0, ti_f1, "f1" },
      { 1, 0, ti_f2, "f2" },
      { 2, 0, ti_f3, "f3" }
    });
}

static DDS_XTypes_TypeObject *get_typeobj7 (void)
{
  DDS_XTypes_TypeIdentifier ti_f1, ti_f2;

  /* f1 type identifier */
  {
    struct DDS_XTypes_CompleteBitflag *flag_seq = calloc (2, sizeof (*flag_seq));
    flag_seq[0] = (DDS_XTypes_CompleteBitflag) { .common = { .position = 0, .flags = 0 }, .detail.name = "bm0" };
    flag_seq[1] = (DDS_XTypes_CompleteBitflag) { .common = { .position = 5, .flags = 0 }, .detail.name = "bm5" };

    DDS_XTypes_TypeObject *to_bitmask = calloc (1, sizeof (*to_bitmask));
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
    struct DDS_XTypes_CompleteEnumeratedLiteral *literal_seq = calloc (2, sizeof (*literal_seq));
    literal_seq[0] = (DDS_XTypes_CompleteEnumeratedLiteral) { .common = { .value = 0, .flags = 0 }, .detail.name = "en0" };
    literal_seq[1] = (DDS_XTypes_CompleteEnumeratedLiteral) { .common = { .value = 3, .flags = 0 }, .detail.name = "en3" };

    DDS_XTypes_TypeObject *to_enum = calloc (1, sizeof (*to_enum));
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
      { 0, 0, ti_f1, "f1" },
      { 1, 0, ti_f2, "f2" }
    });
}

static DDS_XTypes_TypeObject *get_typeobj8 (void)
{
  DDS_XTypes_TypeIdentifier ti_f1;

  /* f1 type identifier: unsigned long long f1[1][1] */
  {
    DDS_XTypes_TypeIdentifier *ti_f1_el = calloc (1, sizeof (*ti_f1_el));
    ti_f1_el->_d = DDS_XTypes_TK_UINT64;

    uint8_t *f1bound_seq = calloc (2, sizeof (*f1bound_seq));
    f1bound_seq[0] = 1;
    f1bound_seq[1] = 1;
    ti_f1 = (DDS_XTypes_TypeIdentifier) {
      ._d = DDS_XTypes_TI_PLAIN_ARRAY_SMALL,
      ._u.array_sdefn = {
        .header = { .equiv_kind = DDS_XTypes_EK_BOTH, .element_flags = 0 },
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
      { 0, 0, ti_f1, "f1" }
    });
}

static DDS_XTypes_TypeObject *get_typeobj9 (void)
{
  DDS_XTypes_TypeIdentifier ti_f;

  /* f1 and f2 type identifier */
  {
    struct DDS_XTypes_CompleteBitflag *flag_seq = calloc (2, sizeof (*flag_seq));
    flag_seq[0] = (DDS_XTypes_CompleteBitflag) { .common = { .position = 0, .flags = 0 }, .detail.name = "bm0" };
    flag_seq[1] = (DDS_XTypes_CompleteBitflag) { .common = { .position = 1, .flags = 0 }, .detail.name = "bm1" };

    DDS_XTypes_TypeObject *to_bitmask = calloc (1, sizeof (*to_bitmask));
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
      { 0, 0, ti_f, "f1" },
      { 1, 0, ti_f, "f2" }
    });
}

static DDS_XTypes_TypeObject *get_typeobj10 (void)
{
  DDS_XTypes_TypeIdentifier ti_f;

  /* f1 and f2 type identifier */
  {
    struct DDS_XTypes_CompleteEnumeratedLiteral *literal_seq = calloc (2, sizeof (*literal_seq));
    literal_seq[0] = (DDS_XTypes_CompleteEnumeratedLiteral) { .common = { .value = 0, .flags = 0 }, .detail.name = "en0" };
    literal_seq[1] = (DDS_XTypes_CompleteEnumeratedLiteral) { .common = { .value = 1, .flags = 0 }, .detail.name = "en1" };

    DDS_XTypes_TypeObject *to_enum = calloc (1, sizeof (*to_enum));
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
      { 0, 0, ti_f, "f1" },
      { 1, 0, ti_f, "f2" }
    });
}

static DDS_XTypes_TypeObject *get_typeobj11 (void)
{
  return get_typeobj_union (
    "t11::test_union",
    DDS_XTypes_IS_FINAL,
    (DDS_XTypes_TypeIdentifier) { ._d = DDS_XTypes_TK_CHAR8 },
    2, (umember_t[]) {
      { 99, 0, { ._d = DDS_XTypes_TK_INT32 }, "f1", 1, (int32_t[]) { 'a' } },
      { 5, DDS_XTypes_IS_DEFAULT, { ._d = DDS_XTypes_TK_UINT16 }, "f2", 0, (int32_t[]) { } }
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
    { "module t1 { @appendable struct test_struct { @key long long f1; @optional string f2; @external @id(4) char f3; }; };", get_typeobj1 },
    { "module t2 { @mutable struct test_struct { @optional @external unsigned long f1, f2; }; };", get_typeobj2 },
    { "module t3 { @final union test_union switch (short) { case 1: long f1; case 2: case 3: default: @external string f2; }; };", get_typeobj3 },
    { "module t4 { @mutable @nested struct a { @id(5) long a1; }; @mutable @topic struct test_struct : a { @id(10) long f1; }; };", get_typeobj4 },
    { "module t5 { typedef sequence<long> seqshort_t; @final struct test_struct { sequence<seqshort_t> f1; seqshort_t f2; }; };", get_typeobj5 },
    { "module t6 { @final @nested struct a { long a1; }; @final struct test_struct { long f1[5]; string<555> f2[999][3]; a f3[3]; }; };", get_typeobj6 },
    { "module t7 { module x { @bit_bound(6) bitmask bm { @position(5) bm5, @position(0) bm0 }; enum en { @value(3) en3, @value(0) en0 }; @topic @final struct test_struct { bm f1; en f2; }; }; };", get_typeobj7 },
    { "module t8 { @topic struct test_struct { unsigned long long f1[1][1]; }; };", get_typeobj8 },
    { "module t9 { @bit_bound(2) bitmask bm { bm0, bm1 }; @topic @final struct test_struct { bm f1; bm f2; }; };", get_typeobj9 },
    { "module t10 { enum en { en0, en1 }; @topic @final struct test_struct { en f1; en f2; }; };", get_typeobj10 },
    { "module t11 { @final union test_union switch (char) { case 'a': @id(99) long f1; default: @id(5) unsigned short f2; }; };", get_typeobj11 },
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
      const char *type_name = idl_identifier(tm->node);
      printf ("test type %s "PTYPEIDFMT"\n", type_name ? type_name : "<anonymous>", PTYPEID (*tm->ti_complete));

      // serialize the generated type object
      dds_ostream_t os;
      xcdr2_ser (tm->to_complete, &DDS_XTypes_TypeObject_desc, &os);

      if (tm->node == descriptor.topic)
      {
        dds_ostream_t os_test;
        // serializer the reference type object
        DDS_XTypes_TypeObject *to_test = tests[i].get_typeobj_fn();
        xcdr2_ser (to_test, &DDS_XTypes_TypeObject_desc, &os_test);

        // compare serialized blobs
        CU_ASSERT_EQUAL_FATAL (os.m_index, os_test.m_index);
        assert (os.m_index == os_test.m_index);
        int cmp = memcmp (os.m_buffer, os_test.m_buffer, os.m_index);
        CU_ASSERT_EQUAL_FATAL (cmp, 0);

        dds_stream_free_sample (to_test, DDS_XTypes_TypeObject_desc.m_ops);
        free (to_test);
        dds_ostream_fini (&os_test);
        }

      // test that generated type object can be serialized
      DDS_XTypes_TypeObject *to;
      xcdr2_deser (os.m_buffer, os.m_index, (void **)&to, &DDS_XTypes_TypeObject_desc);

      // cleanup
      dds_stream_free_sample (to, DDS_XTypes_TypeObject_desc.m_ops);
      free (to);
      dds_ostream_fini (&os);
    }

    descriptor_type_meta_fini (&dtm);
    descriptor_fini (&descriptor);
    idl_delete_pstate (pstate);
  }
}


#endif /* DDS_HAS_TYPE_DISCOVERY */
