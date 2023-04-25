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
#include <stdint.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "idl/string.h"
#include "idl/processor.h"

#include "CUnit/Theory.h"

/* a union must have at least one case */
CU_Test(idl_union, no_case)
{
  idl_retcode_t ret;
  idl_pstate_t *pstate = NULL;

  const char str[] = "union u switch(char) { };";
  ret = idl_create_pstate(0u, NULL, &pstate);
  CU_ASSERT_EQUAL_FATAL(ret, IDL_RETCODE_OK);
  CU_ASSERT_PTR_NOT_NULL_FATAL(pstate);
  ret = idl_parse_string(pstate, str);
  CU_ASSERT_EQUAL(ret, IDL_RETCODE_SYNTAX_ERROR);
  idl_delete_pstate(pstate);
}

CU_Test(idl_union, name_clash)
{
  idl_retcode_t ret;
  idl_pstate_t *pstate = NULL;

  const char str[] = "union u switch (long) { case 1: char c; };\n"
                     "union u switch (long) { case 1: char c; };";
  ret = idl_create_pstate(0u, NULL, &pstate);
  CU_ASSERT_EQUAL_FATAL(ret, IDL_RETCODE_OK);
  CU_ASSERT_PTR_NOT_NULL_FATAL(pstate);
  ret = idl_parse_string(pstate, str);
  CU_ASSERT_EQUAL(ret, IDL_RETCODE_SEMANTIC_ERROR);
  idl_delete_pstate(pstate);
}

CU_Test(idl_union, single_case)
{
  idl_retcode_t ret;
  idl_pstate_t *pstate = NULL;
  idl_union_t *u;
  idl_case_t *c;

  const char str[] = "union u switch(long) { case 1: char c; };";
  ret = idl_create_pstate(0u, NULL, &pstate);
  CU_ASSERT_EQUAL_FATAL(ret, IDL_RETCODE_OK);
  CU_ASSERT_PTR_NOT_NULL_FATAL(pstate);
  ret = idl_parse_string(pstate, str);
  CU_ASSERT_EQUAL_FATAL(ret, IDL_RETCODE_OK);
  u = (idl_union_t *)pstate->root;
  CU_ASSERT_FATAL(idl_is_union(u));
  assert(u);
  CU_ASSERT(idl_type(u->switch_type_spec->type_spec) == IDL_LONG);
  c = (idl_case_t *)u->cases;
  CU_ASSERT_FATAL(idl_is_case(c));
  CU_ASSERT_PTR_EQUAL(idl_parent(c), u);
  CU_ASSERT(idl_is_case_label(c->labels));
  CU_ASSERT(idl_type(c->type_spec) == IDL_CHAR);
  CU_ASSERT_FATAL(idl_is_declarator(c->declarator));
  CU_ASSERT_STRING_EQUAL(idl_identifier(c->declarator), "c");
  c = idl_next(c);
  CU_ASSERT_PTR_NULL(c);
  idl_delete_pstate(pstate);
}

CU_Test(idl_union, single_default_case)
{
  idl_retcode_t ret;
  idl_pstate_t *pstate = NULL;
  idl_union_t *u;
  idl_case_t *c;

  const char str[] = "union u switch(char) { default: char c; };";
  ret = idl_create_pstate(0u, NULL, &pstate);
  CU_ASSERT_EQUAL_FATAL(ret, IDL_RETCODE_OK);
  CU_ASSERT_PTR_NOT_NULL_FATAL(pstate);
  ret = idl_parse_string(pstate, str);
  CU_ASSERT_EQUAL_FATAL(ret, IDL_RETCODE_OK);
  u = (idl_union_t *)pstate->root;
  CU_ASSERT_FATAL(idl_is_union(u));
  assert(u);
  CU_ASSERT(idl_type(u->switch_type_spec->type_spec) == IDL_CHAR);
  c = (idl_case_t *)u->cases;
  CU_ASSERT_FATAL(idl_is_case(c));
  CU_ASSERT_PTR_EQUAL(idl_parent(c), u);
  CU_ASSERT(idl_is_default_case(c));
  CU_ASSERT(idl_type(c->type_spec) == IDL_CHAR);
  CU_ASSERT_FATAL(idl_is_declarator(c->declarator));
  CU_ASSERT_STRING_EQUAL(idl_identifier(c->declarator), "c");
  c = idl_next(c);
  CU_ASSERT_PTR_NULL(c);
  idl_delete_pstate(pstate);
}

