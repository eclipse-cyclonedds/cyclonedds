/*
 * Copyright(c) 2020 ADLINK Technology Limited and others
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
#include <stdlib.h>
#include <string.h>

#include "idl/processor.h"

#include "CUnit/Test.h"

CU_Test(idl_inheritance, base_struct)
{
  idl_retcode_t ret;
  idl_pstate_t *pstate = NULL;
  idl_struct_t *s1, *s2;

  const char str[] = "struct s1 { char c; }; struct s2 : s1 { octet o; };";
  ret = idl_create_pstate(0u, NULL, &pstate);
  CU_ASSERT_EQUAL_FATAL(ret, IDL_RETCODE_OK);
  CU_ASSERT_PTR_NOT_NULL(pstate);
  assert(pstate);
  ret = idl_parse_string(pstate, str);
  CU_ASSERT_EQUAL(ret, IDL_RETCODE_OK);
  CU_ASSERT_PTR_NOT_NULL_FATAL(pstate->root);
  assert(pstate->root);
  s1 = (idl_struct_t *)pstate->root;
  CU_ASSERT_FATAL(idl_is_struct(s1));
  CU_ASSERT_STRING_EQUAL(idl_identifier(s1), "s1");
  s2 = idl_next(s1);
  CU_ASSERT_FATAL(idl_is_struct(s2));
  CU_ASSERT_STRING_EQUAL(idl_identifier(s2), "s2");
  CU_ASSERT(s2->inherit_spec && s2->inherit_spec->base == s1);
  idl_delete_pstate(pstate);
}

CU_Test(idl_inheritance, empty_structs)
{
  static struct {
    const char *str;
    uint32_t ids;
  } tests[] = {
    { "struct s1 { }; struct s2 : s1 { char c1; }; struct sx : s2 { char c2; };", 2 },
    { "struct s1 { char c1; }; struct s2 : s1 { }; struct s3 : s2 { }; struct sx : s3 { char c2; };", 2 },
    { "struct s1 { }; struct s2 : s1 { char c1; }; struct s3 : s2 { }; struct sx : s3 { char c2; };", 2 },
    { "struct s1 { char c1; }; struct s2 : s1 { char c2; }; struct sx : s2 { };", 2 },
    { "struct sx { };", 0 },
    { "struct s1 { }; struct s2 : s1 { }; struct sx : s2 { };", 0 }
  };

  for (size_t i=0, n=sizeof(tests)/sizeof(tests[0]); i < n; i++) {
    uint32_t fields = 0, ids = tests[i].ids;
    idl_retcode_t ret;
    idl_pstate_t *pstate = NULL;
    ret = idl_create_pstate(0, NULL, &pstate);
    CU_ASSERT_EQUAL(ret, IDL_RETCODE_OK);
    CU_ASSERT_PTR_NOT_NULL_FATAL(pstate);
    assert(pstate);
    ret = idl_parse_string(pstate, tests[i].str);
    CU_ASSERT_EQUAL(ret, IDL_RETCODE_OK);
    CU_ASSERT_PTR_NOT_NULL_FATAL(pstate->root);
    assert(pstate->root);
    const idl_struct_t *s = (idl_struct_t *)pstate->root;
    // search for struct by name
    while (s && !(idl_is_struct(s) && strcmp(idl_identifier(s), "sx") == 0))
      s = idl_next(s);
    // recurse down
    CU_ASSERT_PTR_NOT_NULL(s);
    while (s) {
      for (const idl_member_t *m = s->members; m; m = idl_next(m)) {
        for (const idl_declarator_t *d = m->declarators; d; d = idl_next(d)) {
          if (ids)
            CU_ASSERT_EQUAL(d->id.value, --ids);
          fields++;
        }
      }
      s = s->inherit_spec ? s->inherit_spec->base : NULL;
    }
    CU_ASSERT_EQUAL(ids, 0);
    CU_ASSERT_EQUAL(fields, tests[i].ids);
    idl_delete_pstate(pstate);
  }
}
