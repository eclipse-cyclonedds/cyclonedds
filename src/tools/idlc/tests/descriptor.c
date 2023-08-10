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
    CU_ASSERT_EQUAL_FATAL (ret, IDL_RETCODE_OK);

    memset (&descriptor, 0, sizeof (descriptor)); /* static analyzer */
    ret = generate_test_descriptor (pstate, tests[i].idl, &descriptor);
    CU_ASSERT_EQUAL_FATAL (ret, IDL_RETCODE_OK);

    CU_ASSERT_EQUAL_FATAL (descriptor.n_keys, tests[i].n_keys);
    CU_ASSERT_EQUAL_FATAL (descriptor.key_offsets.count, tests[i].n_key_offs);
    CU_ASSERT_EQUAL_FATAL (pstate->keylists, tests[i].keylist);

    for (uint32_t k = 0; k < descriptor.n_keys; k++) {
      for (uint32_t j = 0; j < descriptor.keys[k].n_order; j++)
        CU_ASSERT_EQUAL_FATAL (descriptor.keys[k].order[j], tests[i].key_order[k][j]);
      CU_ASSERT_PTR_NOT_NULL_FATAL (descriptor.keys[k].name);
      assert (descriptor.keys[k].name && tests[i].key_name[k]);
      CU_ASSERT_STRING_EQUAL_FATAL (descriptor.keys[k].name, tests[i].key_name[k]);
      CU_ASSERT_EQUAL_FATAL (descriptor.keys[k].key_idx, tests[i].key_index[k]);
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
    { S("@appendable"), IDL_MUTABLE, IDL_APPENDABLE },
    { S("@extensibility(MUTABLE)"), IDL_APPENDABLE, IDL_MUTABLE },
    { U("@appendable"), IDL_FINAL, IDL_APPENDABLE },
    { U("@extensibility(APPENDABLE)"), IDL_MUTABLE, IDL_APPENDABLE },
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
    CU_ASSERT_EQUAL_FATAL (ret, IDL_RETCODE_OK);
    memset (&descriptor, 0, sizeof (descriptor)); /* static analyzer */
    ret = idl_parse_string(pstate, tests[i].idl);
    CU_ASSERT_EQUAL_FATAL (ret, IDL_RETCODE_OK);
    CU_ASSERT_PTR_NOT_NULL_FATAL (pstate->root);
    ret = generate_descriptor_impl(pstate, pstate->root, &descriptor);
    CU_ASSERT_EQUAL_FATAL (ret, IDL_RETCODE_OK);

    uint32_t instr1 = 0;
    assert (descriptor.constructed_types);
    assert (descriptor.constructed_types->instructions.table);
    if (descriptor.constructed_types->instructions.table[0].type == OPCODE)
      instr1 = descriptor.constructed_types->instructions.table[0].data.opcode.code;
    switch (tests[i].exp_ext) {
      case IDL_FINAL:
        CU_ASSERT_FATAL(instr1 != DDS_OP_DLC && instr1 != DDS_OP_PLC);
        break;
      case IDL_APPENDABLE:
        CU_ASSERT_FATAL(instr1 == DDS_OP_DLC);
        break;
      case IDL_MUTABLE:
        CU_ASSERT_FATAL(instr1 == DDS_OP_PLC);
        break;
    }
    descriptor_fini (&descriptor);
    CU_ASSERT_EQUAL_FATAL (ret, IDL_RETCODE_OK);
    idl_delete_pstate (pstate);
  }
}

#undef S
#undef U

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
    { false, "@topic struct test { @key string a; @key string<5> b; @key string c[3]; }; " },
    { false, "@topic struct test { @key string<5> a[2]; }; " },
    { false, "@topic struct test { @key sequence<long> a; }; " },
    { false, "@topic struct test { @key sequence<long> a[2]; }; " },
    { true, "@nested struct sub { long a; long b; }; @topic struct test { @key sub a; }; " },
    { false, "@nested struct sub { long a; }; @topic struct test { @key sub a[2]; }; " },
    { true, "@nested struct sub { @key long a; long b; }; @topic struct test { @key sub a; }; " },
    { false, "@nested struct sub { long a; sequence<long> b; }; @topic struct test { @key sub a; }; " },
    { false, "@nested union u switch(long) { case 1: long a; }; @topic struct test { @key u a; }; " }
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
    CU_ASSERT_EQUAL_FATAL (ret, IDL_RETCODE_OK);
    memset (&descriptor, 0, sizeof (descriptor)); /* static analyzer */
    ret = generate_test_descriptor (pstate, tests[i].idl, &descriptor);
    CU_ASSERT_EQUAL_FATAL (ret, tests[i].valid ? IDL_RETCODE_OK : IDL_RETCODE_UNSUPPORTED);
    if (tests[i].valid)
      descriptor_fini (&descriptor);
    idl_delete_pstate (pstate);
  }
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
    CU_ASSERT_EQUAL_FATAL (ret, IDL_RETCODE_OK);

    memset (&descriptor, 0, sizeof (descriptor)); /* static analyzer */
    ret = generate_test_descriptor (pstate, tests[i].idl, &descriptor);
    CU_ASSERT_EQUAL_FATAL (ret, IDL_RETCODE_OK);
    CU_ASSERT_EQUAL_FATAL (descriptor.n_keys, tests[i].n_keys);

    for (uint32_t k = 0; k < descriptor.n_keys; k++) {
      CU_ASSERT_PTR_NOT_NULL_FATAL (descriptor.keys[k].name);
      assert (descriptor.keys[k].name && tests[i].key_name[k]);
      CU_ASSERT_STRING_EQUAL_FATAL (descriptor.keys[k].name, tests[i].key_name[k]);
    }

    descriptor_fini (&descriptor);
    idl_delete_pstate (pstate);
  }
}
#undef TEST_MAX_KEYS

