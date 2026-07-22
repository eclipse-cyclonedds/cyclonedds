// Copyright(c) 2026 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include "idl/processor.h"

#include "CUnit/Test.h"

static idl_pstate_t *
parse_string_flags(uint32_t flags, const char *str)
{
  idl_pstate_t *pstate = NULL;
  idl_retcode_t ret;

  ret = idl_create_pstate(flags, NULL, &pstate);
  CU_ASSERT_EQ_FATAL(ret, IDL_RETCODE_OK);
  CU_ASSERT_NEQ_FATAL(pstate, NULL);

  ret = idl_parse_string(pstate, str);
  CU_ASSERT_EQ_FATAL(ret, IDL_RETCODE_OK);
  CU_ASSERT_EQ(pstate->scope, pstate->global_scope);
  return pstate;
}

static idl_pstate_t *
parse_string(const char *str)
{
  return parse_string_flags(0u, str);
}

static void
expect_parse_ret(const char *str, idl_retcode_t expected)
{
  idl_pstate_t *pstate = NULL;
  idl_retcode_t ret;

  ret = idl_create_pstate(0u, NULL, &pstate);
  CU_ASSERT_EQ_FATAL(ret, IDL_RETCODE_OK);
  CU_ASSERT_NEQ_FATAL(pstate, NULL);

  ret = idl_parse_string(pstate, str);
  CU_ASSERT_EQ(ret, expected);
  idl_delete_pstate(pstate);
}

CU_Test(idl_hand_parser, module_with_empty_struct)
{
  idl_pstate_t *pstate;
  idl_module_t *module;
  idl_struct_t *strct;

  pstate = parse_string("module outer { struct Leaf { }; };");
  module = (idl_module_t *) pstate->root;
  CU_ASSERT_NEQ_FATAL(module, NULL);
  CU_ASSERT_FATAL(idl_is_module(module));
  CU_ASSERT_EQ(idl_parent(module), NULL);
  CU_ASSERT_EQ(idl_next(module), NULL);
  CU_ASSERT_STREQ(idl_identifier(module), "outer");

  strct = (idl_struct_t *) module->definitions;
  CU_ASSERT_NEQ_FATAL(strct, NULL);
  CU_ASSERT_FATAL(idl_is_struct(strct));
  CU_ASSERT_EQ(idl_parent(strct), module);
  CU_ASSERT_EQ(idl_next(strct), NULL);
  CU_ASSERT_STREQ(idl_identifier(strct), "Leaf");
  CU_ASSERT_EQ(strct->members, NULL);

  idl_delete_pstate(pstate);
}

CU_Test(idl_hand_parser, nested_module_with_empty_struct)
{
  idl_pstate_t *pstate;
  idl_module_t *outer;
  idl_module_t *inner;
  idl_struct_t *strct;
  const char str[] =
    "module outer {\n"
    "  // comments and newlines should not reach the grammar\n"
    "  module inner { struct Leaf { }; };\n"
    "};\n";

  pstate = parse_string(str);
  outer = (idl_module_t *) pstate->root;
  CU_ASSERT_NEQ_FATAL(outer, NULL);
  CU_ASSERT_FATAL(idl_is_module(outer));
  CU_ASSERT_EQ(idl_parent(outer), NULL);
  CU_ASSERT_EQ(idl_next(outer), NULL);
  CU_ASSERT_STREQ(idl_identifier(outer), "outer");

  inner = (idl_module_t *) outer->definitions;
  CU_ASSERT_NEQ_FATAL(inner, NULL);
  CU_ASSERT_FATAL(idl_is_module(inner));
  CU_ASSERT_EQ(idl_parent(inner), outer);
  CU_ASSERT_EQ(idl_next(inner), NULL);
  CU_ASSERT_STREQ(idl_identifier(inner), "inner");

  strct = (idl_struct_t *) inner->definitions;
  CU_ASSERT_NEQ_FATAL(strct, NULL);
  CU_ASSERT_FATAL(idl_is_struct(strct));
  CU_ASSERT_EQ(idl_parent(strct), inner);
  CU_ASSERT_EQ(idl_next(strct), NULL);
  CU_ASSERT_STREQ(idl_identifier(strct), "Leaf");
  CU_ASSERT_EQ(strct->members, NULL);

  idl_delete_pstate(pstate);
}

