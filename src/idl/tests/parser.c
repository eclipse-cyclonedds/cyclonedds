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

#include "CUnit/Theory.h"

#define T(type) "struct s{" type " c;};"

static void
test_base_type(const char *str, uint32_t flags, int32_t retcode, idl_mask_t mask)
{
  idl_retcode_t ret;
  idl_pstate_t *pstate = NULL;
  idl_node_t *node;

  ret = idl_create_pstate(flags, NULL, &pstate);
  CU_ASSERT_EQUAL_FATAL(ret, IDL_RETCODE_OK);
  CU_ASSERT_PTR_NOT_NULL_FATAL(pstate);
  ret = idl_parse_string(pstate, str);
  CU_ASSERT(ret == retcode);
  if (ret != IDL_RETCODE_OK)
    goto bail;
  node = pstate->root;
  CU_ASSERT_PTR_NOT_NULL(node);
  if (!node)
    goto bail;
  CU_ASSERT_EQUAL(idl_mask(node), IDL_STRUCT);
  if (idl_mask(node) == (IDL_DECLARATION | IDL_TYPE | IDL_STRUCT)) {
    idl_member_t *member = ((idl_struct_t *)node)->members;
    CU_ASSERT_PTR_NOT_NULL(member);
    if (!member)
      goto bail;
    CU_ASSERT_EQUAL(idl_mask(member), IDL_DECLARATION | IDL_MEMBER);
    CU_ASSERT_PTR_NOT_NULL(member->type_spec);
    if (!member->type_spec)
      goto bail;
    CU_ASSERT_EQUAL(idl_mask(member->type_spec), IDL_TYPE | mask);
    CU_ASSERT_PTR_NOT_NULL(member->declarators);
    if (!member->declarators)
      goto bail;
    CU_ASSERT_PTR_NOT_NULL(member->declarators->name);
    CU_ASSERT_PTR_NOT_NULL(member->declarators->name->identifier);
    if (!member->declarators->name || !member->declarators->name->identifier)
      goto bail;
    CU_ASSERT_STRING_EQUAL(member->declarators->name->identifier, "c");
  }

bail:
  idl_delete_pstate(pstate);
}

CU_TheoryDataPoints(idl_parser, base_types) = {
  CU_DataPoints(const char *,
    T("short"), T("unsigned short"),
    T("long"), T("unsigned long"),
    T("long long"), T("unsigned long long"),
    T("float"), T("double"), T("long double"),
    T("char"), T("wchar"),
    T("boolean"), T("octet")),
  CU_DataPoints(uint32_t,
    IDL_SHORT, IDL_USHORT,
    IDL_LONG, IDL_ULONG,
    IDL_LLONG, IDL_ULLONG,
    IDL_FLOAT, IDL_DOUBLE, IDL_LDOUBLE,
    IDL_CHAR, IDL_WCHAR,
    IDL_BOOL, IDL_OCTET)
};

CU_Theory((const char *s, uint32_t t), idl_parser, base_types)
{
  test_base_type(s, IDL_FLAG_EXTENDED_DATA_TYPES, 0, t);
}

CU_TheoryDataPoints(idl_parser, extended_base_types) = {
  CU_DataPoints(const char *, T("int8"), T("uint8"),
                              T("int16"), T("uint16"),
                              T("int32"), T("uint32"),
                              T("int64"), T("uint64")),
  CU_DataPoints(uint32_t, IDL_INT8, IDL_UINT8,
                          IDL_INT16, IDL_UINT16,
                          IDL_INT32, IDL_UINT32,
                          IDL_INT64, IDL_UINT64)
};

CU_Theory((const char *s, uint32_t t), idl_parser, extended_base_types)
{
  test_base_type(s, IDL_FLAG_EXTENDED_DATA_TYPES, 0, t);
  test_base_type(s, 0u, IDL_RETCODE_SEMANTIC_ERROR, 0);
}

#define M(name, contents) "module " name " { " contents " };"
#define S(name, contents) "struct " name " { " contents " };"
#define LL(name) "long long " name ";"
#define LD(name) "long double " name ";"

