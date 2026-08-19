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

#include "dds/ddsc/dds_opcodes.h"
#include "idl/string.h"
#include "libidlc/libidlc__descriptor.h"
#include "idlc/generator.h"
#include "test_common.h"

#include "CUnit/Theory.h"

#define TEST_MAX_KEYS 10
#define TEST_MAX_KEY_OFFS 10
CU_Test(idlc_descriptor, keys_nested)
{
  static const struct {
    const char *idl;
    uint32_t n_keys;
    uint32_t n_key_offs; // number of key offset: the sum of (1 + number of nesting levels) for all keys
    bool keylist; // indicates if pragma keylist is used
    uint32_t key_order[TEST_MAX_KEYS][TEST_MAX_KEY_OFFS]; // key order scoped to containing type
    const char *key_name[TEST_MAX_KEYS];
    uint32_t key_index[TEST_MAX_KEYS]; // key index as printed in the dds key descriptor, indicates the index (order 0..n) of the key in the CDR
  } tests[] = {
    { "struct test { @key @id(2) long a; short b; }; ",
      1, 2, false, { { 2 } }, { "a" }, { 0 } },
    { "struct test { @key long a; @key short b; }; ",
      2, 4, false, { { 0 }, { 1 } }, { "a", "b" }, { 0, 1 } },
    { "@nested struct inner { @id(3) long i1; @id(1) short i2; }; struct outer { @key inner o1; }; ",
      2, 6, false, { { 0, 1 }, { 0, 3 } }, { "o1.i2", "o1.i1" }, { 1, 0 } },
    { "@nested struct inner { long i1; @key short i2; }; struct outer { @key inner o1; }; ",
      1, 3, false, { { 0, 1 } }, { "o1.i2" }, { 0 } },
    { "@nested struct inner { @key @id(5) short i1; }; struct outer { @key @id(0) inner o1; @key @id(10) inner o2; }; ",
      2, 6, false, { { 0, 5 }, { 10, 5 } }, { "o1.i1", "o2.i1" }, { 0, 1 } },
    { "@nested struct inner { @key short i1; }; @nested struct mid { @key @id(3) char m1; @key @id(2) inner m2; @id(1) long m3; }; struct outer { @key @id(0) mid o1; @key @id(1) inner o2; }; ",
      3, 10, false, { { 0, 2, 0 }, { 0, 3 }, { 1, 0 } }, { "o1.m2.i1", "o1.m1", "o2.i1" }, { 1, 0, 2 } },
    { "@nested struct inner { char i1; @key char i2; }; struct outer { @key @id(3) inner o1; @key @id(2) short o2; }; ",
      2, 5, false, { { 2 }, { 3, 1 } }, { "o2", "o1.i2" }, { 1, 0 } },

    { "struct test { long a; short b; }; \n#pragma keylist test a",
      1, 2, true, { { 0 } }, { "a" }, { 0 } },
    { "struct test { long a; short b; }; \n#pragma keylist test a b",
      2, 4, true, { { 0 }, { 1 } }, { "a", "b" }, { 0, 1 } },
    { "struct inner { long i1; short i2; }; struct outer { inner o1; inner o2; }; \n#pragma keylist outer o1.i1",
      1, 3, true, { { 0, 0 } }, { "o1.i1" }, { 0 } },
    { "struct inner { long i1; short i2; }; struct outer { inner o1; inner o2; }; \n#pragma keylist outer o1.i1 o2.i1",
      2, 6, true, { { 0, 0 }, { 1, 0 } }, { "o1.i1", "o2.i1" }, { 0, 1 } },
    { "struct inner { long i1; long i2; }; struct mid { inner m1; }; struct outer { inner o1, o2; inner o3[3]; mid o4; double o5; }; \n#pragma keylist outer o4.m1.i2",
      1, 4, true, { { 3, 0, 1 } }, { "o4.m1.i2" }, { 0 } },

    // type 'outer' should not get keys of other types using the same type 'inner' */
    { "struct inner { long i1; short i2; }; struct outer { inner o1; inner o2; }; \n"
      "#pragma keylist outer o1.i1 \n "
      "struct p { inner p1; }; \n"
      "#pragma keylist p p1.i1 \n",
      1, 3, true, { { 0, 0 } }, { "o1.i1" }, { 0 } },
    { "struct inner { long i1; short i2; }; struct outer { inner o1; inner o2; }; \n"
      "#pragma keylist outer \n"
      "struct p { inner p1; }; \n"
      "#pragma keylist p p1.i1 \n",
      0, 0, true, { { 0 } }, { "" }, { 0 } },

    // key fields ordered by member id, not by order used in keylist
    { "struct inner { long long i1; }; struct outer { inner o1; inner o2; }; \n#pragma keylist outer o2.i1 o1.i1",
      2, 6, true, { { 0, 0 }, { 1, 0 } }, { "o1.i1", "o2.i1" }, { 0, 1 } },
    { "struct inner { char i1; }; struct mid { short m1; inner m2; long m3; }; struct outer { mid o1; inner o2; }; \n#pragma keylist outer o1.m1 o2.i1 o1.m2.i1",
      3, 10, true, { { 0, 0 }, { 0, 1, 0 }, { 1, 0 } }, { "o1.m1", "o1.m2.i1", "o2.i1" }, { 0, 1, 2 } },
  };

  idl_retcode_t ret;
  uint32_t flags = IDL_FLAG_EXTENDED_DATA_TYPES |
                   IDL_FLAG_ANONYMOUS_TYPES |
                   IDL_FLAG_ANNOTATIONS;
  for (size_t i = 0; i < sizeof (tests) / sizeof (tests[0]); i++) {
    static idl_pstate_t *pstate = NULL;
    struct descriptor descriptor;

    printf ("running test for idl: %s\n", tests[i].idl);

    ret = idl_create_pstate (flags | (tests[i].keylist ? IDL_FLAG_KEYLIST : 0), NULL, &pstate);
    CU_ASSERT_EQ_FATAL (ret, IDL_RETCODE_OK);

    memset (&descriptor, 0, sizeof (descriptor)); /* static analyzer */
    ret = generate_test_descriptor (pstate, tests[i].idl, &descriptor);
    CU_ASSERT_EQ_FATAL (ret, IDL_RETCODE_OK);

    CU_ASSERT_EQ_FATAL (descriptor.n_keys, tests[i].n_keys);
    CU_ASSERT_EQ_FATAL (descriptor.key_offsets.count, tests[i].n_key_offs);
    CU_ASSERT_EQ_FATAL (pstate->keylists, tests[i].keylist);

    for (uint32_t k = 0; k < descriptor.n_keys; k++) {
      for (uint32_t j = 0; j < descriptor.keys[k].n_order; j++)
        CU_ASSERT_EQ_FATAL (descriptor.keys[k].order[j], tests[i].key_order[k][j]);
      CU_ASSERT_NEQ_FATAL (descriptor.keys[k].name, NULL);
      CU_ASSERT_STREQ_FATAL (descriptor.keys[k].name, tests[i].key_name[k]);
      CU_ASSERT_EQ_FATAL (descriptor.keys[k].key_idx, tests[i].key_index[k]);
    }

    descriptor_fini (&descriptor);
    idl_delete_pstate (pstate);
  }
}
#undef TEST_MAX_KEYS
#undef TEST_MAX_KEY_OFFS