CU_Test(idl_hand_parser, strips_identifier_escape)
{
  idl_pstate_t *pstate;
  idl_module_t *module;
  idl_struct_t *strct;

  pstate = parse_string("module _module { struct _struct { }; };");
  module = (idl_module_t *) pstate->root;
  CU_ASSERT_NEQ_FATAL(module, NULL);
  CU_ASSERT_FATAL(idl_is_module(module));
  CU_ASSERT_STREQ(idl_identifier(module), "module");

  strct = (idl_struct_t *) module->definitions;
  CU_ASSERT_NEQ_FATAL(strct, NULL);
  CU_ASSERT_FATAL(idl_is_struct(strct));
  CU_ASSERT_STREQ(idl_identifier(strct), "struct");

  idl_delete_pstate(pstate);
}

CU_Test(idl_hand_parser, struct_with_primitive_member)
{
  idl_pstate_t *pstate;
  idl_struct_t *strct;
  idl_member_t *member;
  idl_declarator_t *declarator;

  pstate = parse_string("struct Sample { long value; };");
  strct = (idl_struct_t *) pstate->root;
  CU_ASSERT_NEQ_FATAL(strct, NULL);
  CU_ASSERT_FATAL(idl_is_struct(strct));
  CU_ASSERT_STREQ(idl_identifier(strct), "Sample");

  member = strct->members;
  CU_ASSERT_NEQ_FATAL(member, NULL);
  CU_ASSERT_FATAL(idl_is_member(member));
  CU_ASSERT_EQ(idl_parent(member), strct);
  CU_ASSERT_EQ(idl_next(member), NULL);
  CU_ASSERT_EQ(idl_mask(member->type_spec), IDL_LONG);
  CU_ASSERT_EQ(idl_parent(member->type_spec), member);

  declarator = member->declarators;
  CU_ASSERT_NEQ_FATAL(declarator, NULL);
  CU_ASSERT_FATAL(idl_is_declarator(declarator));
  CU_ASSERT_EQ(idl_parent(declarator), member);
  CU_ASSERT_EQ(idl_next(declarator), NULL);
  CU_ASSERT_STREQ(idl_identifier(declarator), "value");

  idl_delete_pstate(pstate);
}

CU_Test(idl_hand_parser, struct_with_primitive_member_list)
{
  idl_pstate_t *pstate;
  idl_struct_t *strct;
  idl_member_t *member;
  idl_declarator_t *declarator;
  const char str[] =
    "struct Numbers {"
    "  unsigned long a, b;"
    "  long long c;"
    "  long double d;"
    "  uint32 e;"
    "};";

  pstate = parse_string_flags(IDL_FLAG_EXTENDED_DATA_TYPES, str);
  strct = (idl_struct_t *) pstate->root;
  CU_ASSERT_NEQ_FATAL(strct, NULL);
  CU_ASSERT_FATAL(idl_is_struct(strct));

  member = strct->members;
  CU_ASSERT_NEQ_FATAL(member, NULL);
  CU_ASSERT_EQ(idl_mask(member->type_spec), IDL_ULONG);
  declarator = member->declarators;
  CU_ASSERT_NEQ_FATAL(declarator, NULL);
  CU_ASSERT_STREQ(idl_identifier(declarator), "a");
  declarator = idl_next(declarator);
  CU_ASSERT_NEQ_FATAL(declarator, NULL);
  CU_ASSERT_STREQ(idl_identifier(declarator), "b");
  CU_ASSERT_EQ(idl_next(declarator), NULL);

  member = idl_next(member);
  CU_ASSERT_NEQ_FATAL(member, NULL);
  CU_ASSERT_EQ(idl_mask(member->type_spec), IDL_LLONG);
  CU_ASSERT_STREQ(idl_identifier(member->declarators), "c");

  member = idl_next(member);
  CU_ASSERT_NEQ_FATAL(member, NULL);
  CU_ASSERT_EQ(idl_mask(member->type_spec), IDL_LDOUBLE);
  CU_ASSERT_STREQ(idl_identifier(member->declarators), "d");

  member = idl_next(member);
  CU_ASSERT_NEQ_FATAL(member, NULL);
  CU_ASSERT_EQ(idl_mask(member->type_spec), IDL_UINT32);
  CU_ASSERT_STREQ(idl_identifier(member->declarators), "e");
  CU_ASSERT_EQ(idl_next(member), NULL);

  idl_delete_pstate(pstate);
}