CU_Test(idl_parser, embedded_module)
{
  idl_retcode_t ret;
  idl_pstate_t *pstate = NULL;
  idl_node_t *p;
  idl_module_t *m;
  idl_struct_t *s;
  idl_member_t *sm;
  const char str[] = M("foo", M("bar", S("baz", LL("foobar") LD("foobaz"))));

  ret = idl_create_pstate(0u, NULL, &pstate);
  CU_ASSERT_EQUAL_FATAL(ret, IDL_RETCODE_OK);
  CU_ASSERT_PTR_NOT_NULL_FATAL(pstate);
  ret = idl_parse_string(pstate, str);
  CU_ASSERT_EQUAL_FATAL(ret, IDL_RETCODE_OK);
  m = (idl_module_t*)pstate->root;
  CU_ASSERT_PTR_NOT_NULL_FATAL(m);
  CU_ASSERT_PTR_NULL(idl_parent(m));
  //CU_ASSERT_PTR_NULL(idl_previous(m));
  CU_ASSERT_PTR_NULL(idl_next(m));
  CU_ASSERT_FATAL(idl_is_module(m));
  CU_ASSERT_STRING_EQUAL(idl_identifier(m), "foo");
  p = (idl_node_t*)m;
  m = (idl_module_t *)m->definitions;
  CU_ASSERT_PTR_NOT_NULL_FATAL(m);
  CU_ASSERT_PTR_EQUAL(idl_parent(m), p);
  CU_ASSERT_PTR_NULL(idl_previous(m));
  CU_ASSERT_PTR_NULL(idl_next(m));
  CU_ASSERT_FATAL(idl_is_module(m));
  CU_ASSERT_STRING_EQUAL(idl_identifier(m), "bar");
  p = (idl_node_t*)m;
  s = (idl_struct_t *)m->definitions;
  CU_ASSERT_PTR_NOT_NULL_FATAL(s);
  CU_ASSERT_PTR_EQUAL(idl_parent(s), p);
  CU_ASSERT_PTR_NULL(idl_previous(s));
  CU_ASSERT_PTR_NULL(idl_next(s));
  CU_ASSERT_FATAL(idl_is_struct(s));
  CU_ASSERT_STRING_EQUAL(idl_identifier(s), "baz");
  p = (idl_node_t*)s;
  sm = s->members;
  CU_ASSERT_PTR_NOT_NULL_FATAL(sm);
  CU_ASSERT_PTR_EQUAL(idl_parent(sm), p);
  CU_ASSERT_PTR_NULL(idl_previous(sm));
  CU_ASSERT_PTR_NOT_NULL_FATAL(idl_next(sm));
  CU_ASSERT_FATAL(idl_is_member(sm));
  CU_ASSERT(idl_type(sm->type_spec) == IDL_LLONG);
  CU_ASSERT(idl_is_declarator(sm->declarators));
  CU_ASSERT_STRING_EQUAL(idl_identifier(sm->declarators), "foobar");
  CU_ASSERT_PTR_EQUAL(sm, idl_previous(idl_next(sm)));
  sm = idl_next(sm);
  CU_ASSERT_PTR_EQUAL(idl_parent(sm), p);
  CU_ASSERT_PTR_NULL(idl_next(sm));
  CU_ASSERT_FATAL(idl_is_member(sm));
  CU_ASSERT(idl_type(sm->type_spec) == IDL_LDOUBLE);
  CU_ASSERT(idl_is_declarator(sm->declarators));
  CU_ASSERT_STRING_EQUAL(idl_identifier(sm->declarators), "foobaz");
  idl_delete_pstate(pstate);
}

// x. use already existing name

CU_Test(idl_parser, struct_in_struct_same_module)
{
  idl_retcode_t ret;
  idl_pstate_t *pstate = NULL;
  idl_module_t *m;
  idl_struct_t *s1, *s2;
  idl_member_t *s;
  const char str[] = "module m { struct s1 { char c; }; struct s2 { s1 s; }; };";

  ret = idl_create_pstate(0u, NULL, &pstate);
  CU_ASSERT_EQUAL_FATAL(ret, IDL_RETCODE_OK);
  CU_ASSERT_PTR_NOT_NULL_FATAL(pstate);
  ret = idl_parse_string(pstate, str);
  CU_ASSERT_EQUAL_FATAL(ret, IDL_RETCODE_OK);
  m = (idl_module_t *)pstate->root;
  CU_ASSERT_FATAL(idl_is_module(m));
  s1 = (idl_struct_t *)m->definitions;
  CU_ASSERT_FATAL(idl_is_struct(s1));
  s2 = idl_next(s1);
  CU_ASSERT_FATAL(idl_is_struct(s2));
  s = s2->members;
  CU_ASSERT_PTR_EQUAL(s->type_spec, s1);
  idl_delete_pstate(pstate);
}

CU_Test(idl_parser, struct_in_struct_other_module)
{
  idl_retcode_t ret;
  idl_pstate_t *pstate = NULL;
  idl_module_t *m1, *m2;
  idl_struct_t *s1, *s2;
  idl_member_t *s;
  const char str[] = "module m1 { struct s1 { char c; }; }; "
                     "module m2 { struct s2 { m1::s1 s; }; };";

  ret = idl_create_pstate(0u, NULL, &pstate);
  CU_ASSERT_EQUAL_FATAL(ret, IDL_RETCODE_OK);
  CU_ASSERT_PTR_NOT_NULL(pstate);
  ret = idl_parse_string(pstate, str);
  CU_ASSERT_EQUAL_FATAL(ret, IDL_RETCODE_OK);
  m1 = (idl_module_t *)pstate->root;
  CU_ASSERT_FATAL(idl_is_module(m1));
  s1 = (idl_struct_t *)m1->definitions;
  CU_ASSERT_FATAL(idl_is_struct(s1));
  CU_ASSERT_PTR_EQUAL(s1->node.parent, m1);
  m2 = idl_next(m1);
  CU_ASSERT_FATAL(idl_is_module(m2));
  s2 = (idl_struct_t *)m2->definitions;
  CU_ASSERT_FATAL(idl_is_struct(s2));
  s = s2->members;
  CU_ASSERT_PTR_EQUAL(s->type_spec, s1);
  CU_ASSERT_PTR_EQUAL(s2->node.parent, m2);
  idl_delete_pstate(pstate);
}

// x. use nonexisting type!
// x. union with same declarators
// x. struct with same declarators
// x. struct with embedded struct
// x. struct with anonymous embedded struct
// x. forward declared union
//   x.x. forward declared union before definition
//   x.x. forward declared union after definition
//   x.x. forward declared union with no definition at all
// x. forward declared struct
//   x.x. see union
// x. constant expressions
// x. identifier that collides with a keyword
