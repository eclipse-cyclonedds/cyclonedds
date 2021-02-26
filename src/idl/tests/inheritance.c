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
  ret = idl_parse_string(pstate, str);
  CU_ASSERT_EQUAL(ret, IDL_RETCODE_OK);
  CU_ASSERT_PTR_NOT_NULL_FATAL(pstate->root);
  s1 = (idl_struct_t *)pstate->root;
  CU_ASSERT_FATAL(idl_is_struct(s1));
  CU_ASSERT_STRING_EQUAL(idl_identifier(s1), "s1");
  s2 = idl_next(s1);
  CU_ASSERT_FATAL(idl_is_struct(s2));
  CU_ASSERT_STRING_EQUAL(idl_identifier(s2), "s2");
  CU_ASSERT_PTR_NOT_NULL_FATAL(s2->inherit_spec);
  CU_ASSERT_PTR_EQUAL(s2->inherit_spec->base, s1);
  idl_delete_pstate(pstate);
}