CU_Test(idl_hand_parser, struct_with_local_type_ref)
{
  idl_pstate_t *pstate;
  idl_module_t *module;
  idl_struct_t *s1;
  idl_struct_t *s2;
  idl_member_t *member;
  const char str[] =
    "module m { struct s1 { char c; }; struct s2 { s1 s; }; };";

  pstate = parse_string(str);
  module = (idl_module_t *) pstate->root;
  CU_ASSERT_NEQ_FATAL(module, NULL);
  CU_ASSERT_FATAL(idl_is_module(module));

  s1 = (idl_struct_t *) module->definitions;
  CU_ASSERT_NEQ_FATAL(s1, NULL);
  CU_ASSERT_FATAL(idl_is_struct(s1));
  s2 = idl_next(s1);
  CU_ASSERT_NEQ_FATAL(s2, NULL);
  CU_ASSERT_FATAL(idl_is_struct(s2));

  member = s2->members;
  CU_ASSERT_NEQ_FATAL(member, NULL);
  CU_ASSERT_EQ(member->type_spec, s1);
  CU_ASSERT_STREQ(idl_identifier(member->declarators), "s");

  idl_delete_pstate(pstate);
}

CU_Test(idl_hand_parser, struct_with_cross_module_type_ref)
{
  idl_pstate_t *pstate;
  idl_module_t *m1;
  idl_module_t *m2;
  idl_struct_t *s1;
  idl_struct_t *s2;
  idl_member_t *member;
  const char str[] =
    "module m1 { struct s1 { char c; }; };"
    "module m2 { struct s2 { m1::s1 r; ::m1::s1 a; }; };";

  pstate = parse_string(str);
  m1 = (idl_module_t *) pstate->root;
  CU_ASSERT_NEQ_FATAL(m1, NULL);
  CU_ASSERT_FATAL(idl_is_module(m1));
  s1 = (idl_struct_t *) m1->definitions;
  CU_ASSERT_NEQ_FATAL(s1, NULL);
  CU_ASSERT_FATAL(idl_is_struct(s1));

  m2 = idl_next(m1);
  CU_ASSERT_NEQ_FATAL(m2, NULL);
  CU_ASSERT_FATAL(idl_is_module(m2));
  s2 = (idl_struct_t *) m2->definitions;
  CU_ASSERT_NEQ_FATAL(s2, NULL);
  CU_ASSERT_FATAL(idl_is_struct(s2));

  member = s2->members;
  CU_ASSERT_NEQ_FATAL(member, NULL);
  CU_ASSERT_EQ(member->type_spec, s1);
  CU_ASSERT_STREQ(idl_identifier(member->declarators), "r");

  member = idl_next(member);
  CU_ASSERT_NEQ_FATAL(member, NULL);
  CU_ASSERT_EQ(member->type_spec, s1);
  CU_ASSERT_STREQ(idl_identifier(member->declarators), "a");
  CU_ASSERT_EQ(idl_next(member), NULL);

  idl_delete_pstate(pstate);
}

CU_Test(idl_hand_parser, typedef_with_simple_declarators)
{
  idl_pstate_t *pstate;
  idl_typedef_t *t;
  idl_declarator_t *d;

  pstate = parse_string("typedef char foo, bar, baz;");
  t = (idl_typedef_t *) pstate->root;
  CU_ASSERT_NEQ_FATAL(t, NULL);
  CU_ASSERT_FATAL(idl_is_typedef(t));
  CU_ASSERT_EQ(idl_next(t), NULL);
  CU_ASSERT_EQ(idl_parent(t), NULL);
  CU_ASSERT_EQ(idl_type(t->type_spec), IDL_CHAR);

  d = t->declarators;
  CU_ASSERT_NEQ_FATAL(d, NULL);
  CU_ASSERT_FATAL(idl_is_declarator(d));
  CU_ASSERT_EQ(idl_parent(d), t);
  CU_ASSERT_STREQ(idl_identifier(d), "foo");

  d = idl_next(d);
  CU_ASSERT_NEQ_FATAL(d, NULL);
  CU_ASSERT_FATAL(idl_is_declarator(d));
  CU_ASSERT_EQ(idl_parent(d), t);
  CU_ASSERT_STREQ(idl_identifier(d), "bar");

  d = idl_next(d);
  CU_ASSERT_NEQ_FATAL(d, NULL);
  CU_ASSERT_FATAL(idl_is_declarator(d));
  CU_ASSERT_EQ(idl_parent(d), t);
  CU_ASSERT_STREQ(idl_identifier(d), "baz");
  CU_ASSERT_EQ(idl_next(d), NULL);

  idl_delete_pstate(pstate);
}