#define S(ann) ann " struct s { char f; };"
#define U(ann) ann " union u switch(short) { case 1: char f; };"
CU_Test(idlc_descriptor, default_extensibility)
{
  idl_retcode_t ret;
  static const struct {
    const char *idl;
    idl_extensibility_t default_ext;
    idl_extensibility_t exp_ext;
  } tests[] = {
    { S(""), IDL_FINAL, IDL_FINAL },
    { S(""), IDL_APPENDABLE, IDL_APPENDABLE },
    { S(""), IDL_MUTABLE, IDL_MUTABLE },
    { U(""), IDL_FINAL, IDL_FINAL },
    { U(""), IDL_APPENDABLE, IDL_APPENDABLE },
    { U(""), IDL_MUTABLE, IDL_MUTABLE },
    { S("@appendable"), IDL_MUTABLE, IDL_APPENDABLE },
    { S("@extensibility(MUTABLE)"), IDL_APPENDABLE, IDL_MUTABLE },
    { U("@appendable"), IDL_FINAL, IDL_APPENDABLE },
    { U("@extensibility(APPENDABLE)"), IDL_MUTABLE, IDL_APPENDABLE },
    { U("@extensibility(MUTABLE)"), IDL_APPENDABLE, IDL_MUTABLE },
  };

  uint32_t flags = IDL_FLAG_EXTENDED_DATA_TYPES |
                   IDL_FLAG_ANONYMOUS_TYPES |
                   IDL_FLAG_ANNOTATIONS;

  for (size_t i = 0; i < sizeof (tests) / sizeof (tests[0]); i++) {
    static idl_pstate_t *pstate = NULL;
    struct descriptor descriptor;

    printf ("running test for idl: %s\n", tests[i].idl);
    ret = idl_create_pstate (flags, NULL, &pstate);
    pstate->config.default_extensibility = (int) tests[i].default_ext;
    CU_ASSERT_EQ_FATAL (ret, IDL_RETCODE_OK);
    memset (&descriptor, 0, sizeof (descriptor)); /* static analyzer */
    ret = idl_parse_string(pstate, tests[i].idl);
    CU_ASSERT_EQ_FATAL (ret, IDL_RETCODE_OK);
    CU_ASSERT_NEQ_FATAL (pstate->root, NULL);
    ret = generate_descriptor_impl(pstate, pstate->root, &descriptor);
    CU_ASSERT_EQ_FATAL (ret, IDL_RETCODE_OK);

    uint32_t instr1 = 0;
    assert (descriptor.constructed_types);
    assert (descriptor.constructed_types->instructions.table);
    if (descriptor.constructed_types->instructions.table[0].type == OPCODE)
      instr1 = descriptor.constructed_types->instructions.table[0].data.opcode.code;
    const bool is_union = idl_is_union (descriptor.constructed_types->node);
    switch (tests[i].exp_ext) {
      case IDL_FINAL:
        CU_ASSERT_FATAL (DDS_OP (instr1) != DDS_OP_DLC && DDS_OP (instr1) != DDS_OP_PLC);
        break;
      case IDL_APPENDABLE:
        CU_ASSERT_EQ_FATAL (DDS_OP (instr1), DDS_OP_DLC);
        CU_ASSERT_FATAL (DDS_OP_DLC_REQUIRED_PREFIX (instr1) != 0);
        break;
      case IDL_MUTABLE:
        CU_ASSERT_EQ_FATAL (instr1, DDS_OP_PLC);
        if (is_union) {
          const uint32_t instr2 = descriptor.constructed_types->instructions.table[1].data.opcode.code;
          CU_ASSERT_EQ_FATAL (DDS_OP (instr2), DDS_OP_ADR);
          CU_ASSERT_EQ_FATAL (DDS_OP_TYPE (instr2), DDS_OP_VAL_UNI);
        }
        break;
    }
    descriptor_fini (&descriptor);
    CU_ASSERT_EQ_FATAL (ret, IDL_RETCODE_OK);
    idl_delete_pstate (pstate);
  }
}