// x. union with same declarators
// x. forward declared union
//   x.x. forward declared union before definition
//   x.x. forward declared union after definition
//   x.x. forward declared union with no definition at all
// x. forward declared struct
//   x.x. see union
// x. constant expressions
// x. identifier that collides with a keyword
// x. union with default
// x. union with two default branches
// x. union with multile labels for branch
// x. union with enumeration A and an enumerator from enumeration B

CU_Test(idl_union, enumerator_switch_type)
{
  idl_retcode_t ret;
  idl_pstate_t *pstate = NULL;
  idl_enum_t *e;
  idl_enumerator_t *el;
  idl_union_t *u;
  idl_case_t *c;
  const char *str;

  str = "enum Color { Red, Yellow, Blue };\n"
        "union u switch(Color) { case Red: char c; default: long l; };";

  ret = idl_create_pstate(0u, NULL, &pstate);
  CU_ASSERT_EQUAL_FATAL(ret, IDL_RETCODE_OK);
  CU_ASSERT_PTR_NOT_NULL(pstate);
  ret = idl_parse_string(pstate, str);
  CU_ASSERT_EQUAL_FATAL(ret, IDL_RETCODE_OK);
  e = (idl_enum_t *)pstate->root;
  CU_ASSERT_FATAL(idl_is_enum(e));
  assert(e);
  el = e->enumerators;
  CU_ASSERT_FATAL(idl_is_enumerator(el));
  CU_ASSERT_STRING_EQUAL(idl_identifier(el), "Red");
  el = idl_next(el);
  CU_ASSERT_FATAL(idl_is_enumerator(el));
  CU_ASSERT_STRING_EQUAL(idl_identifier(el), "Yellow");
  el = idl_next(el);
  CU_ASSERT_FATAL(idl_is_enumerator(el));
  CU_ASSERT_STRING_EQUAL(idl_identifier(el), "Blue");
  u = (idl_union_t *)idl_next(e);
  CU_ASSERT_FATAL(idl_is_union(u));
  c = u->cases;
  CU_ASSERT_FATAL(idl_is_case(c));
  CU_ASSERT((uintptr_t)c->labels->const_expr == (uintptr_t)e->enumerators);
  idl_delete_pstate(pstate);
}

/* the type for the union discriminator must be an integer, char, boolean,
   enumeration, or a reference to one of these */
#define M(name, definitions) "module " name " { " definitions " };"
#define S(name) "struct " name " { char c; };"
#define T(type, name) "typedef " type " " name ";"
#define U(type) "union u switch (" type ") { default: char c; };"

