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

#include "idl/string.h"
#include "descriptor.h"
#include "plugin.h"

#include "CUnit/Theory.h"

static idl_retcode_t generate_test_descriptor (idl_pstate_t *pstate, const char *idl, struct descriptor *descriptor)
{
  idl_retcode_t ret = idl_parse_string(pstate, idl);
  CU_ASSERT_EQUAL_FATAL (ret, IDL_RETCODE_OK);
  assert (ret == IDL_RETCODE_OK);

  bool topic_found = false;
  for (idl_node_t *node = pstate->root; node; node = idl_next (node))
  {
    if (idl_is_topic (node, (pstate->flags & IDL_FLAG_KEYLIST)))
    {
      ret = generate_descriptor_impl(pstate, node, descriptor);
      CU_ASSERT_EQUAL_FATAL (ret, IDL_RETCODE_OK);
      topic_found = true;
      break;
    }
  }
  CU_ASSERT_FATAL (topic_found);
  CU_ASSERT_PTR_NOT_NULL_FATAL (descriptor);
  assert (descriptor); /* static analyzer */
  return ret;
}

#define TEST_MAX_KEYS 10
#define TEST_MAX_KEY_OFFS 10
CU_Test(idlc_descriptor, keys_nested)
{
  static const struct {
    const char *idl;
    uint32_t n_keys;
    uint32_t n_key_offs; // number of key offset: the sum of (1 + number of nesting levels) for all keys
    bool keylist; // indicates if pragma keylist is used
    uint32_t key_size[TEST_MAX_KEYS]; // key size in bytes
    uint32_t key_order[TEST_MAX_KEYS][TEST_MAX_KEY_OFFS]; // key order (used only when pragma keylist is used)
    const char *key_name[TEST_MAX_KEYS];
    uint32_t key_index[TEST_MAX_KEYS]; // key index as printed in the dds key descriptor, indicates the index (order 0..n) of the key in the CDR
  } tests[] = {
    { "struct test { @key @id(2) long a; short b; }; ",
      1, 2, false, { 4 }, { { 2 } }, { "a" }, { 0 } },
    { "struct test { @key @id(0) long a; @key @id(1) short b; }; ",
      2, 4, false, { 4, 2 }, { { 0 }, { 1 } }, { "a", "b" }, { 0, 1 } },
    { "@nested struct inner { @id(3) long i1; @id(1) short i2; }; struct outer { @key inner o1; }; ",
      2, 6, false, { 2, 4 }, { { 0, 1 }, { 0, 3 } }, { "o1.i2", "o1.i1" }, { 1, 0 } },
    { "@nested struct inner { @id(0) long i1; @key @id(1) short i2; }; struct outer { @key inner o1; }; ",
      1, 3, false, { 2 }, { { 0, 1 } }, { "o1.i2" }, { 0 } },
    { "@nested struct inner { @key @id(5) short i1; }; struct outer { @key @id(0) inner o1; @key @id(10) inner o2; }; ",
      2, 6, false, { 2, 2 }, { { 0, 5 }, { 10, 5 } }, { "o1.i1", "o2.i1" }, { 0, 1 } },
    { "@nested struct inner { @key short i1; }; @nested struct mid { @key @id(3) char m1; @key @id(2) inner m2; @id(1) long m3; }; struct outer { @key @id(0) mid o1; @key @id(1) inner o2; }; ",
      3, 10, false, { 2, 1, 2 }, { { 0, 2, 0 }, { 0, 3 }, { 1, 0 } }, { "o1.m2.i1", "o1.m1", "o2.i1" }, { 1, 0, 2 } },
    { "@nested struct inner { @id(0) char i1; @key @id(1) char i2; }; struct outer { @key @id(3) inner o1; @key @id(2) short o2; }; ",
      2, 5, false, { 2, 1 }, { { 2 }, { 3, 1 } }, { "o2", "o1.i2" }, { 1, 0 } },

    // FIXME: remove @key annotations once sequential auto-id is implemented
    { "struct test { @id(0) long a; @id(1) short b; }; \n#pragma keylist test a",
      1, 2, true, { 4 }, { { 0 } }, { "a" }, { 0 } },
    { "struct test { @id(0) long a; @id(1) short b; }; \n#pragma keylist test a b",
      2, 4, true, { 4, 2 }, { { 0 }, { 1 } }, { "a", "b" }, { 0, 1 } },
    { "struct inner { @id(0) long i1; @id(1) short i2; }; struct outer { @id(0) inner o1; @id(1) inner o2; }; \n#pragma keylist outer o1.i1",
      1, 3, true, { 4 }, { { 0, 0 } }, { "o1.i1" }, { 0 } },
    { "struct inner { @id(0) long i1; @id(1) short i2; }; struct outer { @id(0) inner o1; @id(1) inner o2; }; \n#pragma keylist outer o1.i1 o2.i1",
      2, 6, true, { 4, 4 }, { { 0, 0 }, { 1, 0 } }, { "o1.i1", "o2.i1" }, { 0, 1 } },
    { "struct inner { @id(0) long i1; @id(1) long i2; }; struct mid { @id(0) inner m1; }; struct outer { inner o1, o2; @id(2) inner o3[3]; @id(3) mid o4; @id(4) double o5; }; \n#pragma keylist outer o4.m1.i2",
      1, 4, true, { 4 }, { { 3, 0, 1 } }, { "o4.m1.i2" }, { 0 } },

    // type 'outer' should not get keys of other types using the same type 'inner' */
    { "struct inner { @id(0) long i1; @id(1) short i2; }; struct outer { @id(0) inner o1; @id(1) inner o2; }; \n"
      "#pragma keylist outer o1.i1 \n "
      "struct p { inner p1; }; \n"
      "#pragma keylist p p1.i1 \n",
      1, 3, true, { 4 }, { { 0, 0 } }, { "o1.i1" }, { 0 } },
    { "struct inner { @id(0) long i1; @id(1) short i2; }; struct outer { @id(0) inner o1; @id(1) inner o2; }; \n"
      "#pragma keylist outer \n"
      "struct p { inner p1; }; \n"
      "#pragma keylist p p1.i1 \n",
      0, 0, true, { 0 }, { { 0 } }, { "" }, { 0 } },

    // key fields ordered by member id, not by order used in keylist
    { "struct inner { @id(0) long long i1; }; struct outer { @id(0) inner o1; @id(1) inner o2; }; \n#pragma keylist outer o2.i1 o1.i1",
      2, 6, true, { 8, 8 }, { { 0, 0 }, { 1, 0 } }, { "o1.i1", "o2.i1" }, { 0, 1 } },
    { "struct inner { @id(0) char i1; }; struct mid { @id(0) short m1; @id(1) inner m2; @id(2) long m3; }; struct outer { @id(0) mid o1; @id(1) inner o2; }; \n#pragma keylist outer o1.m1 o2.i1 o1.m2.i1",
      3, 10, true, { 2, 1, 1 }, { { 0, 0 }, { 0, 1, 0 }, { 1, 0 } }, { "o1.m1", "o1.m2.i1", "o2.i1" }, { 0, 1, 2 } },
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

    uint32_t sz = 0;
    struct key_print_meta *keys = key_print_meta_init (&descriptor, &sz);
    for (uint32_t k = 0; k < descriptor.n_keys; k++) {
      CU_ASSERT_EQUAL_FATAL (keys[k].size, tests[i].key_size[k]);
      for (uint32_t j = 0; j < keys[k].n_order; j++)
        CU_ASSERT_EQUAL_FATAL (keys[k].order[j], tests[i].key_order[k][j]);
      CU_ASSERT_PTR_NOT_NULL_FATAL (keys[k].name);
      assert (keys[k].name && tests[i].key_name[k]);
      CU_ASSERT_STRING_EQUAL_FATAL (keys[k].name, tests[i].key_name[k]);
      CU_ASSERT_EQUAL_FATAL (keys[k].key_idx, tests[i].key_index[k]);
    }

    ret = descriptor_fini (&descriptor);
    CU_ASSERT_EQUAL_FATAL (ret, IDL_RETCODE_OK);

    idl_delete_pstate (pstate);
    key_print_meta_free (keys, descriptor.n_keys);
  }
}