#undef S
#undef U

static const struct instruction *instruction_at_absolute_offset (const struct descriptor *descriptor, uint32_t offs)
{
  for (const struct constructed_type *ctype = descriptor->constructed_types; ctype; ctype = ctype->next)
  {
    if (offs >= ctype->offset && offs < ctype->offset + ctype->instructions.count)
      return &ctype->instructions.table[offs - ctype->offset];
  }
  return NULL;
}

static bool instruction_uses_enum_value_metadata (const struct instruction *inst)
{
  if (inst == NULL || inst->type != OPCODE)
    return false;

  const uint32_t op = inst->data.opcode.code;
  switch (DDS_OP (op))
  {
    case DDS_OP_ADR:
      switch (DDS_OP_TYPE (op))
      {
        case DDS_OP_VAL_ENU:
          return true;
        case DDS_OP_VAL_UNI:
          return DDS_OP_SUBTYPE (op) == DDS_OP_VAL_ENU;
        case DDS_OP_VAL_SEQ:
        case DDS_OP_VAL_BSQ:
        case DDS_OP_VAL_ARR:
          return DDS_OP_SUBTYPE (op) == DDS_OP_VAL_ENU;
        default:
          return false;
      }
    case DDS_OP_JEQ4:
      return DDS_OP_TYPE (op) == DDS_OP_VAL_ENU;
    default:
      return false;
  }
}