CU_Test(idl_union, typedef_switch_types)
{
  idl_retcode_t ret;
  idl_pstate_t *pstate = NULL;
  idl_module_t *m;
  idl_typedef_t *t;
  idl_union_t *u;
  const char *str;

  ret = idl_create_pstate(0u, NULL, &pstate);
  CU_ASSERT_EQUAL_FATAL(ret, IDL_RETCODE_OK);
  CU_ASSERT_PTR_NOT_NULL(pstate);
  assert(pstate);
  str = T("char", "baz") U("baz");
  ret = idl_parse_string(pstate, str);
  CU_ASSERT_EQUAL_FATAL(ret, IDL_RETCODE_OK);
  t = (idl_typedef_t *)pstate->root;
  CU_ASSERT(idl_is_typedef(t));
  u = idl_next(t);
  CU_ASSERT_FATAL(idl_is_union(u));
  CU_ASSERT_PTR_EQUAL(t->declarators, u->switch_type_spec->type_spec);
  idl_delete_pstate(pstate);

  ret = idl_create_pstate(0u, NULL, &pstate);
  CU_ASSERT_EQUAL_FATAL(ret, IDL_RETCODE_OK);
  assert(ret == IDL_RETCODE_OK);
  // Coverity thinks pstate is possibly a freed pointer
  // coverity[use_after_free:FALSE]
  CU_ASSERT_PTR_NOT_NULL(pstate);
  assert(pstate);
  str = M("foo", T("char", "baz") U("baz"));
  ret = idl_parse_string(pstate, str);
  CU_ASSERT_EQUAL_FATAL(ret, IDL_RETCODE_OK);
  m = (idl_module_t *)pstate->root;
  CU_ASSERT(idl_is_module(m));
  t = (idl_typedef_t *)m->definitions;
  CU_ASSERT(idl_is_typedef(t));
  u = idl_next(t);
  CU_ASSERT(idl_is_union(u));
  CU_ASSERT_PTR_EQUAL(t->declarators, u->switch_type_spec->type_spec);
  idl_delete_pstate(pstate);

  ret = idl_create_pstate(0u, NULL, &pstate);
  CU_ASSERT_EQUAL_FATAL(ret, IDL_RETCODE_OK);
  CU_ASSERT_PTR_NOT_NULL(pstate);
  assert(pstate);
  str = M("foo", T("char", "baz")) M("bar", U("foo::baz"));
  ret = idl_parse_string(pstate, str);
  CU_ASSERT_EQUAL_FATAL(ret, IDL_RETCODE_OK);
  m = (idl_module_t *)pstate->root;
  CU_ASSERT(idl_is_module(m));
  t = (idl_typedef_t *)m->definitions;
  CU_ASSERT(idl_is_typedef(t));
  m = idl_next(m);
  CU_ASSERT(idl_is_module(m));
  u = (idl_union_t *)m->definitions;
  CU_ASSERT(idl_is_union(u));
  CU_ASSERT_PTR_EQUAL(t->declarators, u->switch_type_spec->type_spec);
  idl_delete_pstate(pstate);
}

CU_TheoryDataPoints(idl_union, bad_switch_types) = {
  CU_DataPoints(const char *,
    S("baz") U("baz"),
    U("baz"),
    M("foo", T("float", "baz")) M("bar", U("foo::baz"))),
  CU_DataPoints(idl_retcode_t,
    IDL_RETCODE_SEMANTIC_ERROR,
    IDL_RETCODE_SEMANTIC_ERROR,
    IDL_RETCODE_SEMANTIC_ERROR)
};

