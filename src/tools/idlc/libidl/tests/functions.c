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

#include "idl/processor.h"

#include "CUnit/Theory.h"

CU_Test(functions, array_size)
{
  idl_retcode_t ret;
  idl_pstate_t* pstate = NULL;

  idl_declarator_t decl = {};
  //wrong input parameters
  uint32_t dims = 0;
  CU_ASSERT(!idl_array_size(NULL, NULL));
  CU_ASSERT_EQ(dims, 0);
  CU_ASSERT(!idl_array_size(NULL, &dims));
  CU_ASSERT_EQ(dims, 0);
  CU_ASSERT(!idl_array_size(&decl, NULL));
  CU_ASSERT_EQ(dims, 0);
  CU_ASSERT(!idl_array_size(&decl, &dims));
  CU_ASSERT_EQ(dims, 1);

  //invalid declarator (not declarator)
  CU_ASSERT(!idl_array_size(&decl, &dims));
  CU_ASSERT_EQ(dims, 1);

  //invalid declarator (no literal)
  decl.node.mask |= IDL_DECLARATOR;
  CU_ASSERT(!idl_array_size(&decl, &dims));
  CU_ASSERT_EQ(dims, 1);

  const char str[] =
  "typedef char too_large_array1[1000000][1000000];\n"
  "typedef char too_large_array2[65536][65536];\n"
  "typedef char exact_array[65537][65535];\n"
  "typedef char small_array[12][35];\n"
  "typedef char multidim_array[2][3][5][7];\n";

  ret = idl_create_pstate(0u, NULL, &pstate);
  CU_ASSERT_EQ_FATAL (ret, IDL_RETCODE_OK);
  CU_ASSERT_NEQ (pstate, NULL);
  ret = idl_parse_string(pstate, str);
  CU_ASSERT_EQ_FATAL (ret, IDL_RETCODE_OK);

  const idl_typedef_t *node = (const idl_typedef_t*)pstate->root;
  //too_large_array1
  CU_ASSERT_FATAL(idl_is_typedef(node));
  CU_ASSERT(!idl_array_size(node->declarators, &dims));
  CU_ASSERT_EQ(dims, 1);
  node = idl_next(node);
  CU_ASSERT_NEQ_FATAL (node, NULL);

  //too_large_array2
  CU_ASSERT_FATAL(idl_is_typedef(node));
  CU_ASSERT(!idl_array_size(node, &dims));
  CU_ASSERT_EQ(dims, 1);
  node = idl_next(node);
  CU_ASSERT_NEQ_FATAL (node, NULL);

  //exact_array
  CU_ASSERT_FATAL(idl_is_typedef(node));
  CU_ASSERT(idl_array_size(node->declarators, &dims));
  CU_ASSERT_EQ_FATAL(dims, 65535U*65537U);
  node = idl_next(node);
  CU_ASSERT_NEQ_FATAL (node, NULL);

  //small_array
  CU_ASSERT_FATAL(idl_is_typedef(node));
  CU_ASSERT(idl_array_size(node->declarators, &dims));
  CU_ASSERT_EQ_FATAL(dims, 12U*35U);
  node = idl_next(node);
  CU_ASSERT_NEQ_FATAL (node, NULL);

  //multidim_array + multiplication
  CU_ASSERT_FATAL(idl_is_typedef(node));
  CU_ASSERT(idl_array_size(node->declarators, &dims));
  CU_ASSERT_EQ_FATAL(dims, 2U*3U*5U*7U);

  node = idl_next(node);
  CU_ASSERT_EQ (node, NULL);

  idl_delete_pstate(pstate);
}

CU_Test(functions, multiply_by_array_size)
{
  idl_retcode_t ret;
  idl_pstate_t* pstate = NULL;

  idl_declarator_t decl = {};
  //wrong input parameters
  uint32_t dims = 0;
  CU_ASSERT(!idl_multiply_by_array_size(NULL, NULL));
  CU_ASSERT_EQ(dims, 0);
  CU_ASSERT(!idl_multiply_by_array_size(NULL, &dims));
  CU_ASSERT_EQ(dims, 0);
  CU_ASSERT(!idl_multiply_by_array_size(&decl, NULL));
  CU_ASSERT_EQ(dims, 0);
  CU_ASSERT(!idl_multiply_by_array_size(&decl, &dims));
  CU_ASSERT_EQ(dims, 0);

  //invalid declarator (not declarator)
  dims = 1;
  CU_ASSERT(!idl_multiply_by_array_size(&decl, &dims));
  CU_ASSERT_EQ(dims, 1);

  //invalid declarator (no literal)
  decl.node.mask |= IDL_DECLARATOR;
  CU_ASSERT(!idl_multiply_by_array_size(&decl, &dims));
  CU_ASSERT_EQ(dims, 1);

  const char str[] =
  "typedef char too_large_array1[1000000][1000000];\n"
  "typedef char too_large_array2[65536][65536];\n"
  "typedef char exact_array[65537][65535];\n"
  "typedef char small_array[12][35];\n"
  "typedef char multidim_array[2][3][5][7];\n";

  ret = idl_create_pstate(0u, NULL, &pstate);
  CU_ASSERT_EQ_FATAL (ret, IDL_RETCODE_OK);
  CU_ASSERT_NEQ (pstate, NULL);
  ret = idl_parse_string(pstate, str);
  CU_ASSERT_EQ_FATAL (ret, IDL_RETCODE_OK);

  const idl_typedef_t *node = (const idl_typedef_t*)pstate->root;
  //too_large_array1
  CU_ASSERT_FATAL(idl_is_typedef(node));
  CU_ASSERT(!idl_multiply_by_array_size(node->declarators, &dims));
  CU_ASSERT_EQ(dims, 1);
  node = idl_next(node);
  CU_ASSERT_NEQ_FATAL (node, NULL);

  //too_large_array2
  CU_ASSERT_FATAL(idl_is_typedef(node));
  CU_ASSERT(!idl_multiply_by_array_size(node, &dims));
  CU_ASSERT_EQ(dims, 1);
  node = idl_next(node);
  CU_ASSERT_NEQ_FATAL (node, NULL);

  //exact_array
  CU_ASSERT_FATAL(idl_is_typedef(node));
  CU_ASSERT(idl_multiply_by_array_size(node->declarators, &dims));
  CU_ASSERT_EQ_FATAL(dims, 65535U*65537U);
  node = idl_next(node);
  CU_ASSERT_NEQ_FATAL (node, NULL);
  dims = 1;

  //small_array
  CU_ASSERT_FATAL(idl_is_typedef(node));
  CU_ASSERT(idl_multiply_by_array_size(node->declarators, &dims));
  CU_ASSERT_EQ_FATAL(dims, 12U*35U);
  node = idl_next(node);
  CU_ASSERT_NEQ_FATAL (node, NULL);

  //multidim_array + multiplication
  CU_ASSERT_FATAL(idl_is_typedef(node));
  CU_ASSERT(idl_multiply_by_array_size(node->declarators, &dims));
  CU_ASSERT_EQ_FATAL(dims, 12U*35U*2U*3U*5U*7U);

  node = idl_next(node);
  CU_ASSERT_EQ (node, NULL);

  idl_delete_pstate(pstate);
}