CU_Test(idlc_descriptor, enum_value_metadata_union_case_sequence)
{
  idl_retcode_t ret;
  idl_pstate_t *pstate = NULL;
  struct descriptor descriptor;
  uint32_t flags = IDL_FLAG_EXTENDED_DATA_TYPES |
                   IDL_FLAG_ANONYMOUS_TYPES |
                   IDL_FLAG_ANNOTATIONS;
  const char *idl =
    "enum e { A, @default_literal B, @value(4) C }; "
    "@nested union u switch(long) { "
    "case 1: sequence<long> a; "
    "case 2: sequence<e> b; "
    "case 3: string c; "
    "default: string d; "
    "}; "
    "@topic struct s { sequence<u, 4> us; };";

  ret = idl_create_pstate (flags, NULL, &pstate);
  CU_ASSERT_EQ_FATAL (ret, IDL_RETCODE_OK);
  memset (&descriptor, 0, sizeof (descriptor));
  ret = generate_test_descriptor (pstate, idl, &descriptor);
  CU_ASSERT_EQ_FATAL (ret, IDL_RETCODE_OK);

  uint32_t evm_count = 0;
  for (uint32_t n = 0; n + 1 < descriptor.member_ids.count; n++)
  {
    const struct instruction *evm = &descriptor.member_ids.table[n];
    if (evm->type != OPCODE || DDS_OP (evm->data.opcode.code) != DDS_OP_EVM)
      continue;

    const uint32_t offs = evm->data.opcode.code & DDS_MID_OFFSET_MASK;
    CU_ASSERT_FATAL (instruction_uses_enum_value_metadata (instruction_at_absolute_offset (&descriptor, offs)));
    evm_count++;
  }
  CU_ASSERT_EQ_FATAL (evm_count, 1);

  descriptor_fini (&descriptor);
  idl_delete_pstate (pstate);
}

