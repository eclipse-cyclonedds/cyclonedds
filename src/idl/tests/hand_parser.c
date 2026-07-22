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
expect_parse_ret_flags(uint32_t flags, const char *str, idl_retcode_t expected)
{
  idl_pstate_t *pstate = NULL;
  idl_retcode_t ret;

  ret = idl_create_pstate(flags, NULL, &pstate);
  CU_ASSERT_EQ_FATAL(ret, IDL_RETCODE_OK);
  CU_ASSERT_NEQ_FATAL(pstate, NULL);

  ret = idl_parse_string(pstate, str);
  CU_ASSERT_EQ(ret, expected);
  idl_delete_pstate(pstate);
}

static void
expect_parse_ret(const char *str, idl_retcode_t expected)
{
  expect_parse_ret_flags(0u, str, expected);
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

CU_Test(idl_hand_parser, union_forward_declaration)
{
  expect_parse_ret("union Choice;", IDL_RETCODE_SEMANTIC_ERROR);
}

CU_Test(idl_hand_parser, repeated_union_forward_declarations)
{
  expect_parse_ret("union Choice; union Choice;", IDL_RETCODE_SEMANTIC_ERROR);
}

CU_Test(idl_hand_parser, union_with_single_case)
{
  idl_pstate_t *pstate;
  idl_union_t *union_node;
  idl_case_t *case_node;
  idl_case_label_t *case_label;

  pstate = parse_string("union Choice switch(long) { case 1: char c; };");
  union_node = (idl_union_t *) pstate->root;
  CU_ASSERT_NEQ_FATAL(union_node, NULL);
  CU_ASSERT_FATAL(idl_is_union(union_node));
  CU_ASSERT_STREQ(idl_identifier(union_node), "Choice");
  CU_ASSERT_EQ(idl_type(union_node->switch_type_spec->type_spec), IDL_LONG);
  CU_ASSERT_EQ(idl_parent(union_node->switch_type_spec), union_node);

  case_node = union_node->cases;
  CU_ASSERT_NEQ_FATAL(case_node, NULL);
  CU_ASSERT_FATAL(idl_is_case(case_node));
  CU_ASSERT_EQ(idl_parent(case_node), union_node);
  case_label = case_node->labels;
  CU_ASSERT_NEQ_FATAL(case_label, NULL);
  CU_ASSERT_FATAL(idl_is_case_label(case_label));
  CU_ASSERT_NEQ(case_label->const_expr, NULL);
  CU_ASSERT_EQ(idl_next(case_label), NULL);
  CU_ASSERT_EQ(idl_type(case_node->type_spec), IDL_CHAR);
  CU_ASSERT_FATAL(idl_is_declarator(case_node->declarator));
  CU_ASSERT_STREQ(idl_identifier(case_node->declarator), "c");
  CU_ASSERT_EQ(idl_next(case_node), NULL);

  idl_delete_pstate(pstate);
}

CU_Test(idl_hand_parser, union_with_default_case)
{
  idl_pstate_t *pstate;
  idl_union_t *union_node;
  idl_case_t *case_node;

  pstate = parse_string("union Choice switch(char) { default: long value; };");
  union_node = (idl_union_t *) pstate->root;
  CU_ASSERT_NEQ_FATAL(union_node, NULL);
  CU_ASSERT_FATAL(idl_is_union(union_node));
  CU_ASSERT_EQ(idl_type(union_node->switch_type_spec->type_spec), IDL_CHAR);

  case_node = union_node->cases;
  CU_ASSERT_NEQ_FATAL(case_node, NULL);
  CU_ASSERT_FATAL(idl_is_case(case_node));
  CU_ASSERT_FATAL(idl_is_default_case(case_node));
  CU_ASSERT_EQ(union_node->default_case, case_node->labels);
  CU_ASSERT_EQ(idl_type(case_node->type_spec), IDL_LONG);
  CU_ASSERT_STREQ(idl_identifier(case_node->declarator), "value");
  CU_ASSERT_EQ(idl_next(case_node), NULL);

  idl_delete_pstate(pstate);
}

CU_Test(idl_hand_parser, union_with_multiple_case_labels)
{
  idl_pstate_t *pstate;
  idl_union_t *union_node;
  idl_case_t *case_node;
  idl_case_label_t *case_label;

  pstate = parse_string(
    "union Choice switch(long) { case 1: case 2: char c; default: long d; };");
  union_node = (idl_union_t *) pstate->root;
  CU_ASSERT_NEQ_FATAL(union_node, NULL);
  CU_ASSERT_FATAL(idl_is_union(union_node));

  case_node = union_node->cases;
  CU_ASSERT_NEQ_FATAL(case_node, NULL);
  CU_ASSERT_FATAL(idl_is_case(case_node));
  case_label = case_node->labels;
  CU_ASSERT_NEQ_FATAL(case_label, NULL);
  CU_ASSERT_FATAL(idl_is_case_label(case_label));
  case_label = idl_next(case_label);
  CU_ASSERT_NEQ_FATAL(case_label, NULL);
  CU_ASSERT_FATAL(idl_is_case_label(case_label));
  CU_ASSERT_EQ(idl_next(case_label), NULL);
  CU_ASSERT_STREQ(idl_identifier(case_node->declarator), "c");

  case_node = idl_next(case_node);
  CU_ASSERT_NEQ_FATAL(case_node, NULL);
  CU_ASSERT_FATAL(idl_is_default_case(case_node));
  CU_ASSERT_STREQ(idl_identifier(case_node->declarator), "d");
  CU_ASSERT_EQ(idl_next(case_node), NULL);

  idl_delete_pstate(pstate);
}

CU_Test(idl_hand_parser, union_with_boolean_case_labels)
{
  idl_pstate_t *pstate;
  idl_union_t *union_node;
  idl_case_t *case_node;

  pstate = parse_string(
    "union Choice switch(boolean) { case TRUE: char t; case FALSE: char f; };");
  union_node = (idl_union_t *) pstate->root;
  CU_ASSERT_NEQ_FATAL(union_node, NULL);
  CU_ASSERT_FATAL(idl_is_union(union_node));
  CU_ASSERT_EQ(idl_type(union_node->switch_type_spec->type_spec), IDL_BOOL);

  case_node = union_node->cases;
  CU_ASSERT_NEQ_FATAL(case_node, NULL);
  CU_ASSERT_FATAL(idl_is_case(case_node));
  CU_ASSERT_EQ(idl_type(case_node->labels->const_expr), IDL_BOOL);
  CU_ASSERT_STREQ(idl_identifier(case_node->declarator), "t");

  case_node = idl_next(case_node);
  CU_ASSERT_NEQ_FATAL(case_node, NULL);
  CU_ASSERT_FATAL(idl_is_case(case_node));
  CU_ASSERT_EQ(idl_type(case_node->labels->const_expr), IDL_BOOL);
  CU_ASSERT_STREQ(idl_identifier(case_node->declarator), "f");
  CU_ASSERT_EQ(idl_next(case_node), NULL);

  idl_delete_pstate(pstate);
}

CU_Test(idl_hand_parser, union_with_char_case_label)
{
  idl_pstate_t *pstate;
  idl_union_t *union_node;
  idl_case_t *case_node;

  pstate = parse_string("union Choice switch(char) { case 'x': long value; };");
  union_node = (idl_union_t *) pstate->root;
  CU_ASSERT_NEQ_FATAL(union_node, NULL);
  CU_ASSERT_FATAL(idl_is_union(union_node));
  CU_ASSERT_EQ(idl_type(union_node->switch_type_spec->type_spec), IDL_CHAR);

  case_node = union_node->cases;
  CU_ASSERT_NEQ_FATAL(case_node, NULL);
  CU_ASSERT_FATAL(idl_is_case(case_node));
  CU_ASSERT_EQ(idl_type(case_node->labels->const_expr), IDL_CHAR);
  CU_ASSERT_EQ(
    ((idl_literal_t *) case_node->labels->const_expr)->value.chr, 'x');
  CU_ASSERT_STREQ(idl_identifier(case_node->declarator), "value");
  CU_ASSERT_EQ(idl_next(case_node), NULL);

  idl_delete_pstate(pstate);
}

CU_Test(idl_hand_parser, union_with_parenthesized_unary_case_labels)
{
  idl_pstate_t *pstate;
  idl_union_t *union_node;
  idl_case_t *case_node;

  pstate = parse_string(
    "union Choice switch(long) {"
    "  case (-1): char negative;"
    "  case (+2): char positive;"
    "  case ~3: char inverted;"
    "};");
  union_node = (idl_union_t *) pstate->root;
  CU_ASSERT_NEQ_FATAL(union_node, NULL);
  CU_ASSERT_FATAL(idl_is_union(union_node));

  case_node = union_node->cases;
  CU_ASSERT_NEQ_FATAL(case_node, NULL);
  CU_ASSERT_EQ(idl_case_label_intvalue(case_node->labels), -1);
  CU_ASSERT_STREQ(idl_identifier(case_node->declarator), "negative");

  case_node = idl_next(case_node);
  CU_ASSERT_NEQ_FATAL(case_node, NULL);
  CU_ASSERT_EQ(idl_case_label_intvalue(case_node->labels), 2);
  CU_ASSERT_STREQ(idl_identifier(case_node->declarator), "positive");

  case_node = idl_next(case_node);
  CU_ASSERT_NEQ_FATAL(case_node, NULL);
  CU_ASSERT_EQ(idl_case_label_intvalue(case_node->labels), -4);
  CU_ASSERT_STREQ(idl_identifier(case_node->declarator), "inverted");
  CU_ASSERT_EQ(idl_next(case_node), NULL);

  idl_delete_pstate(pstate);
}

CU_Test(idl_hand_parser, union_with_binary_precedence_case_labels)
{
  idl_pstate_t *pstate;
  idl_union_t *union_node;
  idl_case_t *case_node;

  pstate = parse_string(
    "union Choice switch(long) {"
    "  case 1 + 2 * 3: char seven;"
    "  case (1 + 2) * 4: char twelve;"
    "  case 1 << 4 | 3: char nineteen;"
    "  case 7 & 3 ^ 1: char two;"
    "};");
  union_node = (idl_union_t *) pstate->root;
  CU_ASSERT_NEQ_FATAL(union_node, NULL);
  CU_ASSERT_FATAL(idl_is_union(union_node));

  case_node = union_node->cases;
  CU_ASSERT_NEQ_FATAL(case_node, NULL);
  CU_ASSERT_EQ(idl_case_label_intvalue(case_node->labels), 7);
  CU_ASSERT_STREQ(idl_identifier(case_node->declarator), "seven");

  case_node = idl_next(case_node);
  CU_ASSERT_NEQ_FATAL(case_node, NULL);
  CU_ASSERT_EQ(idl_case_label_intvalue(case_node->labels), 12);
  CU_ASSERT_STREQ(idl_identifier(case_node->declarator), "twelve");

  case_node = idl_next(case_node);
  CU_ASSERT_NEQ_FATAL(case_node, NULL);
  CU_ASSERT_EQ(idl_case_label_intvalue(case_node->labels), 19);
  CU_ASSERT_STREQ(idl_identifier(case_node->declarator), "nineteen");

  case_node = idl_next(case_node);
  CU_ASSERT_NEQ_FATAL(case_node, NULL);
  CU_ASSERT_EQ(idl_case_label_intvalue(case_node->labels), 2);
  CU_ASSERT_STREQ(idl_identifier(case_node->declarator), "two");
  CU_ASSERT_EQ(idl_next(case_node), NULL);

  idl_delete_pstate(pstate);
}

CU_Test(idl_hand_parser, union_with_enum_case_label)
{
  idl_pstate_t *pstate;
  idl_enum_t *color;
  idl_union_t *union_node;
  idl_case_t *case_node;
  const char str[] =
    "enum Color { Red, Yellow, Blue };"
    "union Choice switch(Color) { case Red: char c; default: long d; };";

  pstate = parse_string(str);
  color = (idl_enum_t *) pstate->root;
  CU_ASSERT_NEQ_FATAL(color, NULL);
  CU_ASSERT_FATAL(idl_is_enum(color));

  union_node = idl_next(color);
  CU_ASSERT_NEQ_FATAL(union_node, NULL);
  CU_ASSERT_FATAL(idl_is_union(union_node));
  CU_ASSERT_EQ(
    union_node->switch_type_spec->type_spec, (idl_type_spec_t *) color);

  case_node = union_node->cases;
  CU_ASSERT_NEQ_FATAL(case_node, NULL);
  CU_ASSERT_FATAL(idl_is_case(case_node));
  CU_ASSERT_EQ(case_node->labels->const_expr, color->enumerators);
  CU_ASSERT_STREQ(idl_identifier(case_node->declarator), "c");

  case_node = idl_next(case_node);
  CU_ASSERT_NEQ_FATAL(case_node, NULL);
  CU_ASSERT_FATAL(idl_is_default_case(case_node));
  CU_ASSERT_STREQ(idl_identifier(case_node->declarator), "d");
  CU_ASSERT_EQ(idl_next(case_node), NULL);

  idl_delete_pstate(pstate);
}

CU_Test(idl_hand_parser, union_rejects_case_label_from_other_enum)
{
  const char str[] =
    "enum Color { Red };"
    "enum Shape { Circle };"
    "union Choice switch(Color) { case Circle: char c; };";

  expect_parse_ret(str, IDL_RETCODE_SEMANTIC_ERROR);
}

CU_Test(idl_hand_parser, union_forward_declaration_linked_to_definition)
{
  idl_pstate_t *pstate;
  idl_forward_t *forward;
  idl_union_t *union_node;

  pstate = parse_string(
    "union Choice; union Choice switch(long) { case 1: char c; };");
  forward = (idl_forward_t *) pstate->root;
  CU_ASSERT_NEQ_FATAL(forward, NULL);
  CU_ASSERT_FATAL(idl_is_forward(forward));
  CU_ASSERT_EQ(idl_mask(forward), IDL_UNION | IDL_FORWARD);
  CU_ASSERT_STREQ(idl_identifier(forward), "Choice");

  union_node = idl_next(forward);
  CU_ASSERT_NEQ_FATAL(union_node, NULL);
  CU_ASSERT_FATAL(idl_is_union(union_node));
  CU_ASSERT_STREQ(idl_identifier(union_node), "Choice");
  CU_ASSERT_EQ(forward->type_spec, (idl_type_spec_t *) union_node);
  CU_ASSERT_EQ(idl_next(union_node), NULL);

  idl_delete_pstate(pstate);
}

CU_Test(idl_hand_parser, union_rejects_empty_body)
{
  expect_parse_ret(
    "union Choice switch(long) { };", IDL_RETCODE_SYNTAX_ERROR);
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

CU_Test(idl_hand_parser, array_declarator_with_expression_bounds)
{
  idl_pstate_t *pstate;
  idl_struct_t *strct;
  idl_member_t *member;
  idl_declarator_t *declarator;
  const idl_literal_t *bound;
  const char str[] =
    "struct Sample {"
    "  long matrix[1 + 1][1 << 2];"
    "  char bytes[(7 & 3) + 1];"
    "};";

  pstate = parse_string(str);
  strct = (idl_struct_t *) pstate->root;
  CU_ASSERT_NEQ_FATAL(strct, NULL);
  CU_ASSERT_FATAL(idl_is_struct(strct));

  member = strct->members;
  CU_ASSERT_NEQ_FATAL(member, NULL);
  declarator = member->declarators;
  CU_ASSERT_NEQ_FATAL(declarator, NULL);
  CU_ASSERT_FATAL(idl_is_array(declarator));
  bound = declarator->const_expr;
  CU_ASSERT_NEQ_FATAL(bound, NULL);
  CU_ASSERT_EQ(bound->value.uint32, 2u);
  bound = idl_next(bound);
  CU_ASSERT_NEQ_FATAL(bound, NULL);
  CU_ASSERT_EQ(bound->value.uint32, 4u);
  CU_ASSERT_EQ(idl_next(bound), NULL);

  member = idl_next(member);
  CU_ASSERT_NEQ_FATAL(member, NULL);
  declarator = member->declarators;
  CU_ASSERT_NEQ_FATAL(declarator, NULL);
  CU_ASSERT_FATAL(idl_is_array(declarator));
  bound = declarator->const_expr;
  CU_ASSERT_NEQ_FATAL(bound, NULL);
  CU_ASSERT_EQ(bound->value.uint32, 4u);
  CU_ASSERT_EQ(idl_next(bound), NULL);
  CU_ASSERT_EQ(idl_next(member), NULL);

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

CU_Test(idl_hand_parser, struct_with_expression_template_bounds)
{
  idl_pstate_t *pstate;
  idl_struct_t *strct;
  idl_member_t *member;
  idl_sequence_t *sequence;
  const char str[] =
    "struct Text {"
    "  string<2 * 6> label;"
    "  wstring<(3 + 4)> wide_label;"
    "  sequence<char, 1 + 3> bytes;"
    "  sequence<sequence<long, 1 + 1>, (1 << 1) + 1> nested;"
    "};";

  pstate = parse_string(str);
  strct = (idl_struct_t *) pstate->root;
  CU_ASSERT_NEQ_FATAL(strct, NULL);
  CU_ASSERT_FATAL(idl_is_struct(strct));

  member = strct->members;
  CU_ASSERT_NEQ_FATAL(member, NULL);
  CU_ASSERT_FATAL(idl_is_bounded_string(member->type_spec));
  CU_ASSERT_EQ(idl_bound(member->type_spec), 12u);

  member = idl_next(member);
  CU_ASSERT_NEQ_FATAL(member, NULL);
  CU_ASSERT_FATAL(idl_is_bounded_wstring(member->type_spec));
  CU_ASSERT_EQ(idl_bound(member->type_spec), 7u);

  member = idl_next(member);
  CU_ASSERT_NEQ_FATAL(member, NULL);
  CU_ASSERT_FATAL(idl_is_sequence(member->type_spec));
  sequence = (idl_sequence_t *) member->type_spec;
  CU_ASSERT_EQ(idl_bound(sequence), 4u);

  member = idl_next(member);
  CU_ASSERT_NEQ_FATAL(member, NULL);
  CU_ASSERT_FATAL(idl_is_sequence(member->type_spec));
  sequence = (idl_sequence_t *) member->type_spec;
  CU_ASSERT_EQ(idl_bound(sequence), 3u);
  CU_ASSERT_FATAL(idl_is_sequence(sequence->type_spec));
  sequence = (idl_sequence_t *) sequence->type_spec;
  CU_ASSERT_EQ(idl_bound(sequence), 2u);
  CU_ASSERT_EQ(idl_next(member), NULL);

  idl_delete_pstate(pstate);
}

CU_Test(idl_hand_parser, const_declaration_with_integer_expression)
{
  idl_pstate_t *pstate;
  idl_const_t *const_node;
  const idl_literal_t *literal;

  pstate = parse_string("const long VALUE = 1 + 2 * 3;");
  const_node = (idl_const_t *) pstate->root;
  CU_ASSERT_NEQ_FATAL(const_node, NULL);
  CU_ASSERT_FATAL(idl_is_const(const_node));
  CU_ASSERT_STREQ(idl_identifier(const_node), "VALUE");
  CU_ASSERT_EQ(idl_type(const_node->type_spec), IDL_LONG);
  CU_ASSERT_NEQ_FATAL(const_node->const_expr, NULL);
  CU_ASSERT_FATAL(idl_is_literal(const_node->const_expr));
  CU_ASSERT_EQ(idl_type(const_node->const_expr), IDL_LONG);
  literal = (const idl_literal_t *) const_node->const_expr;
  CU_ASSERT_EQ(literal->value.int32, 7);
  CU_ASSERT_EQ(idl_parent(literal), const_node);

  idl_delete_pstate(pstate);
}

CU_Test(idl_hand_parser, const_declaration_with_float_and_string_literals)
{
  idl_pstate_t *pstate;
  idl_const_t *scale;
  idl_const_t *ratio;
  idl_const_t *label;
  const idl_literal_t *literal;
  const char str[] =
    "const double SCALE = 1.25;"
    "const float RATIO = SCALE;"
    "const string LABEL = \"ab\" \"cd\";";

  pstate = parse_string(str);
  scale = (idl_const_t *) pstate->root;
  CU_ASSERT_NEQ_FATAL(scale, NULL);
  CU_ASSERT_FATAL(idl_is_const(scale));
  CU_ASSERT_STREQ(idl_identifier(scale), "SCALE");
  CU_ASSERT_EQ(idl_type(scale->type_spec), IDL_DOUBLE);
  CU_ASSERT_EQ(idl_type(scale->const_expr), IDL_DOUBLE);
  literal = (const idl_literal_t *) scale->const_expr;
  CU_ASSERT_EQ(literal->value.dbl, 1.25);

  ratio = idl_next(scale);
  CU_ASSERT_NEQ_FATAL(ratio, NULL);
  CU_ASSERT_FATAL(idl_is_const(ratio));
  CU_ASSERT_STREQ(idl_identifier(ratio), "RATIO");
  CU_ASSERT_EQ(idl_type(ratio->type_spec), IDL_FLOAT);
  CU_ASSERT_EQ(idl_type(ratio->const_expr), IDL_FLOAT);
  literal = (const idl_literal_t *) ratio->const_expr;
  CU_ASSERT_EQ(literal->value.flt, 1.25f);

  label = idl_next(ratio);
  CU_ASSERT_NEQ_FATAL(label, NULL);
  CU_ASSERT_FATAL(idl_is_const(label));
  CU_ASSERT_STREQ(idl_identifier(label), "LABEL");
  CU_ASSERT_FATAL(idl_is_bounded_string(label->type_spec) ||
                  idl_is_unbounded_string(label->type_spec));
  CU_ASSERT_EQ(idl_type(label->const_expr), IDL_STRING);
  literal = (const idl_literal_t *) label->const_expr;
  CU_ASSERT_STREQ(literal->value.str, "abcd");
  CU_ASSERT_EQ(idl_next(label), NULL);

  idl_delete_pstate(pstate);
}

CU_Test(idl_hand_parser, const_declaration_used_in_bounds_and_labels)
{
  idl_pstate_t *pstate;
  idl_const_t *width;
  idl_const_t *height;
  idl_struct_t *strct;
  idl_member_t *member;
  idl_declarator_t *declarator;
  const idl_literal_t *bound;
  idl_sequence_t *sequence;
  idl_union_t *union_node;
  const char str[] =
    "const unsigned long WIDTH = 1 + 2;"
    "const unsigned long HEIGHT = WIDTH << 1;"
    "struct Sample {"
    "  long matrix[WIDTH][HEIGHT];"
    "  sequence<char, WIDTH + 1> bytes;"
    "};"
    "union Choice switch(long) {"
    "  case HEIGHT: char selected;"
    "  default: char fallback;"
    "};";

  pstate = parse_string(str);
  width = (idl_const_t *) pstate->root;
  CU_ASSERT_NEQ_FATAL(width, NULL);
  CU_ASSERT_FATAL(idl_is_const(width));
  CU_ASSERT_STREQ(idl_identifier(width), "WIDTH");
  CU_ASSERT_EQ(((const idl_literal_t *) width->const_expr)->value.uint32, 3u);

  height = idl_next(width);
  CU_ASSERT_NEQ_FATAL(height, NULL);
  CU_ASSERT_FATAL(idl_is_const(height));
  CU_ASSERT_STREQ(idl_identifier(height), "HEIGHT");
  CU_ASSERT_EQ(((const idl_literal_t *) height->const_expr)->value.uint32, 6u);

  strct = idl_next(height);
  CU_ASSERT_NEQ_FATAL(strct, NULL);
  CU_ASSERT_FATAL(idl_is_struct(strct));
  member = strct->members;
  CU_ASSERT_NEQ_FATAL(member, NULL);
  declarator = member->declarators;
  CU_ASSERT_NEQ_FATAL(declarator, NULL);
  CU_ASSERT_FATAL(idl_is_array(declarator));
  bound = declarator->const_expr;
  CU_ASSERT_NEQ_FATAL(bound, NULL);
  CU_ASSERT_EQ(bound->value.uint32, 3u);
  bound = idl_next(bound);
  CU_ASSERT_NEQ_FATAL(bound, NULL);
  CU_ASSERT_EQ(bound->value.uint32, 6u);
  CU_ASSERT_EQ(idl_next(bound), NULL);

  member = idl_next(member);
  CU_ASSERT_NEQ_FATAL(member, NULL);
  CU_ASSERT_FATAL(idl_is_sequence(member->type_spec));
  sequence = (idl_sequence_t *) member->type_spec;
  CU_ASSERT_EQ(idl_bound(sequence), 4u);

  union_node = idl_next(strct);
  CU_ASSERT_NEQ_FATAL(union_node, NULL);
  CU_ASSERT_FATAL(idl_is_union(union_node));
  CU_ASSERT_EQ(idl_case_label_intvalue(union_node->cases->labels), 6);
  CU_ASSERT_STREQ(idl_identifier(union_node->cases->declarator), "selected");

  idl_delete_pstate(pstate);
}

CU_Test(idl_hand_parser, const_declaration_with_char_and_boolean_labels)
{
  idl_pstate_t *pstate;
  idl_const_t *mark;
  idl_const_t *flag;
  idl_union_t *char_union;
  idl_union_t *bool_union;
  const char str[] =
    "const char MARK = 'x';"
    "const boolean FLAG = true;"
    "union CharChoice switch(char) {"
    "  case MARK: long selected;"
    "  default: long fallback;"
    "};"
    "union BoolChoice switch(boolean) {"
    "  case FLAG: long yes;"
    "  default: long no;"
    "};";

  pstate = parse_string(str);
  mark = (idl_const_t *) pstate->root;
  CU_ASSERT_NEQ_FATAL(mark, NULL);
  CU_ASSERT_FATAL(idl_is_const(mark));
  CU_ASSERT_STREQ(idl_identifier(mark), "MARK");
  CU_ASSERT_EQ(idl_type(mark->const_expr), IDL_CHAR);

  flag = idl_next(mark);
  CU_ASSERT_NEQ_FATAL(flag, NULL);
  CU_ASSERT_FATAL(idl_is_const(flag));
  CU_ASSERT_STREQ(idl_identifier(flag), "FLAG");
  CU_ASSERT_EQ(idl_type(flag->const_expr), IDL_BOOL);

  char_union = idl_next(flag);
  CU_ASSERT_NEQ_FATAL(char_union, NULL);
  CU_ASSERT_FATAL(idl_is_union(char_union));
  CU_ASSERT_EQ(
    ((const idl_literal_t *) char_union->cases->labels->const_expr)->value.chr,
    'x');
  CU_ASSERT_STREQ(idl_identifier(char_union->cases->declarator), "selected");

  bool_union = idl_next(char_union);
  CU_ASSERT_NEQ_FATAL(bool_union, NULL);
  CU_ASSERT_FATAL(idl_is_union(bool_union));
  CU_ASSERT(
    ((const idl_literal_t *) bool_union->cases->labels->const_expr)->value.bln);
  CU_ASSERT_STREQ(idl_identifier(bool_union->cases->declarator), "yes");

  idl_delete_pstate(pstate);
}

CU_Test(idl_hand_parser, const_declaration_with_scoped_alias_types)
{
  idl_pstate_t *pstate;
  idl_typedef_t *count_type;
  idl_typedef_t *label_type;
  idl_const_t *width;
  idl_const_t *label;
  const idl_literal_t *literal;
  const char str[] =
    "typedef unsigned long Count;"
    "typedef string<8> Label;"
    "const Count WIDTH = 5;"
    "const Label NAME = \"sample\";";

  pstate = parse_string(str);
  count_type = (idl_typedef_t *) pstate->root;
  CU_ASSERT_NEQ_FATAL(count_type, NULL);
  CU_ASSERT_FATAL(idl_is_typedef(count_type));
  label_type = idl_next(count_type);
  CU_ASSERT_NEQ_FATAL(label_type, NULL);
  CU_ASSERT_FATAL(idl_is_typedef(label_type));

  width = idl_next(label_type);
  CU_ASSERT_NEQ_FATAL(width, NULL);
  CU_ASSERT_FATAL(idl_is_const(width));
  CU_ASSERT_STREQ(idl_identifier(width), "WIDTH");
  CU_ASSERT_EQ(width->type_spec, count_type->declarators);
  CU_ASSERT_EQ(idl_type(idl_unalias(width->type_spec)), IDL_ULONG);
  literal = (const idl_literal_t *) width->const_expr;
  CU_ASSERT_EQ(idl_type(literal), IDL_ULONG);
  CU_ASSERT_EQ(literal->value.uint32, 5u);

  label = idl_next(width);
  CU_ASSERT_NEQ_FATAL(label, NULL);
  CU_ASSERT_FATAL(idl_is_const(label));
  CU_ASSERT_STREQ(idl_identifier(label), "NAME");
  CU_ASSERT_EQ(label->type_spec, label_type->declarators);
  CU_ASSERT_EQ(idl_type(idl_unalias(label->type_spec)), IDL_STRING);
  literal = (const idl_literal_t *) label->const_expr;
  CU_ASSERT_EQ(idl_type(literal), IDL_STRING);
  CU_ASSERT_STREQ(literal->value.str, "sample");
  CU_ASSERT_EQ(idl_next(label), NULL);

  idl_delete_pstate(pstate);
}

CU_Test(idl_hand_parser, const_declaration_with_enum_constant)
{
  idl_pstate_t *pstate;
  idl_enum_t *color;
  idl_enumerator_t *green;
  idl_typedef_t *shade;
  idl_const_t *pick;
  idl_union_t *choice;
  idl_case_t *case_node;
  const char str[] =
    "enum Color { RED, GREEN, BLUE };"
    "typedef Color Shade;"
    "const Shade PICK = GREEN;"
    "union Choice switch(Color) {"
    "  case PICK: long selected;"
    "  default: long fallback;"
    "};";

  pstate = parse_string(str);
  color = (idl_enum_t *) pstate->root;
  CU_ASSERT_NEQ_FATAL(color, NULL);
  CU_ASSERT_FATAL(idl_is_enum(color));
  green = idl_next(color->enumerators);
  CU_ASSERT_NEQ_FATAL(green, NULL);
  CU_ASSERT_FATAL(idl_is_enumerator(green));

  shade = idl_next(color);
  CU_ASSERT_NEQ_FATAL(shade, NULL);
  CU_ASSERT_FATAL(idl_is_typedef(shade));
  pick = idl_next(shade);
  CU_ASSERT_NEQ_FATAL(pick, NULL);
  CU_ASSERT_FATAL(idl_is_const(pick));
  CU_ASSERT_STREQ(idl_identifier(pick), "PICK");
  CU_ASSERT_EQ(pick->type_spec, shade->declarators);
  CU_ASSERT_EQ(idl_type(idl_unalias(pick->type_spec)), IDL_ENUM);
  CU_ASSERT_EQ(pick->const_expr, green);

  choice = idl_next(pick);
  CU_ASSERT_NEQ_FATAL(choice, NULL);
  CU_ASSERT_FATAL(idl_is_union(choice));
  case_node = choice->cases;
  CU_ASSERT_NEQ_FATAL(case_node, NULL);
  CU_ASSERT_EQ(idl_case_label_intvalue(case_node->labels), 1);
  CU_ASSERT_EQ(case_node->labels->const_expr, green);
  CU_ASSERT_STREQ(idl_identifier(case_node->declarator), "selected");

  idl_delete_pstate(pstate);
}

CU_Test(idl_hand_parser, const_declaration_with_bitmask_constant)
{
  idl_pstate_t *pstate;
  idl_bitmask_t *flags;
  idl_typedef_t *flag_alias;
  idl_const_t *just_a;
  idl_const_t *both;
  idl_union_t *choice;
  idl_case_t *case_node;
  const idl_literal_t *literal;
  const char str[] =
    "bitmask Flags { A, B };"
    "typedef Flags FlagAlias;"
    "const FlagAlias JUST_A = A;"
    "const FlagAlias BOTH = JUST_A | B;"
    "union FlagChoice switch(Flags) {"
    "  case JUST_A: long a;"
    "  case BOTH: long both;"
    "  default: long fallback;"
    "};";

  pstate = parse_string(str);
  flags = (idl_bitmask_t *) pstate->root;
  CU_ASSERT_NEQ_FATAL(flags, NULL);
  CU_ASSERT_FATAL(idl_is_bitmask(flags));
  flag_alias = idl_next(flags);
  CU_ASSERT_NEQ_FATAL(flag_alias, NULL);
  CU_ASSERT_FATAL(idl_is_typedef(flag_alias));

  just_a = idl_next(flag_alias);
  CU_ASSERT_NEQ_FATAL(just_a, NULL);
  CU_ASSERT_FATAL(idl_is_const(just_a));
  CU_ASSERT_STREQ(idl_identifier(just_a), "JUST_A");
  CU_ASSERT_EQ(just_a->type_spec, flag_alias->declarators);
  CU_ASSERT_EQ(idl_type(idl_unalias(just_a->type_spec)), IDL_BITMASK);
  literal = (const idl_literal_t *) just_a->const_expr;
  CU_ASSERT_EQ(idl_type(literal), IDL_BITMASK);
  CU_ASSERT_EQ(literal->value.uint64, 1u);

  both = idl_next(just_a);
  CU_ASSERT_NEQ_FATAL(both, NULL);
  CU_ASSERT_FATAL(idl_is_const(both));
  CU_ASSERT_STREQ(idl_identifier(both), "BOTH");
  CU_ASSERT_EQ(both->type_spec, flag_alias->declarators);
  literal = (const idl_literal_t *) both->const_expr;
  CU_ASSERT_EQ(idl_type(literal), IDL_BITMASK);
  CU_ASSERT_EQ(literal->value.uint64, 3u);

  choice = idl_next(both);
  CU_ASSERT_NEQ_FATAL(choice, NULL);
  CU_ASSERT_FATAL(idl_is_union(choice));
  case_node = choice->cases;
  CU_ASSERT_NEQ_FATAL(case_node, NULL);
  CU_ASSERT_EQ(idl_case_label_intvalue(case_node->labels), 1);
  CU_ASSERT_STREQ(idl_identifier(case_node->declarator), "a");
  case_node = idl_next(case_node);
  CU_ASSERT_NEQ_FATAL(case_node, NULL);
  CU_ASSERT_EQ(idl_case_label_intvalue(case_node->labels), 3);
  CU_ASSERT_STREQ(idl_identifier(case_node->declarator), "both");

  idl_delete_pstate(pstate);
}

CU_Test(idl_hand_parser, annotation_declaration_with_members)
{
  idl_pstate_t *pstate;
  idl_annotation_t *marker;
  idl_annotation_t *tuned;
  idl_annotation_member_t *member;
  idl_typedef_t *alias;
  idl_const_t *limit;
  idl_enum_t *mode;
  idl_bitmask_t *bits;
  const idl_literal_t *literal;
  const char str[] =
    "@annotation marker { };"
    "@annotation tuned {"
    "  long value default 7;"
    "  string label default \"fast\";"
    "  typedef long Alias;"
    "  const Alias LIMIT = 9;"
    "  enum Mode { OFF, ON };"
    "  bitmask Bits { A, B };"
    "};";

  pstate = parse_string_flags(IDL_FLAG_ANNOTATIONS, str);
  marker = (idl_annotation_t *) pstate->root;
  CU_ASSERT_NEQ_FATAL(marker, NULL);
  CU_ASSERT_EQ(idl_mask(marker), IDL_ANNOTATION);
  CU_ASSERT_STREQ(idl_identifier(marker), "marker");
  CU_ASSERT_EQ(marker->definitions, NULL);

  tuned = idl_next(marker);
  CU_ASSERT_NEQ_FATAL(tuned, NULL);
  CU_ASSERT_EQ(idl_mask(tuned), IDL_ANNOTATION);
  CU_ASSERT_STREQ(idl_identifier(tuned), "tuned");

  member = (idl_annotation_member_t *) tuned->definitions;
  CU_ASSERT_NEQ_FATAL(member, NULL);
  CU_ASSERT_FATAL(idl_is_annotation_member(member));
  CU_ASSERT_STREQ(idl_identifier(member->declarator), "value");
  CU_ASSERT_EQ(idl_type(member->type_spec), IDL_LONG);
  literal = (const idl_literal_t *) member->const_expr;
  CU_ASSERT_EQ(idl_type(literal), IDL_LONG);
  CU_ASSERT_EQ(literal->value.int32, 7);

  member = idl_next(member);
  CU_ASSERT_NEQ_FATAL(member, NULL);
  CU_ASSERT_FATAL(idl_is_annotation_member(member));
  CU_ASSERT_STREQ(idl_identifier(member->declarator), "label");
  CU_ASSERT_EQ(idl_type(member->type_spec), IDL_STRING);
  literal = (const idl_literal_t *) member->const_expr;
  CU_ASSERT_EQ(idl_type(literal), IDL_STRING);
  CU_ASSERT_STREQ(literal->value.str, "fast");

  alias = idl_next(member);
  CU_ASSERT_NEQ_FATAL(alias, NULL);
  CU_ASSERT_FATAL(idl_is_typedef(alias));
  CU_ASSERT_STREQ(idl_identifier(alias->declarators), "Alias");

  limit = idl_next(alias);
  CU_ASSERT_NEQ_FATAL(limit, NULL);
  CU_ASSERT_FATAL(idl_is_const(limit));
  CU_ASSERT_STREQ(idl_identifier(limit), "LIMIT");
  CU_ASSERT_EQ(idl_type(limit->const_expr), IDL_LONG);

  mode = idl_next(limit);
  CU_ASSERT_NEQ_FATAL(mode, NULL);
  CU_ASSERT_FATAL(idl_is_enum(mode));
  CU_ASSERT_STREQ(idl_identifier(mode), "Mode");

  bits = idl_next(mode);
  CU_ASSERT_NEQ_FATAL(bits, NULL);
  CU_ASSERT_FATAL(idl_is_bitmask(bits));
  CU_ASSERT_STREQ(idl_identifier(bits), "Bits");
  CU_ASSERT_EQ(idl_next(bits), NULL);
  CU_ASSERT_EQ(idl_next(tuned), NULL);

  idl_delete_pstate(pstate);
}

CU_Test(idl_hand_parser, annotation_application_without_parameters)
{
  idl_pstate_t *pstate;
  idl_annotation_t *marker;
  idl_struct_t *strct;
  idl_annotation_appl_t *appl;
  idl_member_t *member;
  const char str[] =
    "@annotation marker { };"
    "@marker struct Sample {"
    "  @key @marker long id;"
    "  long value;"
    "};";

  pstate = parse_string_flags(IDL_FLAG_ANNOTATIONS, str);
  marker = (idl_annotation_t *) pstate->root;
  CU_ASSERT_NEQ_FATAL(marker, NULL);
  CU_ASSERT_EQ(idl_mask(marker), IDL_ANNOTATION);
  CU_ASSERT_STREQ(idl_identifier(marker), "marker");

  strct = idl_next(marker);
  CU_ASSERT_NEQ_FATAL(strct, NULL);
  CU_ASSERT_FATAL(idl_is_struct(strct));
  appl = strct->node.annotations;
  CU_ASSERT_NEQ_FATAL(appl, NULL);
  CU_ASSERT_FATAL(idl_is_annotation_appl(appl));
  CU_ASSERT_EQ(appl->annotation, marker);
  CU_ASSERT_EQ(appl->parameters, NULL);
  CU_ASSERT_EQ(idl_next(appl), NULL);

  member = strct->members;
  CU_ASSERT_NEQ_FATAL(member, NULL);
  CU_ASSERT_FATAL(idl_is_member(member));
  CU_ASSERT(member->key.value);
  CU_ASSERT_NEQ(member->key.annotation, NULL);
  appl = member->node.annotations;
  CU_ASSERT_NEQ_FATAL(appl, NULL);
  CU_ASSERT_FATAL(idl_is_annotation_appl(appl));
  CU_ASSERT_STREQ(idl_identifier(appl->annotation), "key");
  CU_ASSERT_EQ(appl->parameters, NULL);
  appl = idl_next(appl);
  CU_ASSERT_NEQ_FATAL(appl, NULL);
  CU_ASSERT_FATAL(idl_is_annotation_appl(appl));
  CU_ASSERT_EQ(appl->annotation, marker);
  CU_ASSERT_EQ(appl->parameters, NULL);
  CU_ASSERT_EQ(idl_next(appl), NULL);

  member = idl_next(member);
  CU_ASSERT_NEQ_FATAL(member, NULL);
  CU_ASSERT_FATAL(idl_is_member(member));
  CU_ASSERT(!member->key.value);
  CU_ASSERT_EQ(member->node.annotations, NULL);
  CU_ASSERT_EQ(idl_next(member), NULL);
  CU_ASSERT_EQ(idl_next(strct), NULL);

  idl_delete_pstate(pstate);
}

CU_Test(idl_hand_parser, annotation_application_on_type_positions)
{
  idl_pstate_t *pstate;
  idl_annotation_t *marker;
  idl_struct_t *holder;
  idl_member_t *member;
  idl_sequence_t *sequence;
  idl_union_t *choice;
  idl_switch_type_spec_t *switch_type_spec;
  idl_case_t *case_node;
  idl_annotation_appl_t *appl;
  const char str[] =
    "@annotation marker { };"
    "struct Holder { sequence<@marker long> values; };"
    "@marker union Choice switch (@key @marker long) {"
    "  case 0: @marker sequence<@marker long> branch;"
    "};";

  pstate = parse_string_flags(IDL_FLAG_ANNOTATIONS, str);
  marker = (idl_annotation_t *) pstate->root;
  CU_ASSERT_NEQ_FATAL(marker, NULL);
  CU_ASSERT_EQ(idl_mask(marker), IDL_ANNOTATION);

  holder = idl_next(marker);
  CU_ASSERT_NEQ_FATAL(holder, NULL);
  CU_ASSERT_FATAL(idl_is_struct(holder));
  member = holder->members;
  CU_ASSERT_NEQ_FATAL(member, NULL);
  CU_ASSERT_FATAL(idl_is_sequence(member->type_spec));
  sequence = (idl_sequence_t *) member->type_spec;
  appl = sequence->node.annotations;
  CU_ASSERT_NEQ_FATAL(appl, NULL);
  CU_ASSERT_FATAL(idl_is_annotation_appl(appl));
  CU_ASSERT_EQ(appl->annotation, marker);
  CU_ASSERT_EQ(idl_next(appl), NULL);

  choice = idl_next(holder);
  CU_ASSERT_NEQ_FATAL(choice, NULL);
  CU_ASSERT_FATAL(idl_is_union(choice));
  appl = choice->node.annotations;
  CU_ASSERT_NEQ_FATAL(appl, NULL);
  CU_ASSERT_EQ(appl->annotation, marker);
  CU_ASSERT_EQ(idl_next(appl), NULL);

  switch_type_spec = choice->switch_type_spec;
  CU_ASSERT_NEQ_FATAL(switch_type_spec, NULL);
  CU_ASSERT(switch_type_spec->key.value);
  CU_ASSERT_NEQ(switch_type_spec->key.annotation, NULL);
  appl = switch_type_spec->node.annotations;
  CU_ASSERT_NEQ_FATAL(appl, NULL);
  CU_ASSERT_STREQ(idl_identifier(appl->annotation), "key");
  appl = idl_next(appl);
  CU_ASSERT_NEQ_FATAL(appl, NULL);
  CU_ASSERT_EQ(appl->annotation, marker);
  CU_ASSERT_EQ(idl_next(appl), NULL);

  case_node = choice->cases;
  CU_ASSERT_NEQ_FATAL(case_node, NULL);
  CU_ASSERT_FATAL(idl_is_case(case_node));
  appl = case_node->node.annotations;
  CU_ASSERT_NEQ_FATAL(appl, NULL);
  CU_ASSERT_EQ(appl->annotation, marker);
  CU_ASSERT_EQ(idl_next(appl), NULL);
  CU_ASSERT_FATAL(idl_is_sequence(case_node->type_spec));
  sequence = (idl_sequence_t *) case_node->type_spec;
  appl = sequence->node.annotations;
  CU_ASSERT_NEQ_FATAL(appl, NULL);
  CU_ASSERT_EQ(appl->annotation, marker);
  CU_ASSERT_EQ(idl_next(appl), NULL);
  CU_ASSERT_EQ(idl_next(case_node), NULL);
  CU_ASSERT_EQ(idl_next(choice), NULL);

  idl_delete_pstate(pstate);
}

CU_Test(idl_hand_parser, annotation_application_on_enum_and_bit_values)
{
  idl_pstate_t *pstate;
  idl_annotation_t *marker;
  idl_annotation_t *catalog;
  idl_enum_t *mode;
  idl_enumerator_t *enumerator;
  idl_bitmask_t *bits;
  idl_bit_value_t *bit_value;
  idl_typedef_t *alias;
  idl_sequence_t *sequence;
  idl_annotation_appl_t *appl;
  const char str[] =
    "@annotation marker { };"
    "@annotation catalog {"
    "  enum Mode { @marker OFF, @marker ON };"
    "  bitmask Bits { @marker A, @marker B };"
    "  typedef sequence<@marker long> Seq;"
    "};";

  pstate = parse_string_flags(IDL_FLAG_ANNOTATIONS, str);
  marker = (idl_annotation_t *) pstate->root;
  CU_ASSERT_NEQ_FATAL(marker, NULL);
  CU_ASSERT_EQ(idl_mask(marker), IDL_ANNOTATION);

  catalog = idl_next(marker);
  CU_ASSERT_NEQ_FATAL(catalog, NULL);
  CU_ASSERT_EQ(idl_mask(catalog), IDL_ANNOTATION);

  mode = (idl_enum_t *) catalog->definitions;
  CU_ASSERT_NEQ_FATAL(mode, NULL);
  CU_ASSERT_FATAL(idl_is_enum(mode));
  enumerator = mode->enumerators;
  CU_ASSERT_NEQ_FATAL(enumerator, NULL);
  CU_ASSERT_STREQ(idl_identifier(enumerator), "OFF");
  appl = enumerator->node.annotations;
  CU_ASSERT_NEQ_FATAL(appl, NULL);
  CU_ASSERT_EQ(appl->annotation, marker);
  CU_ASSERT_EQ(idl_next(appl), NULL);
  enumerator = idl_next(enumerator);
  CU_ASSERT_NEQ_FATAL(enumerator, NULL);
  CU_ASSERT_STREQ(idl_identifier(enumerator), "ON");
  appl = enumerator->node.annotations;
  CU_ASSERT_NEQ_FATAL(appl, NULL);
  CU_ASSERT_EQ(appl->annotation, marker);
  CU_ASSERT_EQ(idl_next(appl), NULL);
  CU_ASSERT_EQ(idl_next(enumerator), NULL);

  bits = idl_next(mode);
  CU_ASSERT_NEQ_FATAL(bits, NULL);
  CU_ASSERT_FATAL(idl_is_bitmask(bits));
  bit_value = bits->bit_values;
  CU_ASSERT_NEQ_FATAL(bit_value, NULL);
  CU_ASSERT_STREQ(idl_identifier(bit_value), "A");
  appl = bit_value->node.annotations;
  CU_ASSERT_NEQ_FATAL(appl, NULL);
  CU_ASSERT_EQ(appl->annotation, marker);
  CU_ASSERT_EQ(idl_next(appl), NULL);
  bit_value = idl_next(bit_value);
  CU_ASSERT_NEQ_FATAL(bit_value, NULL);
  CU_ASSERT_STREQ(idl_identifier(bit_value), "B");
  appl = bit_value->node.annotations;
  CU_ASSERT_NEQ_FATAL(appl, NULL);
  CU_ASSERT_EQ(appl->annotation, marker);
  CU_ASSERT_EQ(idl_next(appl), NULL);
  CU_ASSERT_EQ(idl_next(bit_value), NULL);

  alias = idl_next(bits);
  CU_ASSERT_NEQ_FATAL(alias, NULL);
  CU_ASSERT_FATAL(idl_is_typedef(alias));
  CU_ASSERT_FATAL(idl_is_sequence(alias->type_spec));
  sequence = (idl_sequence_t *) alias->type_spec;
  appl = sequence->node.annotations;
  CU_ASSERT_NEQ_FATAL(appl, NULL);
  CU_ASSERT_EQ(appl->annotation, marker);
  CU_ASSERT_EQ(idl_next(appl), NULL);
  CU_ASSERT_EQ(idl_next(alias), NULL);
  CU_ASSERT_EQ(idl_next(catalog), NULL);

  idl_delete_pstate(pstate);
}

CU_Test(idl_hand_parser, annotation_application_positional_parameters)
{
  idl_pstate_t *pstate;
  idl_annotation_t *marker;
  idl_annotation_member_t *annotation_member;
  idl_struct_t *sample;
  idl_struct_t *ordered;
  idl_bitmask_t *bits;
  idl_enum_t *mode;
  idl_member_t *member;
  idl_declarator_t *declarator;
  idl_sequence_t *sequence;
  idl_bit_value_t *bit_value;
  idl_enumerator_t *enumerator;
  idl_annotation_appl_t *appl;
  idl_annotation_appl_param_t *param;
  idl_literal_t *literal;
  const char str[] =
    "@annotation marker { long value; };"
    "@marker(42) struct Sample {"
    "  @key(false) @hashid(\"wire\") long id;"
    "  sequence<@try_construct(TRIM) string<5> > names;"
    "};"
    "@autoid(SEQUENTIAL) struct Ordered { long a; long b; };"
    "@bit_bound(8) bitmask Bits { @position(3) A, B };"
    "enum Mode { @value(5) OFF, ON };";

  pstate = parse_string_flags(IDL_FLAG_ANNOTATIONS, str);
  marker = (idl_annotation_t *) pstate->root;
  CU_ASSERT_NEQ_FATAL(marker, NULL);
  CU_ASSERT_EQ(idl_mask(marker), IDL_ANNOTATION);
  annotation_member = (idl_annotation_member_t *) marker->definitions;
  CU_ASSERT_NEQ_FATAL(annotation_member, NULL);
  CU_ASSERT_FATAL(idl_is_annotation_member(annotation_member));

  sample = idl_next(marker);
  CU_ASSERT_NEQ_FATAL(sample, NULL);
  CU_ASSERT_FATAL(idl_is_struct(sample));
  appl = sample->node.annotations;
  CU_ASSERT_NEQ_FATAL(appl, NULL);
  CU_ASSERT_EQ(appl->annotation, marker);
  param = appl->parameters;
  CU_ASSERT_NEQ_FATAL(param, NULL);
  CU_ASSERT_EQ(param->member, annotation_member);
  CU_ASSERT_EQ(idl_next(param), NULL);
  literal = (idl_literal_t *) param->const_expr;
  CU_ASSERT_EQ(idl_type(literal), IDL_LONG);
  CU_ASSERT_EQ(literal->value.int32, 42);

  member = sample->members;
  CU_ASSERT_NEQ_FATAL(member, NULL);
  CU_ASSERT(!member->key.value);
  CU_ASSERT_NEQ(member->key.annotation, NULL);
  declarator = member->declarators;
  CU_ASSERT_NEQ_FATAL(declarator, NULL);
  CU_ASSERT_NEQ(declarator->id.annotation, NULL);
  CU_ASSERT_NEQ(declarator->id.value, 0u);
  appl = member->node.annotations;
  CU_ASSERT_NEQ_FATAL(appl, NULL);
  CU_ASSERT_STREQ(idl_identifier(appl->annotation), "key");
  param = appl->parameters;
  CU_ASSERT_NEQ_FATAL(param, NULL);
  literal = (idl_literal_t *) param->const_expr;
  CU_ASSERT_EQ(idl_type(literal), IDL_BOOL);
  CU_ASSERT(!literal->value.bln);
  appl = idl_next(appl);
  CU_ASSERT_NEQ_FATAL(appl, NULL);
  CU_ASSERT_STREQ(idl_identifier(appl->annotation), "hashid");
  param = appl->parameters;
  CU_ASSERT_NEQ_FATAL(param, NULL);
  literal = (idl_literal_t *) param->const_expr;
  CU_ASSERT_EQ(idl_type(literal), IDL_STRING);
  CU_ASSERT_STREQ(literal->value.str, "wire");
  CU_ASSERT_EQ(idl_next(appl), NULL);

  member = idl_next(member);
  CU_ASSERT_NEQ_FATAL(member, NULL);
  CU_ASSERT_FATAL(idl_is_sequence(member->type_spec));
  sequence = (idl_sequence_t *) member->type_spec;
  CU_ASSERT_EQ(sequence->elem_try_construct.value, IDL_TRIM);
  CU_ASSERT_NEQ(sequence->elem_try_construct.annotation, NULL);
  appl = sequence->node.annotations;
  CU_ASSERT_NEQ_FATAL(appl, NULL);
  CU_ASSERT_STREQ(idl_identifier(appl->annotation), "try_construct");
  param = appl->parameters;
  CU_ASSERT_NEQ_FATAL(param, NULL);
  CU_ASSERT_STREQ(idl_identifier(param->const_expr), "TRIM");
  CU_ASSERT_EQ(idl_next(member), NULL);

  ordered = idl_next(sample);
  CU_ASSERT_NEQ_FATAL(ordered, NULL);
  CU_ASSERT_FATAL(idl_is_struct(ordered));
  CU_ASSERT_EQ(ordered->autoid.value, IDL_SEQUENTIAL);
  CU_ASSERT_NEQ(ordered->autoid.annotation, NULL);

  bits = idl_next(ordered);
  CU_ASSERT_NEQ_FATAL(bits, NULL);
  CU_ASSERT_FATAL(idl_is_bitmask(bits));
  CU_ASSERT_EQ(bits->bit_bound.value, 8);
  CU_ASSERT_NEQ(bits->bit_bound.annotation, NULL);
  bit_value = bits->bit_values;
  CU_ASSERT_NEQ_FATAL(bit_value, NULL);
  CU_ASSERT_EQ(bit_value->position.value, 3);
  CU_ASSERT_NEQ(bit_value->position.annotation, NULL);
  bit_value = idl_next(bit_value);
  CU_ASSERT_NEQ_FATAL(bit_value, NULL);
  CU_ASSERT_EQ(bit_value->position.value, 4);
  CU_ASSERT_EQ(idl_next(bit_value), NULL);

  mode = idl_next(bits);
  CU_ASSERT_NEQ_FATAL(mode, NULL);
  CU_ASSERT_FATAL(idl_is_enum(mode));
  enumerator = mode->enumerators;
  CU_ASSERT_NEQ_FATAL(enumerator, NULL);
  CU_ASSERT_EQ(enumerator->value.value, 5);
  CU_ASSERT_NEQ(enumerator->value.annotation, NULL);
  enumerator = idl_next(enumerator);
  CU_ASSERT_NEQ_FATAL(enumerator, NULL);
  CU_ASSERT_EQ(enumerator->value.value, 6);
  CU_ASSERT_EQ(idl_next(enumerator), NULL);
  CU_ASSERT_EQ(idl_next(mode), NULL);

  idl_delete_pstate(pstate);
}

CU_Test(idl_hand_parser, unknown_annotation_positional_parameter)
{
  idl_pstate_t *pstate;
  idl_struct_t *strct;
  const char str[] =
    "@unknown(foo + 1) struct Loose { };";

  pstate = parse_string_flags(IDL_FLAG_ANNOTATIONS, str);
  strct = (idl_struct_t *) pstate->root;
  CU_ASSERT_NEQ_FATAL(strct, NULL);
  CU_ASSERT_FATAL(idl_is_struct(strct));
  CU_ASSERT_EQ(strct->node.annotations, NULL);
  CU_ASSERT_EQ(idl_next(strct), NULL);

  idl_delete_pstate(pstate);
}

CU_Test(idl_hand_parser, annotation_application_keyword_parameters)
{
  idl_pstate_t *pstate;
  idl_annotation_t *knobs;
  idl_annotation_member_t *low;
  idl_annotation_member_t *high;
  idl_struct_t *window;
  idl_member_t *member;
  idl_annotation_appl_t *appl;
  idl_annotation_appl_param_t *param;
  idl_literal_t *literal;
  const char str[] =
    "@annotation knobs {"
    "  long low;"
    "  long high;"
    "  string label default \"steady\";"
    "};"
    "@knobs(high = 9, low = 2) struct Window {"
    "  @range(min = 1, max = 10) long value;"
    "};";

  pstate = parse_string_flags(IDL_FLAG_ANNOTATIONS, str);
  knobs = (idl_annotation_t *) pstate->root;
  CU_ASSERT_NEQ_FATAL(knobs, NULL);
  CU_ASSERT_EQ(idl_mask(knobs), IDL_ANNOTATION);
  low = (idl_annotation_member_t *) knobs->definitions;
  CU_ASSERT_NEQ_FATAL(low, NULL);
  CU_ASSERT_FATAL(idl_is_annotation_member(low));
  high = idl_next(low);
  CU_ASSERT_NEQ_FATAL(high, NULL);
  CU_ASSERT_FATAL(idl_is_annotation_member(high));

  window = idl_next(knobs);
  CU_ASSERT_NEQ_FATAL(window, NULL);
  CU_ASSERT_FATAL(idl_is_struct(window));
  appl = window->node.annotations;
  CU_ASSERT_NEQ_FATAL(appl, NULL);
  CU_ASSERT_EQ(appl->annotation, knobs);
  param = appl->parameters;
  CU_ASSERT_NEQ_FATAL(param, NULL);
  CU_ASSERT_EQ(param->member, high);
  literal = (idl_literal_t *) param->const_expr;
  CU_ASSERT_EQ(idl_type(literal), IDL_LONG);
  CU_ASSERT_EQ(literal->value.int32, 9);
  param = idl_next(param);
  CU_ASSERT_NEQ_FATAL(param, NULL);
  CU_ASSERT_EQ(param->member, low);
  literal = (idl_literal_t *) param->const_expr;
  CU_ASSERT_EQ(idl_type(literal), IDL_LONG);
  CU_ASSERT_EQ(literal->value.int32, 2);
  CU_ASSERT_EQ(idl_next(param), NULL);
  CU_ASSERT_EQ(idl_next(appl), NULL);

  member = window->members;
  CU_ASSERT_NEQ_FATAL(member, NULL);
  CU_ASSERT_NEQ(member->min.annotation, NULL);
  literal = (idl_literal_t *) member->min.value;
  CU_ASSERT_EQ(idl_type(literal), IDL_LONG);
  CU_ASSERT_EQ(literal->value.int32, 1);
  CU_ASSERT_NEQ(member->max.annotation, NULL);
  literal = (idl_literal_t *) member->max.value;
  CU_ASSERT_EQ(idl_type(literal), IDL_LONG);
  CU_ASSERT_EQ(literal->value.int32, 10);
  appl = member->node.annotations;
  CU_ASSERT_NEQ_FATAL(appl, NULL);
  CU_ASSERT_STREQ(idl_identifier(appl->annotation), "range");
  param = appl->parameters;
  CU_ASSERT_NEQ_FATAL(param, NULL);
  CU_ASSERT_STREQ(idl_identifier(param->member->declarator), "min");
  param = idl_next(param);
  CU_ASSERT_NEQ_FATAL(param, NULL);
  CU_ASSERT_STREQ(idl_identifier(param->member->declarator), "max");
  CU_ASSERT_EQ(idl_next(param), NULL);
  CU_ASSERT_EQ(idl_next(member), NULL);
  CU_ASSERT_EQ(idl_next(window), NULL);

  idl_delete_pstate(pstate);
}

CU_Test(idl_hand_parser, unknown_annotation_keyword_parameters)
{
  idl_pstate_t *pstate;
  idl_struct_t *strct;
  const char str[] =
    "@unknown(foo = bar + 1, baz = \"x\") struct LooseKeywords { };";

  pstate = parse_string_flags(IDL_FLAG_ANNOTATIONS, str);
  strct = (idl_struct_t *) pstate->root;
  CU_ASSERT_NEQ_FATAL(strct, NULL);
  CU_ASSERT_FATAL(idl_is_struct(strct));
  CU_ASSERT_EQ(strct->node.annotations, NULL);
  CU_ASSERT_EQ(idl_next(strct), NULL);

  idl_delete_pstate(pstate);
}

CU_Test(idl_hand_parser, annotation_application_parameter_scope)
{
  idl_pstate_t *pstate;
  idl_const_t *global;
  idl_module_t *outer;
  idl_const_t *local;
  idl_annotation_t *mark;
  idl_struct_t *positional;
  idl_struct_t *keyword;
  idl_annotation_appl_t *appl;
  idl_annotation_appl_param_t *param;
  const idl_literal_t *literal;
  const char str[] =
    "const long GLOBAL = 1;"
    "module outer {"
    "  const long LOCAL = 2;"
    "  @annotation mark { long value; };"
    "  @mark(LOCAL) struct Positional { long f; };"
    "  @mark(value = ::GLOBAL) struct Keyword { long f; };"
    "};";

  pstate = parse_string_flags(IDL_FLAG_ANNOTATIONS, str);
  global = (idl_const_t *) pstate->root;
  CU_ASSERT_NEQ_FATAL(global, NULL);
  CU_ASSERT_FATAL(idl_is_const(global));

  outer = idl_next(global);
  CU_ASSERT_NEQ_FATAL(outer, NULL);
  CU_ASSERT_FATAL(idl_is_module(outer));
  local = (idl_const_t *) outer->definitions;
  CU_ASSERT_NEQ_FATAL(local, NULL);
  CU_ASSERT_FATAL(idl_is_const(local));
  mark = idl_next(local);
  CU_ASSERT_NEQ_FATAL(mark, NULL);
  CU_ASSERT_EQ(idl_mask(mark), IDL_ANNOTATION);

  positional = idl_next(mark);
  CU_ASSERT_NEQ_FATAL(positional, NULL);
  CU_ASSERT_FATAL(idl_is_struct(positional));
  appl = positional->node.annotations;
  CU_ASSERT_NEQ_FATAL(appl, NULL);
  CU_ASSERT_EQ(appl->annotation, mark);
  param = appl->parameters;
  CU_ASSERT_NEQ_FATAL(param, NULL);
  literal = (const idl_literal_t *) param->const_expr;
  CU_ASSERT_EQ(idl_type(literal), IDL_LONG);
  CU_ASSERT_EQ(literal->value.int32, 2);

  keyword = idl_next(positional);
  CU_ASSERT_NEQ_FATAL(keyword, NULL);
  CU_ASSERT_FATAL(idl_is_struct(keyword));
  appl = keyword->node.annotations;
  CU_ASSERT_NEQ_FATAL(appl, NULL);
  CU_ASSERT_EQ(appl->annotation, mark);
  param = appl->parameters;
  CU_ASSERT_NEQ_FATAL(param, NULL);
  CU_ASSERT_STREQ(idl_identifier(param->member->declarator), "value");
  literal = (const idl_literal_t *) param->const_expr;
  CU_ASSERT_EQ(idl_type(literal), IDL_LONG);
  CU_ASSERT_EQ(literal->value.int32, 1);
  CU_ASSERT_EQ(idl_next(param), NULL);
  CU_ASSERT_EQ(idl_next(keyword), NULL);
  CU_ASSERT_EQ(idl_next(outer), NULL);

  idl_delete_pstate(pstate);
}

CU_Test(idl_hand_parser, annotation_application_rejects_parameter_on_empty_annotation)
{
  expect_parse_ret_flags(
    IDL_FLAG_ANNOTATIONS,
    "@annotation marker { };"
    "@marker(1) struct Sample { };",
    IDL_RETCODE_SEMANTIC_ERROR);
}

CU_Test(idl_hand_parser, annotation_application_rejects_empty_parameter_list)
{
  expect_parse_ret_flags(
    IDL_FLAG_ANNOTATIONS,
    "@annotation marker { long value; };"
    "@marker() struct Sample { };",
    IDL_RETCODE_SYNTAX_ERROR);
}

CU_Test(idl_hand_parser, annotation_application_rejects_keyword_then_positional_parameter)
{
  expect_parse_ret_flags(
    IDL_FLAG_ANNOTATIONS,
    "@annotation marker { long value; };"
    "@marker(value = 1, 2) struct Sample { };",
    IDL_RETCODE_SYNTAX_ERROR);
}

CU_Test(idl_hand_parser, annotation_application_rejects_trailing_keyword_name)
{
  expect_parse_ret_flags(
    IDL_FLAG_ANNOTATIONS,
    "@annotation marker { long value; };"
    "@marker(value = 1, extra) struct Sample { };",
    IDL_RETCODE_SEMANTIC_ERROR);
}

CU_Test(idl_hand_parser, annotation_application_rejects_unknown_keyword_member)
{
  expect_parse_ret_flags(
    IDL_FLAG_ANNOTATIONS,
    "@annotation marker { long value; };"
    "@marker(missing = 1) struct Sample { };",
    IDL_RETCODE_SEMANTIC_ERROR);
}

CU_Test(idl_hand_parser, annotation_application_rejects_missing_keyword_value)
{
  expect_parse_ret_flags(
    IDL_FLAG_ANNOTATIONS,
    "@annotation marker { long value; };"
    "@marker(value = ) struct Sample { };",
    IDL_RETCODE_SYNTAX_ERROR);
}

CU_Test(idl_hand_parser, annotation_application_rejects_positional_then_keyword_parameter)
{
  expect_parse_ret_flags(
    IDL_FLAG_ANNOTATIONS,
    "@annotation marker { long value; };"
    "@marker(1, value = 2) struct Sample { };",
    IDL_RETCODE_SYNTAX_ERROR);
}

CU_Test(idl_hand_parser, unknown_annotation_rejects_missing_keyword_value)
{
  expect_parse_ret_flags(
    IDL_FLAG_ANNOTATIONS,
    "@unknown(foo =) struct Loose { };",
    IDL_RETCODE_SYNTAX_ERROR);
}

CU_Test(idl_hand_parser, unknown_annotation_rejects_incomplete_expression)
{
  expect_parse_ret_flags(
    IDL_FLAG_ANNOTATIONS,
    "@unknown(foo = bar +) struct Loose { };",
    IDL_RETCODE_SYNTAX_ERROR);
}

CU_Test(idl_hand_parser, unknown_annotation_rejects_bad_positional_start)
{
  expect_parse_ret_flags(
    IDL_FLAG_ANNOTATIONS,
    "@unknown(,) struct Loose { };",
    IDL_RETCODE_SYNTAX_ERROR);
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

CU_Test(idl_hand_parser, struct_with_adjacent_nested_sequence_closers)
{
  idl_pstate_t *pstate;
  idl_struct_t *strct;
  idl_member_t *member;
  idl_sequence_t *sequence;
  const char str[] =
    "struct Samples {"
    "  sequence<sequence<long>> values;"
    "  sequence<sequence<sequence<char>>> deeply_nested;"
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
  CU_ASSERT_FATAL(idl_is_sequence(sequence->type_spec));
  sequence = (idl_sequence_t *) sequence->type_spec;
  CU_ASSERT_EQ(idl_bound(sequence), 0u);
  CU_ASSERT_EQ(idl_type(sequence->type_spec), IDL_LONG);
  CU_ASSERT_STREQ(idl_identifier(member->declarators), "values");

  member = idl_next(member);
  CU_ASSERT_NEQ_FATAL(member, NULL);
  CU_ASSERT_FATAL(idl_is_sequence(member->type_spec));
  sequence = (idl_sequence_t *) member->type_spec;
  CU_ASSERT_FATAL(idl_is_sequence(sequence->type_spec));
  sequence = (idl_sequence_t *) sequence->type_spec;
  CU_ASSERT_FATAL(idl_is_sequence(sequence->type_spec));
  sequence = (idl_sequence_t *) sequence->type_spec;
  CU_ASSERT_EQ(idl_type(sequence->type_spec), IDL_CHAR);
  CU_ASSERT_STREQ(idl_identifier(member->declarators), "deeply_nested");
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

CU_Test(idl_hand_parser, enum_with_enumerators)
{
  idl_pstate_t *pstate;
  idl_enum_t *enum_node;
  idl_enumerator_t *enumerator;
  idl_struct_t *strct;
  idl_member_t *member;
  const char str[] =
    "enum Color { RED, GREEN, BLUE };"
    "struct Pixel { Color color; };";

  pstate = parse_string(str);
  enum_node = (idl_enum_t *) pstate->root;
  CU_ASSERT_NEQ_FATAL(enum_node, NULL);
  CU_ASSERT_FATAL(idl_is_enum(enum_node));
  CU_ASSERT_STREQ(idl_identifier(enum_node), "Color");
  CU_ASSERT_EQ(idl_parent(enum_node), NULL);

  enumerator = enum_node->enumerators;
  CU_ASSERT_NEQ_FATAL(enumerator, NULL);
  CU_ASSERT_FATAL(idl_is_enumerator(enumerator));
  CU_ASSERT_STREQ(idl_identifier(enumerator), "RED");
  CU_ASSERT_EQ(enumerator->value.value, 0);
  CU_ASSERT_EQ(idl_parent(enumerator), enum_node);
  CU_ASSERT_EQ(enum_node->default_enumerator, enumerator);

  enumerator = idl_next(enumerator);
  CU_ASSERT_NEQ_FATAL(enumerator, NULL);
  CU_ASSERT_FATAL(idl_is_enumerator(enumerator));
  CU_ASSERT_STREQ(idl_identifier(enumerator), "GREEN");
  CU_ASSERT_EQ(enumerator->value.value, 1);
  CU_ASSERT_EQ(idl_parent(enumerator), enum_node);

  enumerator = idl_next(enumerator);
  CU_ASSERT_NEQ_FATAL(enumerator, NULL);
  CU_ASSERT_FATAL(idl_is_enumerator(enumerator));
  CU_ASSERT_STREQ(idl_identifier(enumerator), "BLUE");
  CU_ASSERT_EQ(enumerator->value.value, 2);
  CU_ASSERT_EQ(idl_parent(enumerator), enum_node);
  CU_ASSERT_EQ(idl_next(enumerator), NULL);

  strct = idl_next(enum_node);
  CU_ASSERT_NEQ_FATAL(strct, NULL);
  CU_ASSERT_FATAL(idl_is_struct(strct));
  member = strct->members;
  CU_ASSERT_NEQ_FATAL(member, NULL);
  CU_ASSERT_EQ(member->type_spec, enum_node);
  CU_ASSERT_STREQ(idl_identifier(member->declarators), "color");

  idl_delete_pstate(pstate);
}

CU_Test(idl_hand_parser, enum_rejects_empty_body)
{
  expect_parse_ret("enum Empty { };", IDL_RETCODE_SYNTAX_ERROR);
}

CU_Test(idl_hand_parser, enum_rejects_duplicate_enumerators)
{
  expect_parse_ret("enum Color { RED, RED };", IDL_RETCODE_SEMANTIC_ERROR);
}

CU_Test(idl_hand_parser, enum_rejects_enumerator_matching_enum)
{
  expect_parse_ret("enum Color { Color };", IDL_RETCODE_SEMANTIC_ERROR);
}

CU_Test(idl_hand_parser, bitmask_with_bit_values)
{
  idl_pstate_t *pstate;
  idl_bitmask_t *bitmask;
  idl_bit_value_t *bit_value;
  idl_struct_t *strct;
  idl_member_t *member;
  const char str[] =
    "bitmask Permissions { READ, WRITE, EXECUTE };"
    "struct Access { Permissions permissions; };";

  pstate = parse_string(str);
  bitmask = (idl_bitmask_t *) pstate->root;
  CU_ASSERT_NEQ_FATAL(bitmask, NULL);
  CU_ASSERT_FATAL(idl_is_bitmask(bitmask));
  CU_ASSERT_STREQ(idl_identifier(bitmask), "Permissions");
  CU_ASSERT_EQ(bitmask->bit_bound.value, 32u);

  bit_value = bitmask->bit_values;
  CU_ASSERT_NEQ_FATAL(bit_value, NULL);
  CU_ASSERT_FATAL(idl_is_bit_value(bit_value));
  CU_ASSERT_STREQ(idl_identifier(bit_value), "READ");
  CU_ASSERT_EQ(bit_value->position.value, 0u);
  CU_ASSERT_EQ(idl_parent(bit_value), bitmask);

  bit_value = idl_next(bit_value);
  CU_ASSERT_NEQ_FATAL(bit_value, NULL);
  CU_ASSERT_FATAL(idl_is_bit_value(bit_value));
  CU_ASSERT_STREQ(idl_identifier(bit_value), "WRITE");
  CU_ASSERT_EQ(bit_value->position.value, 1u);
  CU_ASSERT_EQ(idl_parent(bit_value), bitmask);

  bit_value = idl_next(bit_value);
  CU_ASSERT_NEQ_FATAL(bit_value, NULL);
  CU_ASSERT_FATAL(idl_is_bit_value(bit_value));
  CU_ASSERT_STREQ(idl_identifier(bit_value), "EXECUTE");
  CU_ASSERT_EQ(bit_value->position.value, 2u);
  CU_ASSERT_EQ(idl_parent(bit_value), bitmask);
  CU_ASSERT_EQ(idl_next(bit_value), NULL);

  strct = idl_next(bitmask);
  CU_ASSERT_NEQ_FATAL(strct, NULL);
  CU_ASSERT_FATAL(idl_is_struct(strct));
  member = strct->members;
  CU_ASSERT_NEQ_FATAL(member, NULL);
  CU_ASSERT_EQ(member->type_spec, bitmask);
  CU_ASSERT_STREQ(idl_identifier(member->declarators), "permissions");

  idl_delete_pstate(pstate);
}

CU_Test(idl_hand_parser, bitmask_rejects_empty_body)
{
  expect_parse_ret("bitmask Empty { };", IDL_RETCODE_SYNTAX_ERROR);
}

CU_Test(idl_hand_parser, bitmask_rejects_duplicate_bit_values)
{
  expect_parse_ret(
    "bitmask Permissions { READ, READ };", IDL_RETCODE_SEMANTIC_ERROR);
}

CU_Test(idl_hand_parser, bitmask_rejects_bit_value_matching_bitmask)
{
  expect_parse_ret(
    "bitmask Permissions { Permissions };", IDL_RETCODE_SEMANTIC_ERROR);
}
