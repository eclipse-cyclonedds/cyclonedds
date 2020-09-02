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

#include "hashid.h"
#include "tree.h"
#include "idl/tree.h"

#include "CUnit/Test.h"

CU_Test(idl_hashid, color)
{
  uint32_t id = idl_hashid("color");
  CU_ASSERT_EQUAL(id, 0x0fa5dd70u);
}

CU_Test(idl_hashid, shapesize)
{
  uint32_t id = idl_hashid("shapesize");
  CU_ASSERT_EQUAL(id, 0x047790dau);
}

CU_Test(idl_annotation, id_member)
{
  idl_retcode_t ret;
  idl_tree_t *tree = NULL;
  idl_struct_t *s;
  idl_member_t *c;
  const char str[] = "struct s { @id(1) char c; };";

  ret = idl_parse_string(str, IDL_FLAG_ANNOTATIONS, &tree);
  CU_ASSERT_EQUAL_FATAL(ret, IDL_RETCODE_OK);
  CU_ASSERT_PTR_NOT_NULL_FATAL(tree);
  s = (idl_struct_t *)tree->root;
  CU_ASSERT_PTR_NOT_NULL_FATAL(s);
  CU_ASSERT_FATAL(idl_is_struct(s));
  c = (idl_member_t *)s->members;
  CU_ASSERT_PTR_NOT_NULL(c);
  CU_ASSERT_FATAL(idl_is_member(c));
  CU_ASSERT_EQUAL(c->id, 1u);
  idl_delete_tree(tree);
}

CU_Test(idl_annotation, id_non_member)
{
  idl_retcode_t ret;
  idl_tree_t *tree = NULL;
  const char str[] = "@id(1) struct s { char c; };";

  ret = idl_parse_string(str, IDL_FLAG_ANNOTATIONS, &tree);
  CU_ASSERT_EQUAL(ret, IDL_RETCODE_SEMANTIC_ERROR);
  idl_delete_tree(tree);
}

CU_Test(idl_annotation, hashid_member)
{
  idl_retcode_t ret;
  idl_tree_t *tree = NULL;
  idl_struct_t *s;
  idl_member_t *m;
  const char str[] = "struct s { @hashid char color; @hashid(\"shapesize\") char size; };";

  ret = idl_parse_string(str, IDL_FLAG_ANNOTATIONS, &tree);
  CU_ASSERT_EQUAL_FATAL(ret, IDL_RETCODE_OK);
  CU_ASSERT_PTR_NOT_NULL(tree);
  s = (idl_struct_t *)tree->root;
  CU_ASSERT_PTR_NOT_NULL_FATAL(s);
  CU_ASSERT_FATAL(idl_is_struct(s));
  m = s->members;
  CU_ASSERT_PTR_NOT_NULL(m);
  CU_ASSERT_FATAL(idl_is_member(m));
  CU_ASSERT_EQUAL(m->id, 0x0fa5dd70u);
  m = idl_next(m);
  CU_ASSERT_PTR_NOT_NULL(m);
  CU_ASSERT_FATAL(idl_is_member(m));
  CU_ASSERT_EQUAL(m->id, 0x047790dau);
  idl_delete_tree(tree);
}

// x. @hashid on non-member
// x. @id without const_expr
// x. both @id and @hashid
// x. @id twice
// x. @hashid twice

CU_Test(idl_annotation, autoid_struct)
{
  idl_retcode_t ret;
  idl_tree_t *tree = NULL;
  idl_struct_t *s1, *s2, *s3;
  const char str[] = "@autoid struct s { char c; }; "
                     "@autoid(HASH) struct s { char c; }; "
                     "@autoid(SEQUENTIAL) struct s { char c; };";

  ret = idl_parse_string(str, IDL_FLAG_ANNOTATIONS, &tree);
  CU_ASSERT_EQUAL_FATAL(ret, IDL_RETCODE_OK);
  CU_ASSERT_PTR_NOT_NULL(tree);
  s1 = (idl_struct_t *)tree->root;
  CU_ASSERT_PTR_NOT_NULL_FATAL(s1);
  CU_ASSERT_FATAL(idl_is_struct(s1));
  CU_ASSERT_EQUAL(s1->autoid, IDL_AUTOID_SEQUENTIAL);
  s2 = idl_next(s1);
  CU_ASSERT_PTR_NOT_NULL_FATAL(s2);
  CU_ASSERT_FATAL(idl_is_struct(s2));
  CU_ASSERT_EQUAL(s2->autoid, IDL_AUTOID_HASH);
  s3 = idl_next(s2);
  CU_ASSERT_PTR_NOT_NULL_FATAL(s3);
  CU_ASSERT_FATAL(idl_is_struct(s3));
  CU_ASSERT_EQUAL(s3->autoid, IDL_AUTOID_SEQUENTIAL);
  idl_delete_tree(tree);
}