CU_Test(idl_hand_parser, typedef_used_as_member_type)
{
  idl_pstate_t *pstate;
  idl_typedef_t *t;
  idl_struct_t *strct;
  idl_member_t *member;
  const char str[] = "typedef long my_long; struct Sample { my_long value; };";

  pstate = parse_string(str);
  t = (idl_typedef_t *) pstate->root;
  CU_ASSERT_NEQ_FATAL(t, NULL);
  CU_ASSERT_FATAL(idl_is_typedef(t));
  CU_ASSERT_EQ(idl_type(t->type_spec), IDL_LONG);

  strct = idl_next(t);
  CU_ASSERT_NEQ_FATAL(strct, NULL);
  CU_ASSERT_FATAL(idl_is_struct(strct));
  member = strct->members;
  CU_ASSERT_NEQ_FATAL(member, NULL);
  CU_ASSERT_EQ(member->type_spec, t->declarators);
  CU_ASSERT_STREQ(idl_identifier(member->declarators), "value");

  idl_delete_pstate(pstate);
}

CU_Test(idl_hand_parser, struct_forward_declaration)
{
  idl_pstate_t *pstate;
  idl_forward_t *forward;
  idl_struct_t *strct;

  pstate = parse_string("struct Node; struct Node { long value; };");
  forward = (idl_forward_t *) pstate->root;
  CU_ASSERT_NEQ_FATAL(forward, NULL);
  CU_ASSERT_FATAL(idl_is_forward(forward));
  CU_ASSERT_STREQ(idl_identifier(forward), "Node");

  strct = idl_next(forward);
  CU_ASSERT_NEQ_FATAL(strct, NULL);
  CU_ASSERT_FATAL(idl_is_struct(strct));
  CU_ASSERT_STREQ(idl_identifier(strct), "Node");
  CU_ASSERT_EQ(forward->type_spec, (idl_type_spec_t *) strct);
  CU_ASSERT_EQ(idl_next(strct), NULL);

  idl_delete_pstate(pstate);
}

CU_Test(idl_hand_parser, repeated_struct_forward_declarations)
{
  idl_pstate_t *pstate;
  idl_forward_t *first;
  idl_forward_t *second;
  idl_struct_t *strct;

  pstate = parse_string(
    "struct Node; struct Node; struct Node { long value; };");
  first = (idl_forward_t *) pstate->root;
  CU_ASSERT_NEQ_FATAL(first, NULL);
  CU_ASSERT_FATAL(idl_is_forward(first));
  second = idl_next(first);
  CU_ASSERT_NEQ_FATAL(second, NULL);
  CU_ASSERT_FATAL(idl_is_forward(second));

  strct = idl_next(second);
  CU_ASSERT_NEQ_FATAL(strct, NULL);
  CU_ASSERT_FATAL(idl_is_struct(strct));
  CU_ASSERT_EQ(first->type_spec, (idl_type_spec_t *) strct);
  CU_ASSERT_EQ(second->type_spec, (idl_type_spec_t *) strct);
  CU_ASSERT_EQ(idl_next(strct), NULL);

  idl_delete_pstate(pstate);
}

CU_Test(idl_hand_parser, struct_inheritance)
{
  idl_pstate_t *pstate;
  idl_struct_t *base;
  idl_struct_t *derived;
  const char str[] =
    "struct Base { long base_member; };"
    "struct Derived : Base { long derived_member; };";

  pstate = parse_string(str);
  base = (idl_struct_t *) pstate->root;
  CU_ASSERT_NEQ_FATAL(base, NULL);
  CU_ASSERT_FATAL(idl_is_struct(base));
  CU_ASSERT_STREQ(idl_identifier(base), "Base");

  derived = idl_next(base);
  CU_ASSERT_NEQ_FATAL(derived, NULL);
  CU_ASSERT_FATAL(idl_is_struct(derived));
  CU_ASSERT_STREQ(idl_identifier(derived), "Derived");
  CU_ASSERT_NEQ_FATAL(derived->inherit_spec, NULL);
  CU_ASSERT_EQ(derived->inherit_spec->base, (idl_type_spec_t *) base);
  CU_ASSERT_EQ(idl_parent(derived->inherit_spec), derived);

  idl_delete_pstate(pstate);
}