CU_Test(idlc_descriptor, mutable_union_member_ids)
{
  idl_retcode_t ret;
  idl_pstate_t *pstate = NULL;
  struct descriptor descriptor;
  uint32_t flags = IDL_FLAG_EXTENDED_DATA_TYPES |
                   IDL_FLAG_ANONYMOUS_TYPES |
                   IDL_FLAG_ANNOTATIONS;
  const char *idl =
    "@topic @mutable union test switch(long) { "
    "case 1: case 2: @id(7) long a; "
    "case 3: @id(8) short b; "
    "};";

  ret = idl_create_pstate (flags, NULL, &pstate);
  CU_ASSERT_EQ_FATAL (ret, IDL_RETCODE_OK);
  memset (&descriptor, 0, sizeof (descriptor));
  ret = generate_test_descriptor (pstate, idl, &descriptor);
  CU_ASSERT_EQ_FATAL (ret, IDL_RETCODE_OK);

  const struct constructed_type *ctype = descriptor.constructed_types;
  CU_ASSERT_FATAL (ctype != NULL);
  CU_ASSERT_FATAL (ctype->instructions.table != NULL);
  CU_ASSERT_EQ_FATAL (ctype->instructions.table[0].type, OPCODE);
  CU_ASSERT_EQ_FATAL (ctype->instructions.table[0].data.opcode.code, DDS_OP_PLC);
  CU_ASSERT_EQ_FATAL (ctype->instructions.table[1].type, OPCODE);
  CU_ASSERT_EQ_FATAL (DDS_OP (ctype->instructions.table[1].data.opcode.code), DDS_OP_ADR);
  CU_ASSERT_EQ_FATAL (DDS_OP_TYPE (ctype->instructions.table[1].data.opcode.code), DDS_OP_VAL_UNI);

  uint32_t jeq_count = 0;
  uint32_t mid_count = 0;
  uint32_t mid_7_count = 0;
  uint32_t mid_8_count = 0;
  for (uint32_t n = 0; n < ctype->instructions.count; n++)
    if (ctype->instructions.table[n].type == OPCODE &&
        DDS_OP (ctype->instructions.table[n].data.opcode.code) == DDS_OP_JEQ4)
      jeq_count++;
  for (uint32_t n = 0; n + 1 < descriptor.member_ids.count; n++)
  {
    const struct instruction *mid = &descriptor.member_ids.table[n];
    if (mid->type != MEMBER_ID)
      continue;
    CU_ASSERT_EQ_FATAL (descriptor.member_ids.table[n + 1].type, SINGLE);
    CU_ASSERT_FATAL (mid->data.member_id.addr_offs >= 0);
    CU_ASSERT_LT_FATAL ((uint32_t) mid->data.member_id.addr_offs, ctype->instructions.count);
    CU_ASSERT_EQ_FATAL (ctype->instructions.table[mid->data.member_id.addr_offs].type, OPCODE);
    CU_ASSERT_EQ_FATAL (DDS_OP (ctype->instructions.table[mid->data.member_id.addr_offs].data.opcode.code), DDS_OP_JEQ4);
    mid_count++;
    switch (descriptor.member_ids.table[n + 1].data.single) {
      case 7: mid_7_count++; break;
      case 8: mid_8_count++; break;
      default: CU_FAIL_FATAL ("unexpected mutable union member id");
    }
  }
  CU_ASSERT_EQ_FATAL (jeq_count, 3);
  CU_ASSERT_EQ_FATAL (mid_count, 3);
  CU_ASSERT_EQ_FATAL (mid_7_count, 2);
  CU_ASSERT_EQ_FATAL (mid_8_count, 1);

  descriptor_fini (&descriptor);
  idl_delete_pstate (pstate);
}

static void assert_union_case_labels (const char *idl, uint32_t nlabels, const char * const *labels)
{
  idl_retcode_t ret;
  idl_pstate_t *pstate = NULL;
  struct descriptor descriptor;
  uint32_t flags = IDL_FLAG_EXTENDED_DATA_TYPES |
                   IDL_FLAG_ANONYMOUS_TYPES |
                   IDL_FLAG_ANNOTATIONS;

  ret = idl_create_pstate (flags, NULL, &pstate);
  CU_ASSERT_EQ_FATAL (ret, IDL_RETCODE_OK);
  memset (&descriptor, 0, sizeof (descriptor));
  ret = generate_test_descriptor (pstate, idl, &descriptor);
  CU_ASSERT_EQ_FATAL (ret, IDL_RETCODE_OK);

  const struct constructed_type *ctype = descriptor.constructed_types;
  CU_ASSERT_FATAL (ctype != NULL);
  CU_ASSERT_FATAL (ctype->instructions.table != NULL);

  uint32_t union_offs = (uint32_t) -1;
  for (uint32_t n = 0; n < ctype->instructions.count; n++)
  {
    const struct instruction *inst = &ctype->instructions.table[n];
    if (inst->type == OPCODE &&
        DDS_OP (inst->data.opcode.code) == DDS_OP_ADR &&
        DDS_OP_TYPE (inst->data.opcode.code) == DDS_OP_VAL_UNI)
    {
      union_offs = n;
      break;
    }
  }
  CU_ASSERT_NEQ_FATAL (union_offs, (uint32_t) -1);
  CU_ASSERT_EQ_FATAL (ctype->instructions.table[union_offs + 2].type, SINGLE);
  CU_ASSERT_EQ_FATAL (ctype->instructions.table[union_offs + 2].data.single, nlabels);
  CU_ASSERT_EQ_FATAL (ctype->instructions.table[union_offs + 3].type, COUPLE);

  uint32_t jeq_offs = union_offs + ctype->instructions.table[union_offs + 3].data.couple.low;
  for (uint32_t n = 0; n < nlabels; n++)
  {
    CU_ASSERT_LT_FATAL (jeq_offs + 3, ctype->instructions.count);
    const struct instruction *jeq = &ctype->instructions.table[jeq_offs];
    CU_ASSERT_FATAL ((jeq->type == OPCODE && DDS_OP (jeq->data.opcode.code) == DDS_OP_JEQ4) || jeq->type == JEQ_OFFSET);
    CU_ASSERT_EQ_FATAL (ctype->instructions.table[jeq_offs + 1].type, CONSTANT);
    const char *value = ctype->instructions.table[jeq_offs + 1].data.constant.value;
    if (labels[n])
      CU_ASSERT_STREQ_FATAL (value, labels[n]);
    else
      CU_ASSERT_EQ_FATAL (value, NULL);
    jeq_offs += 4;
  }

  descriptor_fini (&descriptor);
  idl_delete_pstate (pstate);
}

