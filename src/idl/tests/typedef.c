// Copyright(c) 2020 to 2021 ZettaScale Technology and others
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

#include "idl/processor.h"

#include "CUnit/Test.h"

CU_Test(idl_typedef, bogus_type)
{
  idl_retcode_t ret;
  idl_pstate_t *pstate = NULL;

  const char str[] = "typedef foo bar;";
  ret = idl_create_pstate(0u, NULL, &pstate);
  CU_ASSERT_EQUAL_FATAL(ret, IDL_RETCODE_OK);
  CU_ASSERT_PTR_NOT_NULL_FATAL(pstate);
  ret = idl_parse_string(pstate, str);
  CU_ASSERT_EQUAL(ret, IDL_RETCODE_SEMANTIC_ERROR);
  idl_delete_pstate(pstate);
}

CU_Test(idl_typedef, simple_declarator)
{
  idl_retcode_t ret;
  idl_pstate_t *pstate = NULL;
  idl_typedef_t *t;
  idl_declarator_t *d;

  const char str[] = "typedef char foo;";
  ret = idl_create_pstate(0u, NULL, &pstate);
  CU_ASSERT_EQUAL_FATAL(ret, IDL_RETCODE_OK);
  CU_ASSERT_PTR_NOT_NULL(pstate);
  assert(pstate);
  ret = idl_parse_string(pstate, str);
  CU_ASSERT_EQUAL_FATAL(ret, IDL_RETCODE_OK);
  t = (idl_typedef_t *)pstate->root;
  CU_ASSERT_PTR_NOT_NULL_FATAL(t);
  CU_ASSERT_FATAL(idl_is_typedef(t));
  assert(t);
  CU_ASSERT_PTR_NULL(idl_next(t));
  CU_ASSERT_PTR_NULL(idl_parent(t));
  CU_ASSERT_PTR_NOT_NULL(t->type_spec);
  CU_ASSERT(idl_type(t->type_spec) == IDL_CHAR);
  d = t->declarators;
  CU_ASSERT_PTR_NOT_NULL_FATAL(d);
  CU_ASSERT_FATAL(idl_is_declarator(d));
  assert(d);
  CU_ASSERT_PTR_NULL(idl_previous(d));
  CU_ASSERT_PTR_NULL(idl_next(d));
  CU_ASSERT_PTR_EQUAL(idl_parent(d), t);
  CU_ASSERT_STRING_EQUAL(idl_identifier(d), "foo");
  CU_ASSERT_PTR_NULL(d->const_expr);
  idl_delete_pstate(pstate);
}

CU_Test(idl_typedef, simple_declarators)
{
  idl_retcode_t ret;
  idl_pstate_t *pstate = NULL;
  idl_typedef_t *t;
  idl_declarator_t *d;

  const char str[] = "typedef char foo, bar, baz;";
  ret = idl_create_pstate(0u, NULL, &pstate);
  CU_ASSERT_EQUAL_FATAL(ret, IDL_RETCODE_OK);
  CU_ASSERT_PTR_NOT_NULL_FATAL(pstate);
  assert(pstate);
  ret = idl_parse_string(pstate, str);
  CU_ASSERT_EQUAL(ret, IDL_RETCODE_OK);
  t = (idl_typedef_t *)pstate->root;
  CU_ASSERT_PTR_NOT_NULL_FATAL(t);
  CU_ASSERT_FATAL(idl_is_typedef(t));
  assert(t);
  CU_ASSERT_PTR_NOT_NULL(t->type_spec);
  CU_ASSERT(idl_type(t->type_spec) == IDL_CHAR);
  d = t->declarators;
  CU_ASSERT_PTR_NOT_NULL_FATAL(d);
  CU_ASSERT_FATAL(idl_is_declarator(d));
  assert(d);
  CU_ASSERT_PTR_NULL(idl_previous(d));
  CU_ASSERT_PTR_EQUAL(idl_parent(d), t);
  CU_ASSERT_PTR_NOT_NULL_FATAL(idl_identifier(d));
  CU_ASSERT_STRING_EQUAL(idl_identifier(d), "foo");
  CU_ASSERT_PTR_NULL(d->const_expr);
  d = idl_next(d);
  CU_ASSERT_PTR_NOT_NULL_FATAL(d);
  CU_ASSERT_FATAL(idl_is_declarator(d));
  assert(d);
  CU_ASSERT_PTR_EQUAL(idl_parent(d), t);
  CU_ASSERT_PTR_NOT_NULL_FATAL(idl_identifier(d));
  CU_ASSERT_STRING_EQUAL(idl_identifier(d), "bar");
  CU_ASSERT_PTR_NULL(d->const_expr);
  d = idl_next(d);
  CU_ASSERT_PTR_NOT_NULL_FATAL(d);
  CU_ASSERT_FATAL(idl_is_declarator(d));
  assert(d);
  CU_ASSERT_PTR_EQUAL(idl_parent(d), t);
  CU_ASSERT_PTR_NOT_NULL_FATAL(idl_identifier(d));
  CU_ASSERT_STRING_EQUAL(idl_identifier(d), "baz");
  CU_ASSERT_PTR_NULL(d->const_expr);
  CU_ASSERT_PTR_NULL(idl_next(d));
  idl_delete_pstate(pstate);
}