CU_Test(idl_hand_parser, struct_inheritance_through_typedef)
{
  idl_pstate_t *pstate;
  idl_struct_t *base;
  idl_typedef_t *alias;
  idl_struct_t *derived;
  const char str[] =
    "struct Base { long base_member; };"
    "typedef Base BaseAlias;"
    "struct Derived : BaseAlias { long derived_member; };";

  pstate = parse_string(str);
  base = (idl_struct_t *) pstate->root;
  CU_ASSERT_NEQ_FATAL(base, NULL);
  CU_ASSERT_FATAL(idl_is_struct(base));

  alias = idl_next(base);
  CU_ASSERT_NEQ_FATAL(alias, NULL);
  CU_ASSERT_FATAL(idl_is_typedef(alias));
  derived = idl_next(alias);
  CU_ASSERT_NEQ_FATAL(derived, NULL);
  CU_ASSERT_FATAL(idl_is_struct(derived));
  CU_ASSERT_NEQ_FATAL(derived->inherit_spec, NULL);
  CU_ASSERT_EQ(derived->inherit_spec->base, (idl_type_spec_t *) base);

  idl_delete_pstate(pstate);
}

CU_Test(idl_hand_parser, struct_with_array_declarators)
{
  idl_pstate_t *pstate;
  idl_struct_t *strct;
  idl_member_t *member;
  idl_declarator_t *declarator;
  const idl_literal_t *bound;
  const char str[] =
    "struct Sample {"
    "  long matrix[2][3], scalar;"
    "  char bytes[4];"
    "};";

  pstate = parse_string(str);
  strct = (idl_struct_t *) pstate->root;
  CU_ASSERT_NEQ_FATAL(strct, NULL);
  CU_ASSERT_FATAL(idl_is_struct(strct));

  member = strct->members;
  CU_ASSERT_NEQ_FATAL(member, NULL);
  CU_ASSERT_EQ(idl_type(member->type_spec), IDL_LONG);

  declarator = member->declarators;
  CU_ASSERT_NEQ_FATAL(declarator, NULL);
  CU_ASSERT_STREQ(idl_identifier(declarator), "matrix");
  CU_ASSERT_FATAL(idl_is_array(declarator));
  bound = declarator->const_expr;
  CU_ASSERT_NEQ_FATAL(bound, NULL);
  CU_ASSERT_FATAL(idl_is_literal(bound));
  CU_ASSERT_EQ(idl_type(bound), IDL_ULONG);
  CU_ASSERT_EQ(bound->value.uint32, 2u);
  CU_ASSERT_EQ(idl_parent(bound), declarator);
  bound = idl_next(bound);
  CU_ASSERT_NEQ_FATAL(bound, NULL);
  CU_ASSERT_FATAL(idl_is_literal(bound));
  CU_ASSERT_EQ(bound->value.uint32, 3u);
  CU_ASSERT_EQ(idl_parent(bound), declarator);
  CU_ASSERT_EQ(idl_next(bound), NULL);

  declarator = idl_next(declarator);
  CU_ASSERT_NEQ_FATAL(declarator, NULL);
  CU_ASSERT_STREQ(idl_identifier(declarator), "scalar");
  CU_ASSERT(!idl_is_array(declarator));
  CU_ASSERT_EQ(declarator->const_expr, NULL);
  CU_ASSERT_EQ(idl_next(declarator), NULL);

  member = idl_next(member);
  CU_ASSERT_NEQ_FATAL(member, NULL);
  CU_ASSERT_EQ(idl_type(member->type_spec), IDL_CHAR);
  declarator = member->declarators;
  CU_ASSERT_NEQ_FATAL(declarator, NULL);
  CU_ASSERT_STREQ(idl_identifier(declarator), "bytes");
  CU_ASSERT_FATAL(idl_is_array(declarator));
  bound = declarator->const_expr;
  CU_ASSERT_NEQ_FATAL(bound, NULL);
  CU_ASSERT_EQ(bound->value.uint32, 4u);
  CU_ASSERT_EQ(idl_next(bound), NULL);
  CU_ASSERT_EQ(idl_next(member), NULL);

  idl_delete_pstate(pstate);
}