CU_Test(idlc_descriptor, union_default_case_last)
{
  static const char * const default_first[] = { "1", "2", NULL };
  assert_union_case_labels (
      "@topic union test switch(long) { "
      "default: long d; "
      "case 1: long a; "
      "case 2: long b; "
      "};",
      3, default_first);

  static const char * const default_with_explicit_label[] = { "10", "1", NULL };
  assert_union_case_labels (
      "@topic union test switch(long) { "
      "case 10: default: sequence<long> d; "
      "case 1: long a; "
      "};",
      3, default_with_explicit_label);
}

CU_Test(idlc_descriptor, key_valid_types)
{
  static const struct {
    bool valid;
    const char *idl;
  } tests[] = {
    { true, "@topic struct test { @key boolean a; @key boolean b[3]; }; " },
    { true, "@topic struct test { @key char a; @key octet b; @key char c[3]; }; " },
    { true, "@topic struct test { @key short a; @key unsigned short b; @key short c[3]; }; " },
    { true, "@topic struct test { @key long a; @key unsigned long b; @key long c[3]; }; " },
    { true, "@topic struct test { @key long long a; @key unsigned long long b; @key long long c[3]; }; " },
    { true, "@topic struct test { @key float a; @key double b; @key float c[3]; }; " },
    { true, "enum e { E1, E2 }; @topic struct test { @key e a; @key e b[3]; }; " },
    { true, "bitmask bm { BM1, BM2 }; @topic struct test { @key bm a; @key bm b[3]; }; " },
    { true, "@topic struct test { @key string a; @key string<5> b; @key string c[3]; }; " },
    { true, "@topic struct test { @key string<5> a[2]; }; " },
    { true, "@topic struct test { @key sequence<long> a; }; " },
    { true, "@topic struct test { @key sequence<long> a[2]; }; " },
    { true, "@nested struct sub { long a; long b; }; @topic struct test { @key sub a; }; " },
    { true, "@nested struct sub { long a; }; @topic struct test { @key sub a[2]; }; " },
    { true, "@nested struct sub { @key long a; long b; }; @topic struct test { @key sub a; }; " },
    { true, "@nested struct sub { long a; sequence<long> b; }; @topic struct test { @key sub a; }; " },
    { true, "typedef sequence<long> seqlong; @nested struct sub { seqlong a; sequence<long> b; }; @topic struct test { @key sequence<sub> a; }; " },
    { true, "typedef sequence<long> seqlong; @nested @final struct sub { sequence<seqlong, 8> a[4]; sequence<float> b[5]; }; @topic @final struct test { @key sub a[2][3]; };" },
    { true, "typedef long arrlong[5][6]; @nested @final struct sub { sequence<arrlong, 8> a[4]; arrlong b[5]; }; @topic @final struct test { @key sub a[2][3]; };" },
    { true, "@topic union test switch(@key long) { case 1: long a; }; " },
    { true, "@nested union u switch(long) { case 1: long a; }; @topic struct test { @key u a; }; " },
    { true, "@nested union u switch(long) { case 1: long a; }; @topic struct test { @key sequence<u> a; }; " },
    { true, "@nested union u switch(long) { case 1: long a; }; @topic struct test { @key u a[2]; }; " }
  };

  idl_retcode_t ret;
  uint32_t flags = IDL_FLAG_EXTENDED_DATA_TYPES |
                   IDL_FLAG_ANONYMOUS_TYPES |
                   IDL_FLAG_ANNOTATIONS;
  for (size_t i = 0; i < sizeof (tests) / sizeof (tests[0]); i++) {
    static idl_pstate_t *pstate = NULL;
    struct descriptor descriptor;

    printf ("running test for idl: %s\n", tests[i].idl);
    ret = idl_create_pstate (flags, NULL, &pstate);
    CU_ASSERT_EQ_FATAL (ret, IDL_RETCODE_OK);
    memset (&descriptor, 0, sizeof (descriptor)); /* static analyzer */
    ret = generate_test_descriptor (pstate, tests[i].idl, &descriptor);
    CU_ASSERT_EQ_FATAL (ret, tests[i].valid ? IDL_RETCODE_OK : IDL_RETCODE_UNSUPPORTED);
    if (tests[i].valid)
      descriptor_fini (&descriptor);
    idl_delete_pstate (pstate);
  }
}

