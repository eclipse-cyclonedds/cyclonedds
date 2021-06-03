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
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "idl/processor.h"

#include "CUnit/Test.h"

static idl_retcode_t parse_string(const char *str, idl_pstate_t **pstatep)
{
  idl_retcode_t ret;
  idl_pstate_t *pstate = NULL;
  if ((ret = idl_create_pstate(0u, NULL, &pstate)))
    return ret;
  assert(pstate);
  ret = idl_parse_string(pstate, str);
  if (ret == IDL_RETCODE_OK)
    *pstatep = pstate;
  else
    idl_delete_pstate(pstate);
  return ret;
}

static void test_bad_use(const char *str)
{
  idl_retcode_t ret;
  idl_pstate_t *pstate = NULL;

  ret = parse_string(str, &pstate);
  CU_ASSERT_EQUAL(ret, IDL_RETCODE_SEMANTIC_ERROR);
  CU_ASSERT_PTR_NULL(pstate);
  idl_delete_pstate(pstate);
}

CU_Test(idl_pragma, keylist_bad_use)
{
  const char *str[] = {
    /* non-existent type */
    "#pragma keylist foo bar",
    /* non-existent member */
    "struct s { char c; };\n"
    "#pragma keylist s foo\n",
    /* duplicate keylists */
    "struct s { char c; };\n"
    "#pragma keylist s c\n"
    "#pragma keylist s c\n",
    /* duplicate keys */
    "struct s { char c; };\n"
    "#pragma keylist s c c\n",
    NULL
  };

  for (const char **ptr = str; *ptr; ptr++)
    test_bad_use(*ptr);
}

static void test_keylist(const char *str, idl_retcode_t exp)
{
  idl_retcode_t ret;
  idl_pstate_t *pstate = NULL;

  ret = parse_string(str, &pstate);
  CU_ASSERT_EQUAL(ret, exp);
  if (ret == IDL_RETCODE_OK) {
    CU_ASSERT_PTR_NOT_NULL_FATAL(pstate);
    CU_ASSERT_PTR_NOT_NULL_FATAL(pstate->root);
    const void *node = pstate->root;
    const idl_keylist_t *keylist = NULL;
    while (idl_is_enum(node))
      node = idl_next(node);
    CU_ASSERT(idl_is_struct(node));
    if (node && idl_is_struct(node))
      keylist = ((const idl_struct_t *)node)->keylist;
    CU_ASSERT_PTR_NOT_NULL(keylist);
  }
  idl_delete_pstate(pstate);
}

CU_Test(idl_pragma, keylist)
{
#define TEST(type) "struct s { " type " m %s; };\n" \
                   "#pragma keylist s m\n"

  const char *fmts[] = {
    /* base types */
    TEST("boolean"),
    TEST("char"),
    TEST("octet"),
    TEST("short"),
    TEST("unsigned short"),
    TEST("long"),
    TEST("unsigned long"),
    TEST("long long"),
    TEST("unsigned long long"),
    /* enums */
    "enum abc { A, B };\n" TEST("abc"),
    /* strings */
    TEST("string")
  };
#undef TEST

  for (size_t i=0, n=(sizeof(fmts)/sizeof(fmts[0])); i < n; i++) {
    char buf[128];

    /* type */
    (void)snprintf(buf, sizeof(buf), fmts[i], "");
    test_keylist(buf, IDL_RETCODE_OK);
    if (i == n - 1) /* skip array test for strings */
      continue;
    /* array of type */
    (void)snprintf(buf, sizeof(buf), fmts[i], "[2]");
    test_keylist(buf, IDL_RETCODE_OK);
  }
}

CU_Test(idl_pragma, keylist_nested_key)
{
  idl_retcode_t ret;
  idl_pstate_t *pstate = NULL;
  idl_struct_t *s1;
  static const char str[] = "module m1 { struct s2 { char c; }; };\n"
                            "struct s1 { m1::s2 s; };\n"
                            "#pragma keylist s1 s.c";

  ret = parse_string(str, &pstate);
  CU_ASSERT_EQUAL_FATAL(ret, IDL_RETCODE_OK);
  assert(pstate);
  s1 = idl_next(pstate->root);
  CU_ASSERT_FATAL(idl_is_struct(s1));
  CU_ASSERT((idl_mask(s1->keylist) & IDL_KEYLIST) != 0);
  idl_delete_pstate(pstate);
}

CU_Test(idl_pragma, keylist_scoped_name)
{
  idl_retcode_t ret;
  idl_pstate_t *pstate = NULL;
  idl_module_t *m1;
  idl_struct_t *s1;
  static const char str[] = "module m1 { struct s1 { char c; }; };\n"
                            "#pragma keylist m1::s1 c";

  ret = parse_string(str, &pstate);
  CU_ASSERT_EQUAL_FATAL(ret, IDL_RETCODE_OK);
  assert(pstate);
  m1 = (idl_module_t *)pstate->root;
  CU_ASSERT_FATAL(idl_is_module(m1));
  s1 = (idl_struct_t *)m1->definitions;
  CU_ASSERT_FATAL(idl_is_struct(s1));
  CU_ASSERT((idl_mask(s1->keylist) & IDL_KEYLIST) != 0);
  idl_delete_pstate(pstate);
}

CU_Test(idl_pragma, keylist_outer_scope)
{
  idl_retcode_t ret;
  idl_pstate_t *pstate = NULL;
  idl_struct_t *s1;
  static const char str[] = "struct s1 { char c; };\n"
                            "module m1 {\n"
                            "  struct s2 { char c; };\n"
                            "  #pragma keylist s1 c\n"
                            "};\n";

  ret = parse_string(str, &pstate);
  CU_ASSERT_EQUAL_FATAL(ret, IDL_RETCODE_OK);
  assert(pstate);
  s1 = (idl_struct_t *)pstate->root;
  CU_ASSERT_FATAL(idl_is_struct(s1));
  CU_ASSERT((idl_mask(s1->keylist) & IDL_KEYLIST) != 0);
  idl_delete_pstate(pstate);
}

CU_Test(idl_pragma, unknown)
{
  idl_retcode_t ret;
  idl_pstate_t *pstate = NULL;
  idl_struct_t *s1;
  static const char str[] = "struct s1 { char c; };\n"
                            "#pragma foo \"bar::baz\"";

  ret = parse_string(str, &pstate);
  CU_ASSERT_EQUAL_FATAL(ret, IDL_RETCODE_OK);
  CU_ASSERT_PTR_NOT_NULL_FATAL(pstate);
  assert(pstate);
  s1 = (idl_struct_t *)pstate->root;
  CU_ASSERT_FATAL(idl_is_struct(s1));
  idl_delete_pstate(pstate);
}