// x. typedef with complex declarator
// x. typedef with more than one complex declarator
// x. typedef to typedef

CU_Test(idl_typedef, sequence)
{
  idl_retcode_t ret;
  idl_pstate_t *pstate;
  idl_typedef_t *t;
  idl_struct_t *s;
  idl_member_t *m;

  const char str[] = "typedef sequence<long> t; struct s { t m; };";
  ret = idl_create_pstate(0u, NULL, &pstate);
  CU_ASSERT_EQUAL_FATAL(ret, IDL_RETCODE_OK);
  CU_ASSERT_PTR_NOT_NULL(pstate);
  ret = idl_parse_string(pstate, str);
  CU_ASSERT_EQUAL(ret, IDL_RETCODE_OK);
  CU_ASSERT_PTR_NOT_NULL_FATAL(pstate);
  assert(pstate);
  t = (idl_typedef_t *)pstate->root;
  CU_ASSERT_PTR_NOT_NULL_FATAL(t);
  CU_ASSERT_FATAL(idl_is_typedef(t));
  assert(t);
  s = idl_next(t);
  CU_ASSERT_PTR_NOT_NULL_FATAL(s);
  CU_ASSERT_FATAL(idl_is_struct(s));
  m = s->members;
  CU_ASSERT_PTR_NOT_NULL_FATAL(m);
  CU_ASSERT_FATAL(idl_is_member(m));
  assert(m);
  CU_ASSERT_PTR_EQUAL(m->type_spec, t->declarators);
  idl_delete_pstate(pstate);
}