CU_Test(idlc_descriptor, enum_bit_bound_one)
{
  idl_pstate_t *pstate = NULL;
  struct descriptor descriptor;
  uint32_t flags = IDL_FLAG_EXTENDED_DATA_TYPES |
                   IDL_FLAG_ANONYMOUS_TYPES |
                   IDL_FLAG_ANNOTATIONS;

  idl_retcode_t ret = idl_create_pstate (flags, NULL, &pstate);
  CU_ASSERT_EQ_FATAL (ret, IDL_RETCODE_OK);
  memset (&descriptor, 0, sizeof (descriptor));
  ret = generate_test_descriptor (pstate, "@bit_bound(1) enum e { @value(-1) E_NEG1, E_0 }; @topic struct test { e f; };", &descriptor);
  CU_ASSERT_EQ_FATAL (ret, IDL_RETCODE_OK);

  bool saw_evs = false;
  for (uint32_t n = 0; n + 4 < descriptor.member_ids.count; n++)
  {
    const struct instruction *inst = &descriptor.member_ids.table[n];
    if (inst->type != OPCODE || DDS_OP (inst->data.opcode.code) != DDS_OP_EVS)
      continue;
    CU_ASSERT_EQ_FATAL (descriptor.member_ids.table[n + 1].type, SINGLE);
    CU_ASSERT_EQ_FATAL (descriptor.member_ids.table[n + 2].type, SINGLE);
    CU_ASSERT_EQ_FATAL (descriptor.member_ids.table[n + 3].type, SINGLE);
    CU_ASSERT_EQ_FATAL (descriptor.member_ids.table[n + 4].type, SINGLE);
    CU_ASSERT_EQ_FATAL (descriptor.member_ids.table[n + 1].data.single, UINT32_MAX);
    CU_ASSERT_EQ_FATAL (descriptor.member_ids.table[n + 2].data.single, 2);
    CU_ASSERT_EQ_FATAL (descriptor.member_ids.table[n + 3].data.single, 0);
    CU_ASSERT_EQ_FATAL (descriptor.member_ids.table[n + 4].data.single, 0xff);
    saw_evs = true;
    break;
  }
  CU_ASSERT_FATAL (saw_evs);
  descriptor_fini (&descriptor);
  idl_delete_pstate (pstate);

  pstate = NULL;
  ret = idl_create_pstate (flags, NULL, &pstate);
  CU_ASSERT_EQ_FATAL (ret, IDL_RETCODE_OK);
  memset (&descriptor, 0, sizeof (descriptor));
  ret = generate_test_descriptor (pstate, "@bit_bound(1) enum e { E_0, E_1 }; @topic struct test { e f; };", &descriptor);
  CU_ASSERT_EQ_FATAL (ret, IDL_RETCODE_OUT_OF_RANGE);
  idl_delete_pstate (pstate);
}

