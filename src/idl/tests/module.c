// Copyright(c) 2021 ZettaScale Technology and others
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

CU_Test(idl_module, reopen)
{
  idl_retcode_t ret;
  idl_pstate_t* pstate = NULL;

  const char str[] =
  "module m1 {\n"\
  "  struct s1 {\n"\
  "    long l;\n"\
  "  };\n"\
  "};\n"\
  "module m1 {\n"\
  "  struct s2 {\n"\
  "    m1::s1 m_s1;\n"\
  "  };\n"\
  "};\n";

  ret = idl_create_pstate(0u, NULL, &pstate);
  CU_ASSERT_EQUAL_FATAL(ret, IDL_RETCODE_OK);
  CU_ASSERT_PTR_NOT_NULL(pstate);
  ret = idl_parse_string(pstate, str);
  CU_ASSERT_EQUAL_FATAL(ret, IDL_RETCODE_OK);

  idl_module_t* m1 = (idl_module_t*)pstate->root;
  CU_ASSERT_FATAL(idl_is_module(m1));
  assert(m1);
  CU_ASSERT_STRING_EQUAL(m1->name->identifier, "m1");

  idl_struct_t *s1 = (idl_struct_t*)m1->definitions;
  CU_ASSERT_FATAL(idl_is_struct(s1));
  CU_ASSERT_PTR_EQUAL(s1->node.parent, m1);
  CU_ASSERT_PTR_NULL_FATAL(s1->inherit_spec);

  idl_member_t* mem1 = s1->members;
  CU_ASSERT_PTR_NOT_NULL_FATAL(mem1);
  assert(mem1);
  CU_ASSERT_PTR_EQUAL(s1,mem1->node.parent);
  CU_ASSERT((idl_mask(mem1->type_spec) & IDL_LONG) == IDL_LONG);
  CU_ASSERT(!mem1->key.value);

  idl_declarator_t* decl1 = mem1->declarators;
  CU_ASSERT_PTR_NOT_NULL_FATAL(decl1);
  assert(decl1);
  CU_ASSERT_PTR_NULL(decl1->node.next);
  CU_ASSERT_STRING_EQUAL(decl1->name->identifier, "l");
  CU_ASSERT_EQUAL(idl_array_size(decl1), 0);

  idl_module_t* m2 = (idl_module_t*)m1->node.next;
  CU_ASSERT_FATAL(idl_is_module(m2));
  assert(m2);
  CU_ASSERT_STRING_EQUAL(m2->name->identifier, "m1");
#if 0
  /* see comment in idl_create_module in tree.c */
  CU_ASSERT_PTR_EQUAL(m2->previous, m1);
#endif

  idl_struct_t* s2 = (idl_struct_t*)m2->definitions;
  CU_ASSERT_FATAL(idl_is_struct(s2));
  CU_ASSERT_PTR_EQUAL(s2->node.parent, m2);
  CU_ASSERT_PTR_NULL_FATAL(s2->inherit_spec);

  idl_member_t* mem2 = s2->members;
  CU_ASSERT_PTR_NOT_NULL_FATAL(mem2);
  assert(mem2);
  CU_ASSERT_PTR_EQUAL(s2, mem2->node.parent);
  CU_ASSERT_PTR_EQUAL(mem2->type_spec, s1);
  CU_ASSERT(!mem2->key.value);

  idl_declarator_t* decl2 = mem2->declarators;
  CU_ASSERT_PTR_NOT_NULL_FATAL(decl2);
  assert(decl2);
  CU_ASSERT_PTR_NULL(decl2->node.next);
  CU_ASSERT_STRING_EQUAL(decl2->name->identifier, "m_s1");
  CU_ASSERT_EQUAL(idl_array_size(decl2), 0);

  idl_delete_pstate(pstate);
}