// x. autoid twice
// x. autoid (HASH)
// x. autoid (SEQUENTIAL)

CU_Test(idl_annotation, extensibility_struct)
{
  idl_retcode_t ret;
  idl_tree_t *tree = NULL;
  idl_struct_t *s1, *s2, *s3;
  const char str[] = "@extensibility (FINAL) struct s1 { char c; }; "
                     "@extensibility (APPENDABLE) struct s2 { char c; }; "
                     "@extensibility (MUTABLE) struct s3 { char c; };";

  ret = idl_parse_string(str, IDL_FLAG_ANNOTATIONS, &tree);
  CU_ASSERT_EQUAL_FATAL(ret, IDL_RETCODE_OK);
  CU_ASSERT_PTR_NOT_NULL(tree);
  s1 = (idl_struct_t *)tree->root;
  CU_ASSERT_FATAL(idl_is_struct(s1));
  CU_ASSERT_EQUAL(s1->extensibility, IDL_EXTENSIBILITY_FINAL);
  s2 = idl_next(s1);
  CU_ASSERT_FATAL(idl_is_struct(s2));
  CU_ASSERT_EQUAL(s2->extensibility, IDL_EXTENSIBILITY_APPENDABLE);
  s3 = idl_next(s2);
  CU_ASSERT_FATAL(idl_is_struct(s3));
  CU_ASSERT_EQUAL(s3->extensibility, IDL_EXTENSIBILITY_MUTABLE);
  idl_delete_tree(tree);
}

CU_Test(idl_annotation, final_struct)
{
  idl_retcode_t ret;
  idl_tree_t *tree = NULL;
  idl_struct_t *s;
  const char str[] = "@final struct s { char c; };";

  ret = idl_parse_string(str, IDL_FLAG_ANNOTATIONS, &tree);
  CU_ASSERT_EQUAL_FATAL(ret, IDL_RETCODE_OK);
  CU_ASSERT_PTR_NOT_NULL(tree);
  s = (idl_struct_t *)tree->root;
  CU_ASSERT_FATAL(idl_is_struct(s));
  CU_ASSERT_EQUAL(s->extensibility, IDL_EXTENSIBILITY_FINAL);
  idl_delete_tree(tree);
}

CU_Test(idl_annotation, appendable_struct)
{
  idl_retcode_t ret;
  idl_tree_t *tree = NULL;
  idl_struct_t *s;
  const char str[] = "@appendable struct s { char c; };";

  ret = idl_parse_string(str, IDL_FLAG_ANNOTATIONS, &tree);
  CU_ASSERT_EQUAL_FATAL(ret, IDL_RETCODE_OK);
  CU_ASSERT_PTR_NOT_NULL(tree);
  s = (idl_struct_t *)tree->root;
  CU_ASSERT_FATAL(idl_is_struct(s));
  CU_ASSERT_EQUAL(s->extensibility, IDL_EXTENSIBILITY_APPENDABLE);
  idl_delete_tree(tree);
}

CU_Test(idl_annotation, mutable_struct)
{
  idl_retcode_t ret;
  idl_tree_t *tree = NULL;
  idl_struct_t *s;
  const char str[] = "@mutable struct s { char c; };";

  ret = idl_parse_string(str, IDL_FLAG_ANNOTATIONS, &tree);
  CU_ASSERT_EQUAL_FATAL(ret, IDL_RETCODE_OK);
  CU_ASSERT_PTR_NOT_NULL(tree);
  s = (idl_struct_t *)tree->root;
  CU_ASSERT_FATAL(idl_is_struct(s));
  CU_ASSERT_EQUAL(s->extensibility, IDL_EXTENSIBILITY_MUTABLE);
  idl_delete_tree(tree);
}

CU_Test(idl_annotation, foobar_struct)
{
  idl_retcode_t ret;
  idl_tree_t *tree = NULL;
  idl_struct_t *s;
  const char str[] = "@foobar struct s { char c; };";

  ret = idl_parse_string(str, IDL_FLAG_ANNOTATIONS, &tree);
  CU_ASSERT_EQUAL_FATAL(ret, IDL_RETCODE_OK);
  CU_ASSERT_PTR_NOT_NULL(tree);
  s = (idl_struct_t *)tree->root;
  CU_ASSERT_FATAL(idl_is_struct(s));
  idl_delete_tree(tree);
}