#define TEST_MAX_KEYS 10
CU_Test(idlc_descriptor, keys_inheritance)
{
  static const struct {
    const char *idl;
    uint32_t n_keys;
    const char *key_name[TEST_MAX_KEYS];
  } tests[] = {
    /* no keys */
    { "@nested struct test_base { long a; }; @topic struct test : test_base { long c; };",
      0, { "" } },
    /* single inheritance, one key field */
    { "@nested struct test_base { @key long a; short b; }; @topic struct test : test_base { };",
      1, { "parent.a" } },
    /* two levels of inheritance */
    { "@nested struct test_base2 { @key long a2; }; @nested struct test_base1 : test_base2 { long a1; }; @topic struct test : test_base1 { long a; };",
      1, { "parent.parent.a2" } },
    /* base type has (all members of) struct type test_base2 as key */
    { "@nested struct test_base2 { long a2; long b2; }; @nested struct test_base1 { @key long a1; @key test_base2 b1; }; @topic struct test : test_base1 { long c; };",
      3, { "parent.a1", "parent.b1.a2", "parent.b1.b2" } },
    /* single inheritance, key fields reversed by @id */
    { "@nested struct test_base { @key @id(1) long a; @key @id(0) short b; }; @topic struct test : test_base { @id(2) long c; };",
      2, { "parent.b", "parent.a" } },
    /* single inheritance appendable struct, one key field */
    { "@nested @appendable struct test_base { @key long a; short b; }; @topic @appendable struct test : test_base { long c; };",
      1, { "parent.a" } },
    /* single inheritance mutable struct, one key field */
    { "@nested @mutable struct test_base { @key long a; short b; }; @topic @mutable struct test : test_base { long c; };",
      1, { "a" } },
    /* two levels of inheritance, mutable struct */
    { "@nested @mutable struct test_base2 { @key long a2; @key long b2; }; @nested @mutable struct test_base1 : test_base2 { long a1; }; @topic @mutable struct test : test_base1 { long a; };",
      2, { "a2", "b2" } },
    /* base type has (all members of) struct type test_base2 as key, mutable struct */
    { "@nested @appendable struct test_base2 { long a2; long b2; }; @nested @mutable struct test_base1 { @key long a1; @key test_base2 b1; }; @topic @mutable struct test : test_base1 { long c; };",
      3, { "a1", "b1.a2", "b1.b2" } },
    /* single inheritance, mutable types, key fields reversed by @id */
    { "@nested @mutable struct test_base { @key @id(1) long a; @key @id(0) short b; }; @topic @mutable struct test : test_base { @id(2) long c; };",
      2, { "b", "a" } },
  };

  idl_retcode_t ret;
  uint32_t flags = IDL_FLAG_EXTENDED_DATA_TYPES |
                   IDL_FLAG_ANONYMOUS_TYPES |
                   IDL_FLAG_ANNOTATIONS;
  for (size_t i = 0; i < sizeof (tests) / sizeof (tests[0]); i++) {
    static idl_pstate_t *pstate = NULL;
    struct descriptor descriptor;

    printf ("running test for idl: %s\n", tests[i].idl);

    ret = idl_create_pstate (flags, NULL, &pstate);
    CU_ASSERT_EQ_FATAL (ret, IDL_RETCODE_OK);

    memset (&descriptor, 0, sizeof (descriptor)); /* static analyzer */
    ret = generate_test_descriptor (pstate, tests[i].idl, &descriptor);
    CU_ASSERT_EQ_FATAL (ret, IDL_RETCODE_OK);
    CU_ASSERT_EQ_FATAL (descriptor.n_keys, tests[i].n_keys);

    for (uint32_t k = 0; k < descriptor.n_keys; k++) {
      CU_ASSERT_NEQ_FATAL (descriptor.keys[k].name, NULL);
      CU_ASSERT_STREQ_FATAL (descriptor.keys[k].name, tests[i].key_name[k]);
    }

    descriptor_fini (&descriptor);
    idl_delete_pstate (pstate);
  }
}
#undef TEST_MAX_KEYS