CU_Theory((const char *str, idl_retcode_t expret), idl_union, bad_switch_types)
{
  idl_retcode_t ret;
  idl_pstate_t *pstate = NULL;

  ret = idl_create_pstate(0u, NULL, &pstate);
  CU_ASSERT_EQUAL_FATAL(ret, IDL_RETCODE_OK);
  CU_ASSERT_PTR_NOT_NULL(pstate);
  ret = idl_parse_string(pstate, str);
  CU_ASSERT_EQUAL(ret, expret);
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

CU_Test(idl_union, default_discriminator_bool)
{
  idl_retcode_t ret;
  idl_pstate_t *pstate;
  char buf[256];
  const char *fmt = "union u switch(boolean) { %s };";

  static const struct {
    const char *cases;
    idl_retcode_t result;
    enum { FIRST_DISCRIMINANT, DEFAULT_CASE, IMPLICIT_DEFAULT_CASE } condition;
    bool discriminant;
  } tests[] = {
    { "case FALSE: char x;",
      IDL_RETCODE_OK, IMPLICIT_DEFAULT_CASE, true },
    { "case FALSE: char x; default: char y;",
      IDL_RETCODE_OK, DEFAULT_CASE, true },
    { "case TRUE:  char x;",
      IDL_RETCODE_OK, IMPLICIT_DEFAULT_CASE, false },
    { "case TRUE:  char x; default: char y;",
      IDL_RETCODE_OK, DEFAULT_CASE, false },
    { "case FALSE: char x; case TRUE:  char y;",
      IDL_RETCODE_OK, FIRST_DISCRIMINANT, false },
    { "case TRUE:  char x; case FALSE: char y;",
      IDL_RETCODE_OK, FIRST_DISCRIMINANT, true },
    { "case FALSE: char x; case TRUE:  char y; default: char z;",
      IDL_RETCODE_SEMANTIC_ERROR, DEFAULT_CASE, false }
  };

  for (size_t i=0, n=sizeof(tests)/sizeof(tests[0]); i < n; i++) {

    pstate = NULL;
    idl_snprintf(buf, sizeof(buf), fmt, tests[i].cases);
    ret = parse_string(buf, &pstate);
    CU_ASSERT_EQUAL(ret, tests[i].result);
    if (ret == IDL_RETCODE_OK) {
      const idl_case_t *c;
      const idl_case_label_t *cl;
      const idl_literal_t *l;
      const idl_union_t *u;

      CU_ASSERT_PTR_NOT_NULL_FATAL(pstate);
      assert(pstate);
      u = (const idl_union_t *)pstate->root;
      CU_ASSERT_FATAL(idl_is_union(u));
      c = u->cases;
      CU_ASSERT_FATAL(idl_is_case(c));
      cl = c->labels;
      CU_ASSERT_FATAL(idl_is_case_label(cl));
      CU_ASSERT_FATAL(idl_is_literal(cl->const_expr));
      l = cl->const_expr;
      CU_ASSERT_FATAL(idl_type(l) == IDL_BOOL);
      if (tests[i].condition == DEFAULT_CASE) {
        static const idl_mask_t mask = IDL_DEFAULT_CASE_LABEL;
        CU_ASSERT_EQUAL(l->value.bln, !tests[i].discriminant);
        CU_ASSERT_NOT_EQUAL(u->unused_labels, 0);
        CU_ASSERT(idl_mask(u->default_case) == mask);
        CU_ASSERT_PTR_EQUAL(idl_parent(u->default_case), idl_next(c));
      } else if (tests[i].condition == IMPLICIT_DEFAULT_CASE) {
        static const idl_mask_t mask = IDL_IMPLICIT_DEFAULT_CASE_LABEL;
        CU_ASSERT_EQUAL(l->value.bln, !tests[i].discriminant);
        CU_ASSERT_NOT_EQUAL(u->unused_labels, 0);
        CU_ASSERT(idl_mask(u->default_case) == mask);
        CU_ASSERT_PTR_EQUAL(idl_parent(u->default_case), u);
      } else {
        static const idl_mask_t mask = IDL_CASE_LABEL;
        CU_ASSERT_EQUAL(l->value.bln, tests[i].discriminant);
        CU_ASSERT_EQUAL(u->unused_labels, 0);
        CU_ASSERT(idl_mask(u->default_case) == mask);
        CU_ASSERT_PTR_EQUAL(idl_parent(u->default_case), c);
      }
      l = u->default_case ? u->default_case->const_expr : NULL;
      CU_ASSERT_FATAL(idl_type(l) == IDL_BOOL);
      assert(l);
      CU_ASSERT_EQUAL(l->value.bln, tests[i].discriminant);
    }
    idl_delete_pstate(pstate);
  }
}

CU_Test(idl_union, default_discriminator_signed_int)
{
#define IMPLICIT \
  "union u switch(int8) { case %"PRId8": int8 i1; };"
#define EXPLICIT \
  "union u switch(int8) { case %"PRId8": int8 i1; default: int8 i2; };"

  idl_retcode_t ret;
  idl_pstate_t *pstate = NULL;
  char buf[256];

  static struct {
    int8_t label;
    int8_t discriminant;
    bool branch;
    const char *format;
  } tests[] = {
    { -1, 0, false, IMPLICIT },
    {  0, 1, false, IMPLICIT },
    {  1, 0, false, IMPLICIT },
    { -1, 0, true,  EXPLICIT },
    {  0, 1, true,  EXPLICIT },
    {  1, 0, true,  EXPLICIT }
  };

  for (size_t i=0, n=sizeof(tests)/sizeof(tests[0]); i < n; i++) {
    const idl_case_t *c;
    const idl_case_label_t *cl;
    const idl_literal_t *l;
    const idl_union_t *u;

    pstate = NULL;
    idl_snprintf(buf, sizeof(buf), tests[i].format, tests[i].label);
    ret = parse_string(buf, &pstate);
    CU_ASSERT_EQUAL_FATAL(ret, IDL_RETCODE_OK);
    CU_ASSERT_PTR_NOT_NULL_FATAL(pstate);
    assert(pstate);
    u = (const idl_union_t *)pstate->root;
    CU_ASSERT_FATAL(idl_is_union(u));
    c = u->cases;
    CU_ASSERT_FATAL(idl_is_case(c));
    cl = c->labels;
    CU_ASSERT_FATAL(idl_is_case_label(cl));
    CU_ASSERT_FATAL(idl_is_literal(cl->const_expr));
    l = cl->const_expr;
    CU_ASSERT_FATAL(idl_type(l) == IDL_INT8);
    CU_ASSERT_EQUAL(l->value.int8, tests[i].label);
    if (tests[i].branch) {
      CU_ASSERT(idl_mask(u->default_case) == IDL_DEFAULT_CASE_LABEL);
      CU_ASSERT_PTR_EQUAL(idl_parent(u->default_case), idl_next(c));
    } else {
      CU_ASSERT(idl_mask(u->default_case) == IDL_IMPLICIT_DEFAULT_CASE_LABEL);
      CU_ASSERT_PTR_EQUAL(idl_parent(u->default_case), u);
    }
    CU_ASSERT_FATAL(idl_is_literal(u->default_case->const_expr));
    l = u->default_case->const_expr;
    CU_ASSERT_FATAL(idl_type(l) == IDL_INT8);
    CU_ASSERT_EQUAL(l->value.int8, tests[i].discriminant);
    idl_delete_pstate(pstate);
  }

  // x. test if first discriminant is used if entire range is covered

#undef EXPLICIT
#undef IMPLICIT
}

CU_Test(idl_union, default_discriminator_unsigned_int)
{
#define IMPLICIT \
  "union u switch(uint8) { case %" PRIu8 ": uint8 i1; };"
#define EXPLICIT \
  "union u switch(uint8) { case %" PRIu8 ": uint8 i1; default: uint8 i2; };"

  idl_retcode_t ret;
  idl_pstate_t *pstate;
  char buf[256];

  static const struct {
    uint8_t label;
    uint8_t discriminant;
    bool branch;
    const char *format;
  } tests[] = {
    {  0, 1, false, IMPLICIT },
    {  1, 0, false, IMPLICIT },
    {  0, 1, true,  EXPLICIT },
    {  1, 0, true,  EXPLICIT }
  };

  for (size_t i=0, n=sizeof(tests)/sizeof(tests[0]); i < n; i++) {
    const idl_case_t *c;
    const idl_case_label_t *cl;
    const idl_literal_t *l;
    const idl_union_t *u;

    pstate = NULL;
    idl_snprintf(buf, sizeof(buf), tests[i].format, tests[i].label);
    ret = parse_string(buf, &pstate);
    CU_ASSERT_EQUAL_FATAL(ret, IDL_RETCODE_OK);
    CU_ASSERT_PTR_NOT_NULL_FATAL(pstate);
    assert(pstate);
    u = (const idl_union_t *)pstate->root;
    CU_ASSERT_FATAL(idl_is_union(u));
    c = u->cases;
    CU_ASSERT_FATAL(idl_is_case(c));
    cl = c->labels;
    CU_ASSERT_FATAL(idl_is_case_label(cl));
    CU_ASSERT_FATAL(idl_is_literal(cl->const_expr));
    l = cl->const_expr;
    CU_ASSERT_FATAL(idl_type(l) == IDL_UINT8);
    CU_ASSERT_EQUAL(l->value.uint8, tests[i].label);
    if (tests[i].branch) {
      CU_ASSERT(u->default_case && u->default_case->const_expr);
      CU_ASSERT(idl_mask(u->default_case) == IDL_DEFAULT_CASE_LABEL);
      CU_ASSERT_PTR_EQUAL(idl_parent(u->default_case), idl_next(c));
    } else {
      CU_ASSERT(idl_mask(u->default_case) == IDL_IMPLICIT_DEFAULT_CASE_LABEL);
      CU_ASSERT(u->default_case && u->default_case->const_expr);
      CU_ASSERT_PTR_EQUAL(idl_parent(u->default_case), u);
    }
    CU_ASSERT_FATAL(idl_is_literal(u->default_case->const_expr));
    l = u->default_case->const_expr;
    CU_ASSERT_FATAL(idl_type(l) == IDL_UINT8);
    CU_ASSERT_EQUAL(l->value.uint8, tests[i].discriminant);
    idl_delete_pstate(pstate);
  }

  // x. test if first discriminant is used if entire range is covered

#undef EXPLICIT
#undef IMPLICIT
}

CU_Test(idl_union, default_discriminator_enum)
{
  idl_retcode_t ret;
  idl_pstate_t *pstate = NULL;
  const idl_case_t *c;
  const idl_case_label_t *cl;
  const idl_enum_t *e1;
  const idl_enumerator_t *e1x;
  const idl_union_t *u1;
  const char *idl;

  idl = "enum e1 { e11, e12 };\n"
        "union u1 switch(e1) { case e12: char c; };";
  pstate = NULL;
  ret = parse_string(idl, &pstate);
  CU_ASSERT_EQUAL_FATAL(ret, IDL_RETCODE_OK);
  CU_ASSERT_PTR_NOT_NULL_FATAL(pstate);
  assert(pstate);
  e1 = (const idl_enum_t *)pstate->root;
  CU_ASSERT_FATAL(idl_is_enum(e1));
  e1x = e1->enumerators;
  CU_ASSERT_FATAL(idl_is_enumerator(e1x));
  u1 = idl_next(e1);
  CU_ASSERT_FATAL(idl_is_union(u1));
  CU_ASSERT_FATAL(idl_mask(u1->default_case) == IDL_IMPLICIT_DEFAULT_CASE_LABEL);
  CU_ASSERT(u1->default_case && u1->default_case->const_expr == e1x);
  idl_delete_pstate(pstate);

  idl = "enum e1 { e11, e12 };\n"
        "union u1 switch(e1) { case e11: char c; };";
  pstate = NULL;
  ret = parse_string(idl, &pstate);
  CU_ASSERT_EQUAL_FATAL(ret, IDL_RETCODE_OK);
  CU_ASSERT_PTR_NOT_NULL_FATAL(pstate);
  assert(pstate);
  e1 = (const idl_enum_t *)pstate->root;
  CU_ASSERT_FATAL(idl_is_enum(e1));
  e1x = idl_next(e1->enumerators);
  CU_ASSERT_FATAL(idl_is_enumerator(e1x));
  u1 = idl_next(e1);
  CU_ASSERT_FATAL(idl_is_union(u1));
  CU_ASSERT_FATAL(idl_mask(u1->default_case) == IDL_IMPLICIT_DEFAULT_CASE_LABEL);
  CU_ASSERT(u1->default_case && u1->default_case->const_expr == e1x);
  idl_delete_pstate(pstate);

  idl = "enum e1 { e11, e12 };\n"
        "union u1 switch(e1) { case e12: char c; default: long l; };";
  pstate = NULL;
  ret = parse_string(idl, &pstate);
  CU_ASSERT_EQUAL_FATAL(ret, IDL_RETCODE_OK);
  CU_ASSERT_PTR_NOT_NULL_FATAL(pstate);
  assert(pstate);
  e1 = (const idl_enum_t *)pstate->root;
  CU_ASSERT_FATAL(idl_is_enum(e1));
  e1x = e1->enumerators;
  CU_ASSERT_FATAL(idl_is_enumerator(e1x));
  u1 = idl_next(e1);
  CU_ASSERT_FATAL(idl_is_union(u1));
  c = idl_next(u1->cases);
  CU_ASSERT_FATAL(idl_is_case(c));
  cl = c->labels;
  CU_ASSERT_FATAL(idl_is_case_label(cl));
  CU_ASSERT_PTR_NOT_NULL(cl->const_expr);
  CU_ASSERT_FATAL(idl_mask(u1->default_case) == IDL_DEFAULT_CASE_LABEL);
  CU_ASSERT(u1->default_case && u1->default_case->const_expr == e1x);
  idl_delete_pstate(pstate);

  idl = "enum e1 { e11, e12 };\n"
        "union u1 switch(e1) { case e12: char c; case e11: long l; };";
  pstate = NULL;
  ret = parse_string(idl, &pstate);
  CU_ASSERT_EQUAL_FATAL(ret, IDL_RETCODE_OK);
  CU_ASSERT_PTR_NOT_NULL_FATAL(pstate);
  assert(pstate);
  e1 = (const idl_enum_t *)pstate->root;
  CU_ASSERT_FATAL(idl_is_enum(e1));
  e1x = idl_next(e1->enumerators);
  CU_ASSERT_FATAL(idl_is_enumerator(e1x));
  u1 = idl_next(e1);
  CU_ASSERT_FATAL(idl_is_union(u1));
  c = u1->cases;
  CU_ASSERT_FATAL(idl_is_case(c));
  cl = c->labels;
  CU_ASSERT_FATAL(idl_is_case_label(cl));
  CU_ASSERT_PTR_EQUAL(cl->const_expr, e1x);
  CU_ASSERT(idl_mask(u1->default_case) == IDL_CASE_LABEL);
  CU_ASSERT_PTR_EQUAL(u1->default_case, cl);
  idl_delete_pstate(pstate);

  idl = "enum e1 { e11, e12 };\n"
        "union u1 switch(e1) { case e11: char c; case e12: long l; default: double d; };";
  pstate = NULL;
  ret = parse_string(idl, &pstate);
  CU_ASSERT_EQUAL(ret, IDL_RETCODE_SEMANTIC_ERROR);
  CU_ASSERT_PTR_NULL(pstate);
  idl_delete_pstate(pstate);
}

CU_Test(idl_union, two_unions_one_enum)
{
  idl_retcode_t ret;
  idl_pstate_t *pstate = NULL;

  const char str[] = "enum aap { noot, mies };\n"
                     "union wim switch (aap) { case mies: double zus; };\n"
                     "union jet switch (aap) { case noot: double zus; };";

  ret = idl_create_pstate(0u, NULL, &pstate);
  CU_ASSERT_EQUAL_FATAL(ret, IDL_RETCODE_OK);
  CU_ASSERT_PTR_NOT_NULL_FATAL(pstate);
  ret = idl_parse_string(pstate, str);
  CU_ASSERT_EQUAL(ret, IDL_RETCODE_OK);
  idl_delete_pstate(pstate);
}