CU_Test(idl_typedef, typedef_of_typedef_sequence)
{
  idl_retcode_t ret;
  idl_pstate_t *pstate = NULL;
  idl_module_t *m1, *m2;
  idl_typedef_t *t0, *t1, *t2, *t3;
  idl_struct_t *s1;
  idl_sequence_t *s2;
  idl_member_t *m_t2, *m_t3;

  const char str[] =
    "module m1 {\n"\
    "  typedef long t0;\n"
    "  typedef t0 t1;\n"
    "  typedef t1 t2;\n"
    "  typedef sequence<t2> t3;\n"
    "};\n"
    "module m2 {\n"
    "  struct s1 {\n"
    "    m1::t2 m_t2;\n"
    "    sequence<m1::t3> m_t3;\n"
    "  };\n"
    "};\n";

  ret = idl_create_pstate(0u, NULL, &pstate);
  CU_ASSERT_EQUAL_FATAL(ret, IDL_RETCODE_OK);
  CU_ASSERT_PTR_NOT_NULL(pstate);
  ret = idl_parse_string(pstate, str);
  CU_ASSERT_EQUAL_FATAL(ret, IDL_RETCODE_OK);
  CU_ASSERT_PTR_NOT_NULL_FATAL(pstate);
  assert(pstate);
  m1 = (idl_module_t *)pstate->root;
  CU_ASSERT_FATAL(idl_is_module(m1));
  t0 = (idl_typedef_t *)m1->definitions;
  CU_ASSERT_FATAL(idl_is_typedef(t0));
  CU_ASSERT_PTR_EQUAL(t0->node.parent, m1);
  t1 = idl_next(t0);
  CU_ASSERT_FATAL(idl_is_typedef(t1));
  CU_ASSERT_PTR_EQUAL(t1->node.parent, m1);
  CU_ASSERT_PTR_EQUAL(t1->type_spec, t0->declarators);
  t2 = idl_next(t1);
  CU_ASSERT_FATAL(idl_is_typedef(t2));
  CU_ASSERT_PTR_EQUAL(t2->node.parent, m1);
  CU_ASSERT_PTR_EQUAL(t2->type_spec, t1->declarators);
  t3 = idl_next(t2);
  CU_ASSERT_FATAL(idl_is_typedef(t3));
  CU_ASSERT_PTR_EQUAL(t3->node.parent, m1);
  s2 = idl_type_spec(t3);
  CU_ASSERT_FATAL(idl_is_sequence(s2));
  CU_ASSERT(idl_is_alias(s2->type_spec));
  CU_ASSERT_PTR_EQUAL(s2->type_spec, t2->declarators);
  m2 = idl_next(m1);
  CU_ASSERT_FATAL(idl_is_module(m2));
  s1 = (idl_struct_t *)m2->definitions;
  CU_ASSERT_FATAL(idl_is_struct(s1));
  m_t2 = (idl_member_t *)s1->members;
  CU_ASSERT_FATAL(idl_is_member(m_t2));
  CU_ASSERT_PTR_EQUAL(m_t2->type_spec, t2->declarators);
  m_t3 = idl_next(m_t2);
  CU_ASSERT_FATAL(idl_is_member(m_t3));
  CU_ASSERT_FATAL(idl_is_sequence(m_t3->type_spec));
  CU_ASSERT_PTR_EQUAL(((idl_sequence_t *)m_t3->type_spec)->type_spec, t3->declarators);
  idl_delete_pstate(pstate);
}

static idl_retcode_t parse_string(const char *str, idl_pstate_t **pstatep)
{
  idl_retcode_t ret;
  idl_pstate_t *pstate = NULL;

  if ((ret = idl_create_pstate(IDL_FLAG_EXTENDED_DATA_TYPES, NULL, &pstate)))
    return ret;
  if ((ret = idl_parse_string(pstate, str)))
    idl_delete_pstate(pstate);
  else
    *pstatep = pstate;
  return ret;
}

CU_Test(idl_typedef, forward_declaration)
{
  static const struct {
    idl_retcode_t retcode;
    idl_type_t type;
    const char *idl;
  } tests[] = {
    { IDL_RETCODE_SEMANTIC_ERROR, IDL_STRUCT, "typedef struct a b;" },
    { IDL_RETCODE_OK, IDL_STRUCT, "typedef struct a b; struct a { long b; };" },
    { IDL_RETCODE_SEMANTIC_ERROR, IDL_UNION, "typedef union a b;" },
    { IDL_RETCODE_OK, IDL_UNION, "typedef union a b; union a switch (short) { case 1: long b; };" }
  };

  for (size_t i = 0; i < sizeof(tests)/sizeof(tests[0]); i++) {
    idl_retcode_t ret;
    idl_pstate_t *pstate = NULL;
    printf("test idl: %s\n", tests[i].idl);
    ret = parse_string(tests[i].idl, &pstate);
    CU_ASSERT_EQUAL_FATAL(ret, tests[i].retcode);
    if (ret == IDL_RETCODE_OK) {
      const idl_forward_t *forward;
      const idl_typedef_t *alias;
      const idl_type_spec_t *type_spec;
      forward = (const idl_forward_t *)pstate->root;
      CU_ASSERT_FATAL(idl_is_forward(forward));
      alias = idl_next(forward);
      CU_ASSERT_FATAL(idl_is_typedef(alias));
      type_spec = idl_next(alias);
      CU_ASSERT_EQUAL_FATAL(idl_type(type_spec), tests[i].type);
      CU_ASSERT_PTR_EQUAL(alias->type_spec, forward);
      CU_ASSERT_PTR_EQUAL(forward->type_spec, type_spec);
    }
    idl_delete_pstate(pstate);
  }
}