CU_Test(idl_hand_parser, typedef_with_array_declarators)
{
  idl_pstate_t *pstate;
  idl_typedef_t *typedef_node;
  idl_declarator_t *matrix;
  idl_declarator_t *vector;
  const idl_literal_t *bound;

  pstate = parse_string("typedef long Matrix[2][3], Vector[4];");
  typedef_node = (idl_typedef_t *) pstate->root;
  CU_ASSERT_NEQ_FATAL(typedef_node, NULL);
  CU_ASSERT_FATAL(idl_is_typedef(typedef_node));
  CU_ASSERT_EQ(idl_type(typedef_node->type_spec), IDL_LONG);

  matrix = typedef_node->declarators;
  CU_ASSERT_NEQ_FATAL(matrix, NULL);
  CU_ASSERT_FATAL(idl_is_array(matrix));
  CU_ASSERT_STREQ(idl_identifier(matrix), "Matrix");
  bound = matrix->const_expr;
  CU_ASSERT_NEQ_FATAL(bound, NULL);
  CU_ASSERT_EQ(bound->value.uint32, 2u);
  bound = idl_next(bound);
  CU_ASSERT_NEQ_FATAL(bound, NULL);
  CU_ASSERT_EQ(bound->value.uint32, 3u);
  CU_ASSERT_EQ(idl_next(bound), NULL);

  vector = idl_next(matrix);
  CU_ASSERT_NEQ_FATAL(vector, NULL);
  CU_ASSERT_FATAL(idl_is_array(vector));
  CU_ASSERT_STREQ(idl_identifier(vector), "Vector");
  bound = vector->const_expr;
  CU_ASSERT_NEQ_FATAL(bound, NULL);
  CU_ASSERT_EQ(bound->value.uint32, 4u);
  CU_ASSERT_EQ(idl_next(bound), NULL);
  CU_ASSERT_EQ(idl_next(vector), NULL);

  idl_delete_pstate(pstate);
}

CU_Test(idl_hand_parser, array_declarator_rejects_zero_bound)
{
  expect_parse_ret(
    "struct Sample { long values[0]; };", IDL_RETCODE_OUT_OF_RANGE);
}

CU_Test(idl_hand_parser, array_declarator_rejects_oversized_bound)
{
  expect_parse_ret(
    "struct Sample { long values[4294967296]; };",
    IDL_RETCODE_OUT_OF_RANGE);
}

CU_Test(idl_hand_parser, struct_with_string_and_wstring_members)
{
  idl_pstate_t *pstate;
  idl_struct_t *strct;
  idl_member_t *member;
  const char str[] =
    "struct Text {"
    "  string name;"
    "  string<12> label;"
    "  wstring wide;"
    "  wstring<7> wide_label;"
    "};";

  pstate = parse_string(str);
  strct = (idl_struct_t *) pstate->root;
  CU_ASSERT_NEQ_FATAL(strct, NULL);
  CU_ASSERT_FATAL(idl_is_struct(strct));

  member = strct->members;
  CU_ASSERT_NEQ_FATAL(member, NULL);
  CU_ASSERT_FATAL(idl_is_unbounded_string(member->type_spec));
  CU_ASSERT_EQ(idl_bound(member->type_spec), 0u);
  CU_ASSERT_STREQ(idl_identifier(member->declarators), "name");

  member = idl_next(member);
  CU_ASSERT_NEQ_FATAL(member, NULL);
  CU_ASSERT_FATAL(idl_is_bounded_string(member->type_spec));
  CU_ASSERT_EQ(idl_bound(member->type_spec), 12u);
  CU_ASSERT_STREQ(idl_identifier(member->declarators), "label");

  member = idl_next(member);
  CU_ASSERT_NEQ_FATAL(member, NULL);
  CU_ASSERT_FATAL(idl_is_unbounded_wstring(member->type_spec));
  CU_ASSERT_EQ(idl_bound(member->type_spec), 0u);
  CU_ASSERT_STREQ(idl_identifier(member->declarators), "wide");

  member = idl_next(member);
  CU_ASSERT_NEQ_FATAL(member, NULL);
  CU_ASSERT_FATAL(idl_is_bounded_wstring(member->type_spec));
  CU_ASSERT_EQ(idl_bound(member->type_spec), 7u);
  CU_ASSERT_STREQ(idl_identifier(member->declarators), "wide_label");
  CU_ASSERT_EQ(idl_next(member), NULL);

  idl_delete_pstate(pstate);
}

CU_Test(idl_hand_parser, struct_with_sequence_members)
{
  idl_pstate_t *pstate;
  idl_struct_t *strct;
  idl_member_t *member;
  idl_sequence_t *sequence;
  const char str[] =
    "struct Samples {"
    "  sequence<long> values;"
    "  sequence<char, 4> bytes;"
    "  sequence<sequence<long, 2>, 3> nested;"
    "};";

  pstate = parse_string(str);
  strct = (idl_struct_t *) pstate->root;
  CU_ASSERT_NEQ_FATAL(strct, NULL);
  CU_ASSERT_FATAL(idl_is_struct(strct));

  member = strct->members;
  CU_ASSERT_NEQ_FATAL(member, NULL);
  CU_ASSERT_FATAL(idl_is_sequence(member->type_spec));
  sequence = (idl_sequence_t *) member->type_spec;
  CU_ASSERT_EQ(idl_bound(sequence), 0u);
  CU_ASSERT_EQ(idl_type(sequence->type_spec), IDL_LONG);
  CU_ASSERT_STREQ(idl_identifier(member->declarators), "values");

  member = idl_next(member);
  CU_ASSERT_NEQ_FATAL(member, NULL);
  CU_ASSERT_FATAL(idl_is_sequence(member->type_spec));
  sequence = (idl_sequence_t *) member->type_spec;
  CU_ASSERT_EQ(idl_bound(sequence), 4u);
  CU_ASSERT_EQ(idl_type(sequence->type_spec), IDL_CHAR);
  CU_ASSERT_STREQ(idl_identifier(member->declarators), "bytes");

  member = idl_next(member);
  CU_ASSERT_NEQ_FATAL(member, NULL);
  CU_ASSERT_FATAL(idl_is_sequence(member->type_spec));
  sequence = (idl_sequence_t *) member->type_spec;
  CU_ASSERT_EQ(idl_bound(sequence), 3u);
  CU_ASSERT_FATAL(idl_is_sequence(sequence->type_spec));
  sequence = (idl_sequence_t *) sequence->type_spec;
  CU_ASSERT_EQ(idl_bound(sequence), 2u);
  CU_ASSERT_EQ(idl_type(sequence->type_spec), IDL_LONG);
  CU_ASSERT_STREQ(idl_identifier(member->declarators), "nested");
  CU_ASSERT_EQ(idl_next(member), NULL);

  idl_delete_pstate(pstate);
}

CU_Test(idl_hand_parser, typedef_with_sequence_type)
{
  idl_pstate_t *pstate;
  idl_typedef_t *typedef_node;
  idl_struct_t *strct;
  idl_member_t *member;
  idl_sequence_t *sequence;
  const char str[] =
    "typedef sequence<long, 5> Longs;"
    "struct Sample { Longs values; };";

  pstate = parse_string(str);
  typedef_node = (idl_typedef_t *) pstate->root;
  CU_ASSERT_NEQ_FATAL(typedef_node, NULL);
  CU_ASSERT_FATAL(idl_is_typedef(typedef_node));
  CU_ASSERT_FATAL(idl_is_sequence(typedef_node->type_spec));
  sequence = (idl_sequence_t *) typedef_node->type_spec;
  CU_ASSERT_EQ(idl_bound(sequence), 5u);
  CU_ASSERT_EQ(idl_type(sequence->type_spec), IDL_LONG);

  strct = idl_next(typedef_node);
  CU_ASSERT_NEQ_FATAL(strct, NULL);
  CU_ASSERT_FATAL(idl_is_struct(strct));
  member = strct->members;
  CU_ASSERT_NEQ_FATAL(member, NULL);
  CU_ASSERT_EQ(member->type_spec, typedef_node->declarators);
  CU_ASSERT_STREQ(idl_identifier(member->declarators), "values");

  idl_delete_pstate(pstate);
}

CU_Test(idl_hand_parser, string_type_rejects_uint32_max_bound)
{
  expect_parse_ret(
    "struct Sample { string<4294967295> value; };",
    IDL_RETCODE_UNSUPPORTED);
}