CU_Test(idl_typedef, backwards_forward_declaration)
{
  static const struct {
    idl_retcode_t retcode;
    idl_type_t type;
    const char *idl;
  } tests[] = {
    { IDL_RETCODE_OK, IDL_STRUCT, "struct a { long b; }; typedef struct a b;" },
    { IDL_RETCODE_OK, IDL_UNION, "union a switch (short) { case 1: long b; }; typedef union a b;" }
  };

  for (size_t i = 0; i < sizeof(tests)/sizeof(tests[0]); i++) {
    idl_retcode_t ret;
    idl_pstate_t *pstate = NULL;
    printf("test idl: %s\n", tests[i].idl);
    ret = parse_string(tests[i].idl, &pstate);
    CU_ASSERT_EQUAL_FATAL(ret, tests[i].retcode);
    if (ret == IDL_RETCODE_OK) {
      const idl_forward_t *forward;
      const idl_typedef_t *alias;
      const idl_type_spec_t *type_spec;
      type_spec = (const idl_type_spec_t *)pstate->root;
      CU_ASSERT_EQUAL_FATAL(idl_type(type_spec), tests[i].type);
      forward = idl_next(type_spec);
      CU_ASSERT_FATAL(idl_is_forward(forward));
      alias = idl_next(forward);
      CU_ASSERT_FATAL(idl_is_typedef(alias));
      CU_ASSERT_PTR_EQUAL(alias->type_spec, type_spec);
      CU_ASSERT_PTR_EQUAL(forward->type_spec, type_spec);
    }
    idl_delete_pstate(pstate);
  }
}

CU_Test(idl_typedef, constructed_type)
{
  static const struct {
    idl_retcode_t retcode;
    idl_type_t type;
    const char *idl;
  } tests[] = {
    { IDL_RETCODE_OK, IDL_STRUCT, "typedef struct a { long b; } c;" },
    { IDL_RETCODE_OK, IDL_UNION, "typedef union a switch (short) { case 1: long b; } c;" },
    { IDL_RETCODE_OK, IDL_ENUM, "typedef enum a { b, c } d;" },
    { IDL_RETCODE_OK, IDL_BITMASK, "typedef bitmask a { b } c;" }
  };

  for (size_t i = 0; i < sizeof(tests)/sizeof(tests[0]); i++) {
    idl_retcode_t ret;
    idl_pstate_t *pstate = NULL;
    printf("test idl: %s\n", tests[i].idl);
    ret = parse_string(tests[i].idl, &pstate);
    CU_ASSERT_EQUAL_FATAL(ret, tests[i].retcode);
    if (ret == IDL_RETCODE_OK) {
      const idl_type_spec_t *type_spec;
      const idl_typedef_t *alias;
      type_spec = (const idl_type_spec_t *)pstate->root;
      CU_ASSERT_EQUAL_FATAL(idl_type(type_spec), tests[i].type);
      alias = idl_next(type_spec);
      CU_ASSERT_FATAL(idl_is_typedef(alias));
      CU_ASSERT_PTR_EQUAL(alias->type_spec, type_spec);
    }
    idl_delete_pstate(pstate);
  }
}

CU_Test(idl_typedef, scoped_name)
{
  static const struct {
    idl_retcode_t retcode;
    const char *idl;
  } tests[] = {
    { IDL_RETCODE_OK, "module m1 { struct a { long f1; }; }; typedef m1::a b;" },
    { IDL_RETCODE_OK, "module m1 { module m2 { struct a { long f1; }; }; }; typedef m1::m2::a b;" },
    { IDL_RETCODE_OK, "module m1 { module m2 { struct a { long f1; }; }; typedef m2::a b; };" },
  };

  for (size_t i = 0; i < sizeof(tests)/sizeof(tests[0]); i++) {
    idl_retcode_t ret;
    idl_pstate_t *pstate = NULL;
    printf("test idl: %s\n", tests[i].idl);
    ret = parse_string(tests[i].idl, &pstate);
    CU_ASSERT_EQUAL_FATAL(ret, tests[i].retcode);
    idl_delete_pstate(pstate);
  }
}
