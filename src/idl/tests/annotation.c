// Copyright(c) 2020 to 2022 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <assert.h>

#include "hashid.h"
#include "tree.h"
#include "idl/processor.h"

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

static idl_retcode_t
parse_string(uint32_t flags, const char *str, idl_pstate_t **pstatep)
{
  idl_pstate_t *pstate = NULL;
  idl_retcode_t ret;

  if ((ret = idl_create_pstate(flags, NULL, &pstate)) != IDL_RETCODE_OK)
    return ret;
  pstate->config.default_extensibility = IDL_FINAL;
  ret = idl_parse_string(pstate, str);
  if (ret != IDL_RETCODE_OK)
    idl_delete_pstate(pstate);
  else
    *pstatep = pstate;
  return ret;
}

typedef struct optional_test {
  const char *str;
  idl_retcode_t ret;
  bool optionals[16];
} optional_test_t;

static void test_optional(optional_test_t test)
{
  idl_pstate_t *pstate = NULL;
  idl_retcode_t ret = parse_string(IDL_FLAG_ANNOTATIONS, test.str, &pstate);
  CU_ASSERT_EQUAL_FATAL(ret, test.ret);

  if (pstate) {
    idl_node_t *node;
    int nstructs = 0;
    IDL_FOREACH(node, pstate->root) {
      if (!idl_is_struct(node))
        continue;
      nstructs++;
      idl_struct_t *s = (idl_struct_t *)node;
      CU_ASSERT_PTR_NOT_NULL_FATAL(s);
      CU_ASSERT_FATAL(idl_is_struct(s));
      assert(s);
      idl_member_t *m = NULL;
      int n = 0;
      IDL_FOREACH(m, s->members) {
        CU_ASSERT_EQUAL(m->optional.value, test.optionals[n]);
        n++;
      }
    }
    CU_ASSERT_EQUAL(nstructs, 1);
    idl_delete_pstate(pstate);
  }
}

CU_Test(idl_annotation, optional)
{
  static const optional_test_t tests[] = {
    {"struct s { char c; };",                                   IDL_RETCODE_OK,                  {false} },  //default (not optional)
    {"struct s { @optional char c; };",                         IDL_RETCODE_OK,                  {true} },  //implicit true
    {"struct s { @optional(false) char c; };",                  IDL_RETCODE_OK,                  {false} },  //explicit false
    {"struct s { @optional(true) char c; };",                   IDL_RETCODE_OK,                  {true} },  //explicit true
    {"struct s { @optional(false) @key char c; };",             IDL_RETCODE_OK,                  {false} },  //no conflict
    {"struct s { @optional(true) @key(false) char c; };",       IDL_RETCODE_OK,                  {true} },  //no conflict
    {"struct s { @optional(true) char c_1, c_2; char c_3; };",  IDL_RETCODE_OK,                  {true, false} },  //set on both declarators
    {"struct s { @optional sequence<double> s_d; };",           IDL_RETCODE_OK,                  {true} },  //set on sequence
    {"typedef sequence<long> seq_long;\n"
     "struct s { @optional seq_long s_l_a[15]; };",             IDL_RETCODE_OK,                  {true} },  //set on typedef
    {"struct s { @optional @key char c; };",                    IDL_RETCODE_SEMANTIC_ERROR,      {0} }, //optional not allowed on key members
    {"@optional struct s { char c; };",                         IDL_RETCODE_SEMANTIC_ERROR,      {0} }, //only allowed on member declarators
    {"enum e {  e_0, @optional e_1};",                          IDL_RETCODE_SEMANTIC_ERROR,      {0} }  //only allowed on members
  };

  for (size_t i = 0; i < sizeof(tests)/sizeof(tests[0]); i++) {
    test_optional(tests[i]);
  }
}

typedef struct idl_default_test {
  const char *str;
  idl_retcode_t ret;
  bool has_default;
  idl_type_t default_type;
  const void *default_val_ptr;
} idl_default_test_t;

static void test_default(
  idl_default_test_t test)
{
  idl_pstate_t *pstate = NULL;
  idl_retcode_t ret = parse_string(IDL_FLAG_ANNOTATIONS, test.str, &pstate);
  CU_ASSERT_EQUAL_FATAL(ret, test.ret);
  if (pstate) {
    idl_struct_t *s = (idl_struct_t *)pstate->root;
    CU_ASSERT_FATAL(idl_is_struct(s));
    assert(s);
    idl_member_t *m = NULL;
    IDL_FOREACH(m, s->members) {
      const idl_literal_t *def = idl_default_value(m);
      if (test.has_default) {
        CU_ASSERT_EQUAL_FATAL(idl_type(def), test.default_type);
        switch (test.default_type) {
          case IDL_LONG:
            CU_ASSERT_EQUAL(def->value.int32, *(const int32_t*)test.default_val_ptr);
            break;
          case IDL_ULONG:
            CU_ASSERT_EQUAL(def->value.uint32, *(const uint32_t*)test.default_val_ptr);
            break;
          case IDL_DOUBLE:
            CU_ASSERT_EQUAL(def->value.dbl, *(const double*)test.default_val_ptr);
            break;
          case IDL_CHAR:
            CU_ASSERT_EQUAL(def->value.chr, *(const char*)test.default_val_ptr);
            break;
          case IDL_STRING:
            CU_ASSERT_STRING_EQUAL(def->value.str, *(const char**)test.default_val_ptr);
            break;
          case IDL_BOOL:
            CU_ASSERT_EQUAL(def->value.bln, *(const bool*)test.default_val_ptr);
            break;
          default:
            break;
        }
      } else {
        CU_ASSERT_PTR_NULL_FATAL(def);
      }
    }
    idl_delete_pstate(pstate);
  }
}

CU_Test(idl_annotation, idl_default)
{
  static const int32_t t1 = -123456789;
  static const double t2 = 987.654321;
  static const char t3 = 'a';
  static const bool t4 = true;
  static const char *t5 = "hello world!";
  static const uint32_t t6 = 123456789;
  static const double t7 = 123456789;
  static const idl_default_test_t tests[] = {
    {"struct s { long l; };",                                IDL_RETCODE_OK,                  false, IDL_NULL,    NULL}, //no default whatsoever
    {"struct s { @default(-123456789) long l; };",           IDL_RETCODE_OK,                  true,  IDL_LONG,    &t1},  //default long
    {"struct s { @default(987.654321) double d; };",         IDL_RETCODE_OK,                  true,  IDL_DOUBLE,  &t2},  //default double
    {"struct s { @default('a') char c; };",                  IDL_RETCODE_OK,                  true,  IDL_CHAR,    &t3},  //default char
    {"struct s { @default(true) boolean b; };",              IDL_RETCODE_OK,                  true,  IDL_BOOL,    &t4},  //default bool
    {"struct s { @default(\"hello world!\") string str; };", IDL_RETCODE_OK,                  true,  IDL_STRING,  &t5},  //default string
    {"struct s { @default(123456789) unsigned long l; };",   IDL_RETCODE_OK,                  true,  IDL_ULONG,   &t6},  //default unsigned long
    {"struct s { @default(123456789) double l; };",          IDL_RETCODE_OK,                  true,  IDL_DOUBLE,  &t7},  //setting a double member to integer default
    {"struct s { @default(123) @optional long l; };",        IDL_RETCODE_SEMANTIC_ERROR,      false, IDL_NULL,    NULL}, //mixing default and optional
    {"struct s { @default long l; };",                       IDL_RETCODE_SEMANTIC_ERROR,      false, IDL_NULL,    NULL}, //misssing parameter
    {"struct s { @default(123) string str; };",              IDL_RETCODE_ILLEGAL_EXPRESSION,  false, IDL_NULL,    NULL}, //parameter type mismatch (int vs string)
    {"struct s { @default(\"false\") boolean b; };",         IDL_RETCODE_ILLEGAL_EXPRESSION,  false, IDL_NULL,    NULL}, //parameter type mismatch (string vs bool)
    {"struct s { @default(123) boolean b; };",               IDL_RETCODE_ILLEGAL_EXPRESSION,  false, IDL_NULL,    NULL}, //parameter type mismatch (int vs bool)
    {"struct s { @default(-123) unsigned long l; };",        IDL_RETCODE_OUT_OF_RANGE,        false, IDL_NULL,    NULL}, //parameter type mismatch (unsigned vs signed)
    {"@default(e_0) enum e { e_0, e_1, e_2, e_3 };",         IDL_RETCODE_SEMANTIC_ERROR,      false, IDL_NULL,    NULL},  //setting default on enums is done through @default_literal
    {"enum e { e_0, e_1, e_2, e_3 }; struct s { @default(e_1) e m_e; };", IDL_RETCODE_SEMANTIC_ERROR, false, IDL_NULL, NULL},  //setting enums default is not yet supported
  };

  for (size_t i = 0; i < sizeof(tests)/sizeof(tests[0]); i++) {
    test_default(tests[i]);
  }

}

typedef struct enum_default_test {
  const char *str;
  idl_retcode_t ret;
  uint32_t default_index;
  const char *default_name;
  uint32_t default_mask;
} enum_default_test_t;

static void test_enum_default(enum_default_test_t test) {
  idl_pstate_t *pstate = NULL;
  idl_retcode_t ret = parse_string(IDL_FLAG_ANNOTATIONS, test.str, &pstate);
  CU_ASSERT_EQUAL(ret, test.ret);
  if (test.ret == ret
   && ret == IDL_RETCODE_OK) {
    CU_ASSERT_PTR_NOT_NULL_FATAL(pstate);
    assert(pstate);
    idl_enum_t *e = (idl_enum_t *)pstate->root;
    CU_ASSERT(idl_is_enum(e));
    if (idl_is_enum(e)) {
      assert(e);
      idl_enumerator_t *en = e->default_enumerator;
      CU_ASSERT_PTR_NOT_NULL_FATAL(en);
      if (en) {
        CU_ASSERT_EQUAL(en->value.value, test.default_index);
        CU_ASSERT_EQUAL(idl_mask(en), test.default_mask);
        CU_ASSERT_STRING_EQUAL(en->name->identifier, test.default_name);
      }

      IDL_FOREACH(en, e->enumerators) {
        if (en == e->default_enumerator) {
          CU_ASSERT_EQUAL(idl_mask(en), test.default_mask);
        } else {
          CU_ASSERT_EQUAL(idl_mask(en), IDL_ENUMERATOR);
        }
      }
    }

    idl_delete_pstate(pstate);
  }
}

CU_Test(idl_annotation, default_literal)
{
  static const enum_default_test_t tests[] = {
    {"enum e { e_0, e_1, e_2, e_3 };",                                    IDL_RETCODE_OK,             0,    "e_0",  IDL_IMPLICIT_DEFAULT_ENUMERATOR }, //implicit defaults to the first entry
    {"enum e { e_0, @default_literal e_1, e_2, e_3 };",                   IDL_RETCODE_OK,             1,    "e_1",  IDL_DEFAULT_ENUMERATOR          }, //setting an explicit default through the annotation
    {"enum e { @value(123) e_0, @default_literal e_1, e_2, e_3 };",       IDL_RETCODE_OK,             124,  "e_1",  IDL_DEFAULT_ENUMERATOR          }, //starting at a different id
    {"enum e { e_0, e_1, @default_literal @default_literal e_2, e_3 };",  IDL_RETCODE_OK,             2,    "e_2",  IDL_DEFAULT_ENUMERATOR          }, //re-annotate
    {"enum e { e_0, @default_literal e_1, @default_literal e_2, e_3 };",  IDL_RETCODE_SEMANTIC_ERROR, 0,    NULL,   0                               },  //more than one enumerator set as default
    {"struct s { @default_literal long l; double d; };\n",                IDL_RETCODE_SEMANTIC_ERROR, 0,    NULL,   0                               },  //this annotation is only allowed on enumerator fields
  };

  for (size_t i = 0; i < sizeof(tests)/sizeof(tests[0]); i++) {
    test_enum_default(tests[i]);
  }
}

typedef struct key_test {
  const char *str;
  idl_retcode_t ret;
  bool val[8];
  bool annotated[8];
} key_test_t;

static void test_key(key_test_t test)
{
  idl_pstate_t *pstate = NULL;

  idl_retcode_t ret = parse_string(IDL_FLAG_ANNOTATIONS, test.str, &pstate);

  CU_ASSERT_EQUAL(ret, test.ret);

  if (ret)
    return;

  CU_ASSERT_PTR_NOT_NULL_FATAL(pstate);
  assert(pstate);

  if (idl_is_struct(pstate->root)) {
    idl_struct_t *s = (idl_struct_t *)pstate->root;
    assert(s);
    size_t i = 0;
    idl_member_t *m = NULL;
    IDL_FOREACH(m, s->members) {
      if (test.annotated[i]) {
        CU_ASSERT_PTR_NOT_NULL(m->key.annotation);
      } else {
        CU_ASSERT_PTR_NULL(m->key.annotation);
      }
      CU_ASSERT(m->key.value == test.val[i]);

      i++;
    }
  } else if (idl_is_union(pstate->root)) {
    idl_union_t *u = (idl_union_t *)pstate->root;
    assert(u);

    if (test.annotated[0]) {
      CU_ASSERT_PTR_NOT_NULL(u->switch_type_spec->key.annotation);
    } else {
      CU_ASSERT_PTR_NULL(u->switch_type_spec->key.annotation);
    }
    CU_ASSERT(u->switch_type_spec->key.value == test.val[0]);
  } else {
    CU_ASSERT(false);
  }

  idl_delete_pstate(pstate);

}

CU_Test(idl_annotation, key)
{
  key_test_t tests[] = {
    {"@mutable struct s {\n"
     "  @key char a;\n"
     "  @key(TRUE) char b;\n"
     "  @key(FALSE) char c;\n"
     "  @must_understand @key char d;\n"
     "  @optional(FALSE) @key char e;\n"
     "  char f;\n"
     "};", IDL_RETCODE_OK, {true, true, false, true, true, false}, {true, true, true, true, true, false}},
    {"union u switch(@key long) {\n"
     " case 0: char c;\n"
     " case 1: double d;\n"
     "};", IDL_RETCODE_OK, {true}, {true}},
    {"union u switch(long) {\n"
     " case 0: char c;\n"
     " case 1: @key double d;\n"
     "};", IDL_RETCODE_SEMANTIC_ERROR, {false}, {false}},
    {"@mutable struct s {\n"
     "  @must_understand(FALSE) @key char c;\n"
     "};", IDL_RETCODE_SEMANTIC_ERROR, {false}, {false}},
    {"struct s {\n"
     "  @optional @key char c;\n"
     "};", IDL_RETCODE_SEMANTIC_ERROR, {false}, {false}},
    {"@key struct s {\n"
     "  char c;\n"
     "};", IDL_RETCODE_SEMANTIC_ERROR, {false}, {false}},
    {"@key module m {\n"
     "  struct s {\n"
     "    char c;\n"
     "  };\n"
     "};\n", IDL_RETCODE_SEMANTIC_ERROR, {false}, {false}},
  };

  for (size_t i = 0; i < sizeof(tests)/sizeof(tests[0]); i++) {
    test_key(tests[i]);
  }
}

CU_Test(idl_annotation, nested)
{
  idl_retcode_t ret;
  idl_pstate_t *pstate = NULL;
  idl_struct_t *s;
  const char str[] = "struct s1 { char c; };\n"
                     "@nested struct s2 { char c; };\n"
                     "@nested(TRUE) struct s3 { char c; };\n"
                     "@nested(FALSE) struct s4 { char c; };";

  ret = parse_string(IDL_FLAG_ANNOTATIONS, str, &pstate);
  CU_ASSERT_EQUAL_FATAL(ret, IDL_RETCODE_OK);
  CU_ASSERT_PTR_NOT_NULL_FATAL(pstate);
  assert(pstate);
  s = (idl_struct_t *)pstate->root;
  CU_ASSERT_FATAL(idl_is_struct(s));
  CU_ASSERT_PTR_NULL(s->nested.annotation);
  CU_ASSERT(s->nested.value == false);
  s = idl_next(s);
  CU_ASSERT_FATAL(idl_is_struct(s));
  CU_ASSERT_PTR_NOT_NULL(s->nested.annotation);
  CU_ASSERT(s->nested.value == true);
  s = idl_next(s);
  CU_ASSERT_FATAL(idl_is_struct(s));
  CU_ASSERT_PTR_NOT_NULL(s->nested.annotation);
  CU_ASSERT(s->nested.value == true);
  s = idl_next(s);
  CU_ASSERT_FATAL(idl_is_struct(s));
  CU_ASSERT_PTR_NOT_NULL(s->nested.annotation);
  CU_ASSERT(s->nested.value == false);
  idl_delete_pstate(pstate);
}

#define M(name, definitions) " module " name " { " definitions " }; "
#define S(name) " struct " name " { char c; }; "

#define DN(...) " @default_nested " __VA_ARGS__
#define N(...) " @nested " __VA_ARGS__
#define T(...) " @topic " __VA_ARGS__
#define P(platform) " (platform = \"" platform "\") "

#define YES " (TRUE) "
#define NO " (FALSE) "

CU_Test(idl_annotation, topic)
{
  static const struct {
    const char *s;
    const char *a;
    bool v;
  } tests[] = {
    {                   S("s1"), NULL,     false },
    {               N() S("s1"), "nested", true  },
    { T()               S("s1"), "topic",  false },
    { T()           N() S("s1"), "topic",  false },
    { T(P("DDS"))       S("s1"), "topic",  false },
    { T(P("!DDS"))      S("s1"), NULL,     false },
    { T(P("!DDS"))      S("s1"), NULL,     false  },
    { T(P("CORBA"))     S("s1"), NULL,     false  }
  };

  static const size_t n = sizeof(tests)/sizeof(tests[0]);

  idl_retcode_t ret;
  idl_pstate_t *pstate;
  idl_struct_t *s;

  for (size_t i=0; i < n; i++) {
    pstate = NULL;
    ret = parse_string(IDL_FLAG_ANNOTATIONS, tests[i].s, &pstate);
    CU_ASSERT_EQUAL(ret, IDL_RETCODE_OK);
    if (ret == IDL_RETCODE_OK) {
      const char *a;
      s = (idl_struct_t *)pstate->root;
      CU_ASSERT_FATAL(idl_is_struct(s));
      a = idl_identifier(s->nested.annotation);
      CU_ASSERT((a == NULL) == (tests[i].a == NULL) && (a == NULL || strcmp(a, tests[i].a) == 0));
      CU_ASSERT(s->nested.value == tests[i].v);
    }
    idl_delete_pstate(pstate);
  }
}

CU_Test(idl_annotation, default_nested)
{
  static const struct {
    const char *str;
    struct { bool a; bool v; } dn[3];
    struct { bool a; bool v; } n[2];
    bool pstate_default_nested;
  } tests[] = {
    {         M("m1",         M("m2",        S("s1")) M("m3", S("s2"))), {{0,0},{0,0},{0,0}}, {{0,0},{0,0}}, false },
    { DN()    M("m1",         M("m2",        S("s1")) M("m3", S("s2"))), {{1,1},{0,1},{0,1}}, {{0,1},{0,1}}, false },
    { DN()    M("m1", DN(NO)  M("m2",        S("s1")) M("m3", S("s2"))), {{1,1},{1,0},{0,1}}, {{0,0},{0,1}}, false },
    { DN(NO)  M("m1", DN(YES) M("m2",        S("s1")) M("m3", S("s2"))), {{1,0},{1,1},{0,0}}, {{0,1},{0,0}}, false },
    { DN(YES) M("m1",         M("m2", N(NO)  S("s1")) M("m3", S("s2"))), {{1,1},{0,1},{0,1}}, {{1,0},{0,1}}, false },
    {         M("m1",         M("m2",        S("s1")) M("m3", S("s2"))), {{0,1},{0,1},{0,1}}, {{0,1},{0,1}}, true },
    { DN()    M("m1",         M("m2",        S("s1")) M("m3", S("s2"))), {{1,1},{0,1},{0,1}}, {{0,1},{0,1}}, true },
    { DN()    M("m1", DN(NO)  M("m2",        S("s1")) M("m3", S("s2"))), {{1,1},{1,0},{0,1}}, {{0,0},{0,1}}, true },
    { DN(NO)  M("m1", DN(YES) M("m2",        S("s1")) M("m3", S("s2"))), {{1,0},{1,1},{0,0}}, {{0,1},{0,0}}, true },
    { DN(YES) M("m1",         M("m2", N(NO)  S("s1")) M("m3", S("s2"))), {{1,1},{0,1},{0,1}}, {{1,0},{0,1}}, true },
  };

  static const size_t n = sizeof(tests)/sizeof(tests[0]);

  idl_retcode_t ret;
  idl_pstate_t *pstate;
  idl_module_t *m;
  idl_struct_t *s;

  for (size_t i=0; i < n; i++) {
    pstate = NULL;
    ret = idl_create_pstate(IDL_FLAG_ANNOTATIONS, NULL, &pstate);
    if (IDL_RETCODE_OK == ret) {
      pstate->config.default_extensibility = IDL_FINAL;
      pstate->config.default_nested = tests[i].pstate_default_nested;
      ret = idl_parse_string(pstate, tests[i].str);
    }

    CU_ASSERT_EQUAL(ret, IDL_RETCODE_OK);
    if (ret == IDL_RETCODE_OK) {
      m = (idl_module_t *)pstate->root;
      CU_ASSERT_FATAL(idl_is_module(m));
      CU_ASSERT(!tests[i].dn[0].a == !m->default_nested.annotation);
      CU_ASSERT_EQUAL(m->default_nested.value, tests[i].dn[0].v);
      m = m->definitions;
      CU_ASSERT_FATAL(idl_is_module(m));
      CU_ASSERT(!tests[i].dn[1].a == !m->default_nested.annotation);
      CU_ASSERT_EQUAL(m->default_nested.value, tests[i].dn[1].v);
      s = m->definitions;
      CU_ASSERT_FATAL(idl_is_struct(s));
      CU_ASSERT(!tests[i].n[0].a == !s->nested.annotation);
      CU_ASSERT(s->nested.value == tests[i].n[0].v);
      m = idl_next(m);
      CU_ASSERT_FATAL(idl_is_module(m));
      CU_ASSERT(!tests[i].dn[2].a == !m->default_nested.annotation);
      CU_ASSERT_EQUAL(m->default_nested.value, tests[i].dn[2].v);
      s = m->definitions;
      CU_ASSERT_FATAL(idl_is_struct(s));
      CU_ASSERT(!tests[i].n[1].a == !s->nested.annotation);
      CU_ASSERT(s->nested.value == tests[i].n[1].v);
    }
    if (pstate)
      idl_delete_pstate(pstate);
  }
}
#undef M
#undef S
#undef DN
#undef N
#undef T
#undef P

#define ok IDL_RETCODE_OK
#define semantic_error IDL_RETCODE_SEMANTIC_ERROR

static struct {
  idl_retcode_t ret;
  const char *str;
} redef[] = {
  { ok, "@annotation foo { boolean bar default TRUE; };"
        "@annotation foo { boolean bar default TRUE; };" },
  { semantic_error, "@annotation foo { boolean bar default TRUE; };"
                    "@annotation foo { boolean bar default FALSE; };" },
  { semantic_error, "@annotation foo { boolean bar default TRUE; };"
                    "@annotation foo { boolean bar; };" }
};

CU_Test(idl_annotation, redefinition)
{
  idl_retcode_t ret;
  idl_pstate_t *pstate = NULL;
  static const size_t n = sizeof(redef)/sizeof(redef[0]);

  for (size_t i = 0; i < n; i++) {
    pstate = NULL;
    ret = parse_string(IDL_FLAG_ANNOTATIONS, redef[i].str, &pstate);
    CU_ASSERT_EQUAL(ret, redef[i].ret);
    if (ret == IDL_RETCODE_OK) {
      CU_ASSERT(pstate && pstate->builtin_root);
    }
    idl_delete_pstate(pstate);
  }
}

typedef struct id_test {
  const char *s;
  idl_retcode_t ret;
  idl_autoid_t aid[8];
  bool annotation_present[8];
  uint32_t id[8];
} id_test_t;

static
void test_id(
  id_test_t test)
{
  idl_pstate_t *pstate = NULL;
  idl_retcode_t ret = parse_string(IDL_FLAG_ANNOTATIONS, test.s, &pstate);
  CU_ASSERT_EQUAL(ret, test.ret);

  if (ret == IDL_RETCODE_OK && ret == test.ret) {

    size_t m = 0, n = 0;
    const idl_node_t *node = pstate->root;
    while (idl_is_module(node)) {
      const idl_module_t *mod = (const idl_module_t *)node;
      CU_ASSERT_TRUE_FATAL(m < sizeof(test.id)/sizeof(test.id[0]));
      CU_ASSERT_EQUAL(mod->autoid.value, test.aid[m]);
      m++;
      assert(mod->definitions);
      node = (const idl_node_t*)mod->definitions;
    }

    IDL_FOREACH(node, node) {
      CU_ASSERT_TRUE_FATAL(m < sizeof(test.id)/sizeof(test.id[0]));
      if (idl_is_struct(node)) {
        const idl_struct_t *s = (const idl_struct_t*)node;
        CU_ASSERT_EQUAL(s->autoid.value, test.aid[m]);
        m++;

        const idl_member_t *mem = NULL;
        const idl_declarator_t *decl = NULL;
        IDL_FOREACH(mem, s->members) {
          IDL_FOREACH(decl, mem->declarators) {
            CU_ASSERT_TRUE_FATAL(n < sizeof(test.aid)/sizeof(test.aid[0]));
            if (test.annotation_present[n]) {
              CU_ASSERT_PTR_NOT_NULL(decl->id.annotation);
            } else {
              CU_ASSERT_PTR_NULL(decl->id.annotation);
            }
            CU_ASSERT_EQUAL(decl->id.value, test.id[n]);
            n++;
          }
        }
      } else if (idl_is_union(node)) {
        const idl_union_t *u = (const idl_union_t*)node;
        CU_ASSERT_EQUAL(u->autoid.value, test.aid[m]);
        m++;

        const idl_case_t *_case = NULL;
        IDL_FOREACH(_case, u->cases) {
          CU_ASSERT_TRUE_FATAL(n < sizeof(test.aid)/sizeof(test.aid[0]));
          if (test.annotation_present[n]) {
            CU_ASSERT_PTR_NOT_NULL(_case->declarator->id.annotation);
          } else {
            CU_ASSERT_PTR_NULL(_case->declarator->id.annotation);
          }
          CU_ASSERT_EQUAL(_case->declarator->id.value, test.id[n]);
          n++;
        }
      } else {
        CU_ASSERT_FATAL(0);
      }
    }
  }

  idl_delete_pstate(pstate);
}

CU_Test(idl_annotation, id)
{
  static const id_test_t tests[] = {
    {"struct s { @id(1) char c; };",          IDL_RETCODE_OK,             {IDL_SEQUENTIAL}, {true}, {1}},         // @id on member
    {"struct s { @id(0xffffffff) char c; };", IDL_RETCODE_SEMANTIC_ERROR, {0},              {0},    {0}},         // @id out of range
    {"@id(1) struct s { char c; };",          IDL_RETCODE_SEMANTIC_ERROR, {0},              {0},    {0}},         // @id on struct
    {"struct s { @id char c; };",             IDL_RETCODE_SEMANTIC_ERROR, {0},              {0},    {0}},         // @id without const-expr
    {"struct s { @id(1) @id(1) char c; };",   IDL_RETCODE_OK,             {IDL_SEQUENTIAL}, {true}, {1}},         // duplicate @id
    {"struct s { @id(1) @id(2) char c; };",   IDL_RETCODE_SEMANTIC_ERROR, {0},              {0},    {0}},         // conflicting @id
    {"struct s { @id(1) @hashid char c; };",  IDL_RETCODE_SEMANTIC_ERROR, {0},              {0},    {0}},         // @id and @hashid
    {"union u switch(long) {\n"
     "case 0: @id(123) char c;\n"
     "default: long l;\n"
     "};",                                    IDL_RETCODE_OK,             {IDL_SEQUENTIAL}, {true}, {123, 124}},  // @id on u::c
    {"union u switch(long) {\n"
     "case 0: @id(1) @id(1) char c;\n"
     "default: long l;\n"
     "};",                                    IDL_RETCODE_OK,             {IDL_SEQUENTIAL}, {true}, {1, 2}},      // duplicate @id on u::c
    {"union u switch(long) {\n"
     "case 0: @id(1) @id(2) char c;\n"
     "default: long l;\n"
     "};",                                    IDL_RETCODE_SEMANTIC_ERROR, {0},              {0},    {0}},         // conflicting @ids on u::c
    {"union u switch(long) {\n"
     "case 0: @id(1) char c;\n"
     "case 1: long l;\n"
     "default: @id(2) double d;\n"
     "};",                                    IDL_RETCODE_SEMANTIC_ERROR, {0},              {0},    {0}},         // u::d's id clashes with u::c
    {"@id(123) union u switch(long) {\n"
     "case 0: char c;\n"
     "default: double d;\n"
     "};",                                    IDL_RETCODE_SEMANTIC_ERROR, {0},              {0},    {0}},         // @id on union
  };

  for (size_t i = 0; i < sizeof(tests)/sizeof(id_test_t); i++)
    test_id(tests[i]);
}

CU_Test(idl_annotation, hashid)
{
  static const id_test_t tests[] = {
    {"struct s { @hashid char c; };",                       IDL_RETCODE_OK,             {IDL_SEQUENTIAL}, {true},       {0x00088a4au}},               // @hashid without parameter on member
    {"struct s { @hashid(\"s\") char c; };",                IDL_RETCODE_OK,             {IDL_SEQUENTIAL}, {true},       {0x0cc0c703u}},               // @hashid with parameter on member
    {"@hashid struct s { char c; };",                       IDL_RETCODE_SEMANTIC_ERROR, {0},              {0},          {0}},                         // @hashid on struct
    {"struct s { @hashid @hashid char c; };",               IDL_RETCODE_OK,             {IDL_SEQUENTIAL}, {true},       {0x00088a4au}},               // duplicate non-parameterized @hashid
    {"struct s { @hashid(\"c\") @hashid char c; };",        IDL_RETCODE_SEMANTIC_ERROR, {0},              {0},          {0}},                         // mixing explicit and implicit hash
    {"struct s { @hashid(\"c\") @hashid(\"c\") char c; };", IDL_RETCODE_OK,             {IDL_SEQUENTIAL}, {true},       {0x00088a4au}},               // duplicate parameterized @hashid
    {"struct s { @hashid(\"c\") @hashid(\"s\") char c; };", IDL_RETCODE_SEMANTIC_ERROR, {0},              {0},          {0}},                         // conflicting @hashid
    {"union u switch(long) {\n"
     "case 0: @hashid char c;\n"
     "default: @hashid(\"s\") long l;\n"
     "};",                                                  IDL_RETCODE_OK,             {IDL_SEQUENTIAL}, {true, true}, {0x00088a4au, 0x0cc0c703u}},  // @hashid with and without parameter on union branch
    {"union u switch(long) {\n"
     "case 0: @hashid char c;\n"
     "default: @hashid(\"c\") long l;\n"
     "};",                                                  IDL_RETCODE_SEMANTIC_ERROR, {0},              {0},          {0}},                         // u::l's hashid clashes with u::c
    {"@hashid union u switch(long) {\n"
     "case 0: char c;\n"
     "default: long l;\n"
     "};",                                                  IDL_RETCODE_SEMANTIC_ERROR, {0},              {0},          {0}},                         // hashid on union
  };

  for (size_t i = 0; i < sizeof(tests)/sizeof(id_test_t); i++)
    test_id(tests[i]);
}

CU_Test(idl_annotation, autoid_struct)
{
  static const id_test_t tests[] = {
    {"struct s { char c; char d; };",                                               IDL_RETCODE_OK,             {IDL_SEQUENTIAL}, {0},            {0, 1}},                                  //implicit sequential autoid
    {"struct s { @id(456) char c; char d; };",                                      IDL_RETCODE_OK,             {IDL_SEQUENTIAL}, {true, false},  {456, 457}},                              //implicit sequential autoid, starting from a specific id
    {"@autoid struct s { char c; char d; };",                                       IDL_RETCODE_OK,             {IDL_HASH},       {0},            {0x00088a4au, 0x01e07782u}},              //implicit hash autoid
    {"@autoid(HASH) struct s { char c, d; char e; };",                              IDL_RETCODE_OK,             {IDL_HASH},       {0},            {0x00088a4au, 0x01e07782u, 0x071767e1u}}, //explicit hash autoid
    {"@autoid(SEQUENTIAL) struct s { char c, d; char e; };",                        IDL_RETCODE_OK,             {IDL_SEQUENTIAL}, {0},            {0, 1, 2}},                               //explicit sequential autoid
    {"@autoid(SEQUENTIAL) struct s { @hashid char c; char d; };",                   IDL_RETCODE_OK,             {IDL_SEQUENTIAL}, {true, false},  {0x00088a4au, 0x00088a4bu}},              //explicit sequential autoid, with implicit hashid
    {"@autoid(HASH) struct s { @id(123) char c; char d; };",                        IDL_RETCODE_OK,             {IDL_HASH},       {true, false},  {123, 0x01e07782u}},                      //explicit hash autoid, with explicit id
    {"@autoid @autoid(SEQUENTIAL) struct s { char c; };",                           IDL_RETCODE_SEMANTIC_ERROR, {0},              {0},            {0}},                                     //conflicting duplicate autoid
    {"@autoid(SEQUENTIAL) struct s { @id(456) char c; char d; @id(457) char e; };", IDL_RETCODE_SEMANTIC_ERROR, {0},              {0},            {0}},                                     //clashing sequential id fields
    {"@autoid(HASH) struct s { char c; @hashid(\"c\") char d; };",                  IDL_RETCODE_SEMANTIC_ERROR, {0},              {0},            {0}}                                      //clashing hashid fields
  };

  for (size_t i = 0; i < sizeof(tests)/sizeof(id_test_t); i++)
    test_id(tests[i]);
}

CU_Test(idl_annotation, autoid_union)
{
  static const id_test_t tests[] = {
    {"union u switch(long) {\n"
     "case 0: char c;\n"
     "default: string s;\n"
     "};",                                          IDL_RETCODE_OK,             {IDL_SEQUENTIAL}, {false},  {0, 1}},                      // implicit sequential autoid
    {"union u switch(long) {\n"
     "case 0: @id(123) char c;\n"
     "default: string s;\n"
     "};",                                          IDL_RETCODE_OK,             {IDL_SEQUENTIAL}, {true},   {123, 124}},                  // implicit sequential autoid, starting from a specific id
    {"@autoid union u switch(long) {\n"
     "case 0: char c;\n"
     "default: string s;\n"
     "};",                                          IDL_RETCODE_OK,             {IDL_HASH},       {false},  {0x00088a4au, 0x0cc0c703u}},  // explicit autoid, implicit hash
    {"@autoid(HASH) union u switch(long) {\n"
     "case 0: char c;\n"
     "default: string s;\n"
     "};",                                          IDL_RETCODE_OK,             {IDL_HASH},       {false},  {0x00088a4au, 0x0cc0c703u}},  // explicit hash autoid
    {"@autoid(SEQUENTIAL) union u switch(long) {\n"
     "case 0: @id(123) char c;\n"
     "case 1: double d;\n"
     "case 2: @id(122) long e;\n"
     "default: string s;\n"
     "};",                                          IDL_RETCODE_SEMANTIC_ERROR, {0},              {0},      {0}},                         // u::s clashes with u::c
    {"@autoid(HASH) union u switch(long) {\n"
     "case 0: char c;\n"
     "default: @hashid(\"c\") string s;\n"
     "};",                                          IDL_RETCODE_SEMANTIC_ERROR, {0},              {0},      {0}},                         // u::s clashes with u::c
  };

  for (size_t i = 0; i < sizeof(tests)/sizeof(id_test_t); i++)
    test_id(tests[i]);
}

CU_Test(idl_annotation, autoid_inheritance)
{
  static const id_test_t tests[] = {
    {"struct base { char c; char d; };\n"
     "struct derived : base { char e; char f; };",                IDL_RETCODE_OK,             {IDL_SEQUENTIAL, IDL_SEQUENTIAL},                 {0},  {0, 1, 2, 3}},                                          //implicit sequential autoid, single inheritance
    {"struct base { char c; char d; };\n"
     "struct derived : base { char e; char f; };\n"
     "struct morederived : derived { char g; char h; };",         IDL_RETCODE_OK,             {IDL_SEQUENTIAL, IDL_SEQUENTIAL, IDL_SEQUENTIAL}, {0},  {0, 1, 2, 3, 4, 5}},                                    //implicit sequential autoid, double inheritance
    {"@autoid(HASH) struct base { char c; char d; };\n"
     "struct derived : base { char e; char f; };",                IDL_RETCODE_OK,             {IDL_HASH, IDL_SEQUENTIAL},                       {0},  {0x00088a4au, 0x01e07782u, 0x01e07783u, 0x01e07784u}},  //explicit hash autoid, derived class counting sequentially
    {"@autoid(SEQUENTIAL) struct base { char c; char d; };\n"
     "@autoid(HASH) struct derived : base { char e; char f; };",  IDL_RETCODE_OK,             {IDL_SEQUENTIAL, IDL_HASH},                       {0},  {0, 1, 0x071767e1u, 0x0d4ca18fu}},                      //explicit sequential autoid, derived class hashing
    {"struct base { char c; char d; };\n"
     "@autoid(SEQUENTIAL) struct derived : base {\n"
     " char e; char f; };",                                       IDL_RETCODE_OK,             {IDL_SEQUENTIAL, IDL_SEQUENTIAL},                 {0},  {0, 1, 2, 3}},                                          //explicit sequential autoid, checking that this starts from the implicit sequential ids from base
    {"struct baz { @id(2) long l2; @id(4) long l4; };\n"
     "struct bar : baz { @id(1) long l1; @id(3) long l3; };\n"
     "struct foo : bar { @id(5) long l5; long l6; };\n",          IDL_RETCODE_OK,             {IDL_SEQUENTIAL, IDL_SEQUENTIAL, IDL_SEQUENTIAL}, {true, true, true, true, true, false},  {2, 4, 1, 3, 5, 6}},  //explicit ids in inherited classes, sequential continuation
    {"struct baz { @id(1) long l1; @id(2) long l2; };\n"
     "struct bar { @id(1) long l1; @id(2) long l2; };\n"
     "struct foo : bar { baz b1; long l; };\n",                   IDL_RETCODE_OK,             {IDL_SEQUENTIAL, IDL_SEQUENTIAL, IDL_SEQUENTIAL}, {true, true, true, true, false},  {1, 2, 1, 2, 3, 4}},        //member ids of baz should not conflict with those in bar
    {"struct baz { @id(1) long l1; @id(3) long l3; };\n"
     "struct bar : baz { @id(2) long l2; @id(4) long l4; };\n"
     "struct foo : bar { @id(0) long l5; long l6; };\n",          IDL_RETCODE_SEMANTIC_ERROR, {0},                                              {0},  {0}},                                                   //foo::l6 clashes with baz::l1
    {"struct base { char c; char d; };\n"
     "struct derived : base { @id(1) char e; char f; };",         IDL_RETCODE_SEMANTIC_ERROR, {0},                                              {0},  {0}},                                                   //derived::e clashes with base::d
    {"@autoid(HASH) struct base { char c; char d; };\n"
     "struct derived : base { @hashid(\"c\") char e; char f; };", IDL_RETCODE_SEMANTIC_ERROR, {0},                                              {0},  {0}},                                                   //derived::e (hash(c)) clashes with base::c
    {"struct baz { @id(2) long l2; @id(4) long l4; };\n"
     "struct bar : baz { @id(1) long l1; @id(3) long l3; };\n"
     "struct foo : bar { long l; };\n",                           IDL_RETCODE_SEMANTIC_ERROR, {0},                                              {0},  {0}},                                                   //foo::l attempts to take id 4, but this is already assigned to baz::l4
  };

  for (size_t i = 0; i < sizeof(tests)/sizeof(id_test_t); i++)
    test_id(tests[i]);
}

CU_Test(idl_annotation, autoid_module)
{
  static const id_test_t tests[] = {
    {"module m { struct s { char c; char d; }; };",                                 IDL_RETCODE_OK,             {IDL_SEQUENTIAL, IDL_SEQUENTIAL},     {0}, {0, 1}},               //implicit sequential autoid on module and struct inherits
    {"@autoid module m { struct s { char c; }; };",                                 IDL_RETCODE_OK,             {IDL_HASH, IDL_HASH},                 {0}, {0x00088a4au}},        //implicit hash autoid on module and struct inherits
    {"@autoid(HASH) module m {\n"
     "  @autoid(SEQUENTIAL) struct s { char c; char d; };\n"
     "  struct t { char c; };\n"
     "};",                                                                          IDL_RETCODE_OK,             {IDL_HASH, IDL_SEQUENTIAL, IDL_HASH}, {0}, {0, 1, 0x00088a4au}},  //explicit hash autoid on module, one struct hash explicit sequential, the other inherits
    {"@autoid(SEQUENTIAL) @autoid(SEQUENTIAL) module m { struct s { char c; }; };", IDL_RETCODE_OK,             {IDL_SEQUENTIAL, IDL_SEQUENTIAL},     {0}, {0}},                  //duplicate autoid declarations
    {"@autoid @autoid(HASH) module m { struct s { char c; }; };",                   IDL_RETCODE_OK,             {IDL_HASH, IDL_HASH},                 {0}, {0x00088a4au}},        //duplicate autoid declarations
    {"@autoid module m1 {\n"
     "@autoid(SEQUENTIAL) module m2 { struct s { char c; char d; }; };\n"
     "module m3 { struct s { char c; char d; }; };\n"
     "};",                                                                          IDL_RETCODE_OK,             {IDL_HASH, IDL_SEQUENTIAL,
                                                                                                                 IDL_SEQUENTIAL, IDL_HASH, IDL_HASH}, {0}, {0, 1, 0x00088a4au, 0x01e07782u}},  //mixing module level autoids
    {"@autoid module m1 { @autoid(SEQUENTIAL) module m2 {\n"
     "@autoid module m3 { @autoid(SEQUENTIAL) module m4 {\n"
     "  struct s { char c; char d; };\n"
     "}; }; }; };",                                                                 IDL_RETCODE_OK,             {IDL_HASH, IDL_SEQUENTIAL, IDL_HASH,
                                                                                                                 IDL_SEQUENTIAL, IDL_SEQUENTIAL},     {0}, {0, 1}},               //overwriting module level autoids
    {"@autoid(HASH) module m1 {\n"
     "  struct s { char c; @hashid(\"c\") char d; };\n"
     "};",                                                                          IDL_RETCODE_SEMANTIC_ERROR, {0},                                  {0}, {0}},                  //s::c clashes with s:::d
    {"@autoid(SEQUENTIAL) module m {\n"
     "  struct s { @id(1) char c1; @id(0) char c0; char c; };\n"
     "};",                                                                          IDL_RETCODE_SEMANTIC_ERROR, {0},                                  {0}, {0}},                  //s::c clashes with s::c1
    {"@autoid @autoid(SEQUENTIAL) module m { struct s { char c; }; };",             IDL_RETCODE_SEMANTIC_ERROR, {0},                                  {0}, {0}}                   //clashing autoid declarations
  };

  for (size_t i = 0; i < sizeof(tests)/sizeof(id_test_t); i++)
    test_id(tests[i]);
}

// x. do not allow annotation_appl in annotation

#define S(ann) ann " struct s { char c; };"
#define U(ann) ann " union u switch(short) { case 1: char c; };"
#define BM(ann) ann " bitmask bm { bm0, bm1; };"
#define E(ann) ann " enum e { enum0, enum1; };"
CU_Test(idl_annotation, extensibility)
{
  static const struct {
    idl_type_t type;
    const char *str;
    enum idl_extensibility ext;
    idl_retcode_t ret;
  } tests[] = {
    { IDL_STRUCT, S("@final"), IDL_FINAL, IDL_RETCODE_OK },
    { IDL_STRUCT, S("@appendable"), IDL_APPENDABLE, IDL_RETCODE_OK },
    { IDL_STRUCT, S("@mutable"), IDL_MUTABLE, IDL_RETCODE_OK },
    { IDL_STRUCT, S("@extensibility(FINAL)"), IDL_FINAL, IDL_RETCODE_OK },
    { IDL_STRUCT, S("@extensibility(APPENDABLE)"), IDL_APPENDABLE, IDL_RETCODE_OK },
    { IDL_STRUCT, S("@extensibility(MUTABLE)"), IDL_MUTABLE, IDL_RETCODE_OK },
    { IDL_UNION, U("@mutable"), IDL_MUTABLE, IDL_RETCODE_OK },
    { IDL_UNION, U("@extensibility(APPENDABLE)"), IDL_APPENDABLE, IDL_RETCODE_OK },
    //clashes with datarepresentation
    { IDL_STRUCT, S("@mutable @data_representation(XCDR1)"), IDL_MUTABLE, IDL_RETCODE_SEMANTIC_ERROR },
    { IDL_UNION, U("@mutable @data_representation(XCDR1)"), IDL_MUTABLE, IDL_RETCODE_SEMANTIC_ERROR },

    /* FIXME: extensibility on bitmask and enum not supported yet
        (both can be final or appendable, not mutable */
    { IDL_BITMASK, BM("@appendable"), 0, IDL_RETCODE_SYNTAX_ERROR },
    { IDL_BITMASK, BM("@mutable"), 0, IDL_RETCODE_SYNTAX_ERROR },
    { IDL_ENUM, E("@appendable"), 0, IDL_RETCODE_SYNTAX_ERROR },
    { IDL_ENUM, E("@mutable"), 0, IDL_RETCODE_SYNTAX_ERROR },
  };
  static const size_t n = sizeof(tests)/sizeof(tests[0]);

  idl_retcode_t ret;
  idl_pstate_t *pstate = NULL;

  for (size_t i = 0; i < n; i++) {
    printf("idl: %s\n", tests[i].str);
    pstate = NULL;
    ret = parse_string(IDL_FLAG_ANNOTATIONS, tests[i].str, &pstate);
    CU_ASSERT_EQUAL_FATAL(ret, tests[i].ret);
    if (tests[i].ret == IDL_RETCODE_OK) {
      CU_ASSERT_PTR_NOT_NULL_FATAL(pstate);
      assert(pstate);
      switch (tests[i].type) {
        case IDL_STRUCT: {
          idl_struct_t *s = (idl_struct_t *)pstate->root;
          CU_ASSERT_PTR_NOT_NULL_FATAL(s);
          assert(s);
          CU_ASSERT_FATAL(idl_is_struct(s));
          CU_ASSERT_EQUAL(s->extensibility.value, tests[i].ext);
          break;
        }
        case IDL_UNION: {
          idl_union_t *u = (idl_union_t *)pstate->root;
          CU_ASSERT_PTR_NOT_NULL_FATAL(u);
          assert(u);
          CU_ASSERT_FATAL(idl_is_union(u));
          CU_ASSERT_EQUAL(u->extensibility.value, tests[i].ext);
          break;
        }
        default:
          CU_FAIL_FATAL("Unexpected type");
      }
      idl_delete_pstate(pstate);
    }
  }
}
#undef S
#undef U
#undef BM
#undef E

#if 0

CU _ Test(idl_annotation, foobar_struct)
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
#endif

#define E(name, definitions) " enum " name " { " definitions " };\n"
#define C(name, value) " const long " name " = " value ";\n"
#define M(name, definitions) " module " name " {\n " definitions "\n};\n"
#define A(name, definitions) " @annotation " name " {\n " definitions "\n};\n"
#define TA(ann) \
  E("gkind", "GKIND1, GKIND2")\
  C("gv", "1") \
  M("m1", \
    E("m1kind", "M1KIND1, M1KIND2") \
    C("m1v", "1") \
    A("a1", "long v;")\
    A("a2", E("a2kind", "KIND1, KIND2") "a2kind v;")\
    M("m2", \
      E("m2kind", "M2KIND1, M2KIND2") \
      C("m2v", "1") \
      ann\
      "struct s { char c1; };"\
    )\
  )

CU_Test(idl_annotation, parameter_scope)
{
  static const struct {
    const char *str;
  } tests[] = {
    { TA("@a1(v = 1)") },
    { TA("@a1(v = m2::m2v)") },
    { TA("@a1(v = m1::m2::m2v)") },
    { TA("@a1(v = m1::m1v)") },
    { TA("@a1(v = ::gv)") },

    { TA("@a2(v = KIND1)") },
    { TA("@a2(v = m2::M2KIND1)") },
    { TA("@a2(v = m1::m2::M2KIND1)") },
    { TA("@a2(v = m1::M1KIND1)") },
    { TA("@a2(v = ::GKIND1)") },
  };
  static const size_t n = sizeof(tests)/sizeof(tests[0]);

  idl_retcode_t ret;
  idl_pstate_t *pstate = NULL;

  for (size_t i = 0; i < n; i++) {
    pstate = NULL;
    ret = parse_string(IDL_FLAG_ANNOTATIONS, tests[i].str, &pstate);
    CU_ASSERT_EQUAL_FATAL(ret, IDL_RETCODE_OK);
    CU_ASSERT_PTR_NOT_NULL_FATAL(pstate);
    idl_delete_pstate(pstate);
  }
}

#undef E
#undef C
#undef M
#undef A
#undef TA

CU_Test(idl_annotation, identifier_clash)
{
  static const struct {
    const char *str;
    idl_retcode_t ret;
  } tests[] = {
    { "struct key { @key long f1; };", IDL_RETCODE_OK },
    { "struct Key { @key long f1; };", IDL_RETCODE_OK },
    { "struct external { long f1; };", IDL_RETCODE_OK },
    { "module m1 { struct key { long f1; }; }; struct b { @key long f1; };", IDL_RETCODE_OK },
    { "@annotation key { }; struct a { @key long f1; };", IDL_RETCODE_SEMANTIC_ERROR },
    { "module m1 { @annotation key { }; struct a { @key long f1; }; };", IDL_RETCODE_OK }, // uses the custom @key annotation, so field is not a key!
    { "@annotation a1 { }; struct a1 { long f1; };", IDL_RETCODE_OK },
    { "@annotation a1 { }; @annotation a1 { }; struct b { @a1 long f1; };", IDL_RETCODE_OK },
    { "@annotation a1 { }; @annotation a1 { unsigned long value; }; struct b { @a1 long f1; };", IDL_RETCODE_SEMANTIC_ERROR },
    { "@annotation a1 { }; @a1 struct a2 { @a1 long f1; };", IDL_RETCODE_OK },
    { "module m1 { @annotation a1 { }; }; struct a1 { long f1; };", IDL_RETCODE_OK },
    { "module m1 { struct a1 { long f1; }; }; @annotation a1 { };", IDL_RETCODE_OK }
  };
  static const size_t n = sizeof(tests)/sizeof(tests[0]);

  idl_retcode_t ret;
  idl_pstate_t *pstate = NULL;

  for (size_t i = 0; i < n; i++) {
    pstate = NULL;
    ret = parse_string(IDL_FLAG_ANNOTATIONS, tests[i].str, &pstate);
    CU_ASSERT_EQUAL_FATAL(ret, tests[i].ret);
    if (tests[i].ret == IDL_RETCODE_OK)
    {
      CU_ASSERT_PTR_NOT_NULL_FATAL(pstate);
      idl_delete_pstate(pstate);
    }
  }
}

#define BM(i) "@bit_bound(" i ") bitmask MyBitMask { flag0 };"
#define E(i) "@bit_bound(" i ") enum MyEnum { ENUM1 };"
CU_Test(idl_annotation, bit_bound)
{
  static const struct {
    const char *str;
    bool valid;
    uint16_t value;
  } tests[] = {
    /* valid */
    { BM("1"), true, 1 },
    { BM("8"), true, 8 },
    { BM("42"), true, 42 },
    { BM("64"), true, 64 },
    { "bitmask MyBitMask { flag0 };", true, 32 },
    { E("1"), true, 1 },
    { E("21"), true, 21 },
    { E("32"), true, 32 },
    { "enum MyEnum { ENUM1 };", true, 32 },
    { "@bit_bound(1) enum MyEnum { ENUM1, ENUM2 };", true, 1 },
    { "@bit_bound(3) enum MyEnum { ENUM1, @value (7) ENUM2 };", true, 3 },
    /* invalid */
    { BM("0"), false, 0 },
    { BM("65"), false, 0 },
    { E("0"), false, 0 },
    { E("33"), false, 0 },
    { "@bit_bound(1) bitmask MyBitMask { flag0, flag1 };", false, 0 },
    { "@bit_bound(1) enum MyEnum { ENUM1, ENUM2, ENUM3 };", false, 0 },
    { "@bit_bound(2) enum MyEnum { ENUM1, @value (4) ENUM2 };", false, 0 },
  };
  static const size_t n = sizeof(tests)/sizeof(tests[0]);

  idl_retcode_t ret;
  idl_pstate_t *pstate;

  for (size_t i = 0; i < n; i++) {
    printf("idl_annotation.bit_bound test: %s\n", tests[i].str);
    pstate = NULL;
    ret = parse_string(IDL_FLAG_ANNOTATIONS, tests[i].str, &pstate);
    if (tests[i].valid) {
      CU_ASSERT_EQUAL_FATAL(ret, IDL_RETCODE_OK);
    } else {
      CU_ASSERT_NOT_EQUAL_FATAL(ret, IDL_RETCODE_OK);
      continue;
    }
    CU_ASSERT_PTR_NOT_NULL_FATAL(pstate);
    assert(pstate);
    CU_ASSERT_PTR_NOT_NULL_FATAL(pstate->root);
    assert(pstate->root);
    if (idl_is_bitmask(pstate->root)) {
      idl_bitmask_t *b = (idl_bitmask_t *)pstate->root;
      CU_ASSERT_EQUAL_FATAL(b->bit_bound.value, tests[i].value);
    } else if (idl_is_enum(pstate->root)) {
      idl_enum_t *e = (idl_enum_t *)pstate->root;
      CU_ASSERT_EQUAL_FATAL(e->bit_bound.value, tests[i].value);
    } else {
      CU_FAIL_FATAL("Invalid data type");
    }
    idl_delete_pstate(pstate);
  }
}

#undef BM
#undef E

#define BM(p0, p1, p2, p3) "@bit_bound(16) bitmask MyBitMask { " p0 " flag0, " p1 " flag1, " p2 " flag2, " p3 " flag3 };"
CU_Test(idl_annotation, position)
{
  static const struct {
    const char *str;
    bool valid;
    uint16_t p[4];
  } tests[] = {
    /* valid */
    { BM("", "", "", ""), true, { 0, 1, 2, 3 } },
    { BM("@position(1)", "", "", ""), true, { 1, 2, 3, 4 } },
    { BM("", "@position(3)", "", "@position(6)"), true, { 0, 3, 4, 6 } },
    { BM("", "", "@position(3)", ""), true, { 0, 1, 3, 4 } },
    { BM("@position(10)", "", "@position(5)", ""), true, { 10, 11, 5, 6 } },
    { BM("@position(12)", "", "", ""), true, { 12, 13, 14, 15 } },
    /* invalid */
    { BM("", "", "", "@position(2)"), false, { 0, 0, 0, 0 } },
    { BM("@position(-1)", "", "", ""), false, { 0, 0, 0, 0 } },
    { BM("", "@position(0)", "", ""), false, { 0, 0, 0, 0 } },
    { BM("@position(10)", "", "@position(9)", ""), false, { 0, 0, 0, 0 } },
    { BM("@position(13)", "", "", ""), false, { 0, 0, 0, 0 } },
  };
  static const size_t n = sizeof(tests)/sizeof(tests[0]);

  idl_retcode_t ret;
  idl_pstate_t *pstate;

  for (size_t i = 0; i < n; i++) {
    printf("idl_annotation.position test: %s\n", tests[i].str);
    pstate = NULL;
    ret = parse_string(IDL_FLAG_ANNOTATIONS, tests[i].str, &pstate);
    if (tests[i].valid) {
      CU_ASSERT_EQUAL_FATAL(ret, IDL_RETCODE_OK);
    } else {
      CU_ASSERT_NOT_EQUAL_FATAL(ret, IDL_RETCODE_OK);
      continue;
    }
    CU_ASSERT_PTR_NOT_NULL_FATAL(pstate);
    assert(pstate);
    CU_ASSERT_PTR_NOT_NULL_FATAL(pstate->root);
    assert(pstate->root);
    CU_ASSERT_FATAL(idl_is_bitmask(pstate->root));
    idl_bitmask_t *b = (idl_bitmask_t *)pstate->root;
    idl_bit_value_t *bv = b->bit_values;
    for (int j = 0; j <= 3; bv = idl_next(bv), j++) {
      CU_ASSERT_PTR_NOT_NULL_FATAL(bv);
      assert(bv);
      CU_ASSERT_EQUAL(bv->position.value, tests[i].p[j]);
    }
    idl_delete_pstate(pstate);
  }
}

#undef BM

typedef struct mu_test {
  const char *s;
  idl_retcode_t ret;
  bool val[8];
  bool annotated[8];
} mu_test_t;

static void test_must_understand(mu_test_t test)
{
  idl_pstate_t *pstate = NULL;
  idl_retcode_t ret = parse_string(IDL_FLAG_ANNOTATIONS, test.s, &pstate);
  CU_ASSERT_EQUAL(ret, test.ret);

  if (ret)
    return;

  size_t i = 0;
  if (idl_is_struct(pstate->root)) {
    const idl_struct_t *s = (const idl_struct_t*)pstate->root;

    const idl_member_t *mem = NULL;
    IDL_FOREACH(mem, s->members) {
      CU_ASSERT_EQUAL(mem->must_understand.value, test.val[i]);
      if (test.annotated[i]) {
        CU_ASSERT_PTR_NOT_NULL(mem->must_understand.annotation);
      } else {
        CU_ASSERT_PTR_NULL(mem->must_understand.annotation);
      }
      i++;
    }
  }

  idl_delete_pstate(pstate);
}

CU_Test(idl_annotation, must_understand)
{
  mu_test_t tests[] = {
    {"@mutable struct s { char c; @must_understand char d; @must_understand(false) char e; @key @must_understand(true) char f; };", IDL_RETCODE_OK, {false, true, false, true}, {false, true, true, true} },
    {"@final struct s { @must_understand char c;  };", IDL_RETCODE_SEMANTIC_ERROR, {false}, {false} },
    {"@final struct s { @must_understand(false) char c;  };", IDL_RETCODE_OK, {false}, {true} },
    {"@appendable struct s { @must_understand char c;  };", IDL_RETCODE_SEMANTIC_ERROR, {false}, {false} },
    {"@appendable struct s { @must_understand(false) char c;  };", IDL_RETCODE_OK, {false}, {true} },
    {"struct s { @key @must_understand(false) char c;  };", IDL_RETCODE_SEMANTIC_ERROR, {false}, {false} },
    {"@must_understand struct s { char b; char c; };", IDL_RETCODE_SEMANTIC_ERROR, {false}, {false} },
    {"@must_understand module m { struct s { char b; char c; }; }; ", IDL_RETCODE_SEMANTIC_ERROR, {false}, {false} },
    {"union u switch(long) { case 0: @must_understand char c; default: string s; };", IDL_RETCODE_SEMANTIC_ERROR, {false}, {false} },
    {"@must_understand union u switch(long) { case 0: char c; default: string s; };", IDL_RETCODE_SEMANTIC_ERROR, {false}, {false} },
  };

  for (size_t i = 0; i < sizeof(tests)/sizeof(tests[0]); i++) {
    test_must_understand(tests[i]);
  }
}

typedef struct tc_test {
  const char *s;
  idl_retcode_t ret;
  bool defaulted;
  idl_try_construct_t tc;
} tc_test_t;


static void test_try_construct(tc_test_t test)
{
  idl_pstate_t *pstate = NULL;
  idl_retcode_t ret = parse_string(IDL_FLAG_ANNOTATIONS, test.s, &pstate);
  CU_ASSERT_EQUAL(ret, test.ret);

  if (ret)
    return;

  if (idl_is_struct(pstate->root)) {
    const idl_member_t *mem = ((const idl_struct_t*)pstate->root)->members;
    CU_ASSERT_EQUAL(test.defaulted, mem->try_construct.annotation == NULL);
    CU_ASSERT_EQUAL(test.tc, mem->try_construct.value);
  } else if (idl_is_union(pstate->root)) {
    const idl_case_t *cs = ((const idl_union_t*)pstate->root)->cases;
    CU_ASSERT_EQUAL(test.defaulted, cs->try_construct.annotation == NULL);
    CU_ASSERT_EQUAL(test.tc, cs->try_construct.value);
  } else {
    CU_FAIL("Invalid data type");
  }

  idl_delete_pstate(pstate);
}

#define U(annotation, field_type) "union u switch(char) { case 'a': " annotation " " field_type " l; };"
#define U_L(annotation) U(annotation, "long")
#define U_D(annotation) U(annotation, "double")
#define S(annotation, type, bound) "struct s { " annotation " " type bound " mem;};"
#define S_L(annotation) S(annotation, "long", "")
#define S_D(annotation) S(annotation, "double", "")

CU_Test(idl_annotation, try_construct)
{
  tc_test_t tests[] = {
    {"@try_construct module m { struct s { char c; }; };",
      IDL_RETCODE_SEMANTIC_ERROR},
    {U_L(""),
      IDL_RETCODE_OK, true,   IDL_DISCARD},
    {U_L("@try_construct"),
      IDL_RETCODE_OK, false,  IDL_USE_DEFAULT},
    {U_L("@try_construct(DISCARD)"),
      IDL_RETCODE_OK, false,  IDL_DISCARD},
    {U_L("@try_construct(USE_DEFAULT)"),
      IDL_RETCODE_OK, false,  IDL_USE_DEFAULT},
    {U_L("@try_construct(NONSENSE)"),
      IDL_RETCODE_SEMANTIC_ERROR},
    {S_L("@try_construct"),
      IDL_RETCODE_OK, false, IDL_USE_DEFAULT},
    {S_L("@try_construct(USE_DEFAULT)"),
      IDL_RETCODE_OK, false, IDL_USE_DEFAULT},
    {S_L("@try_construct(DISCARD)"),
      IDL_RETCODE_OK, false, IDL_DISCARD},
    {S_L("@try_construct(TRIM)"),
      IDL_RETCODE_SEMANTIC_ERROR},
    {S("@try_construct(TRIM)", "string", "<5>"),
      IDL_RETCODE_OK, false, IDL_TRIM},
  };

  for (size_t i = 0; i < sizeof(tests)/sizeof(tests[0]); i++) {
    test_try_construct(tests[i]);
  }
}

typedef struct minmax_test {
  const char *s;
  idl_retcode_t ret;
  bool min_present;
  bool max_present;
  double min;
  double max;
} minmax_test_t;

static void validate_limit(const idl_literal_t *lit, double to_test, double granularity)
{
  assert(lit);
  double fval = 0;
  idl_type_t type = idl_type(lit);
  if (type & IDL_INTEGER_TYPE) {
    if (type & IDL_UNSIGNED)
      fval = (double)lit->value.uint64;
    else
      fval = (double)lit->value.int64;
  } else {
    switch (type) {
      case IDL_FLOAT:
        fval = (double)lit->value.flt;
        break;
      case IDL_DOUBLE:
        fval = (double)lit->value.dbl;
        break;
      case IDL_LDOUBLE:
        fval = (double)lit->value.ldbl;
        break;
      default:
        CU_ASSERT(false);
    }
  }

  CU_ASSERT_DOUBLE_EQUAL(fval, to_test, granularity);
}

static void test_min_max(minmax_test_t test)
{
  idl_pstate_t *pstate = NULL;
  idl_retcode_t ret = parse_string(IDL_FLAG_ANNOTATIONS, test.s, &pstate);
  CU_ASSERT_EQUAL(ret, test.ret);

  if (ret)
    return;

  if (idl_is_struct(pstate->root)) {
    const idl_member_t *mem = ((const idl_struct_t*)pstate->root)->members;
    CU_ASSERT_EQUAL(test.min_present, mem->min.annotation != NULL);
    if (mem->min.annotation)
      validate_limit(mem->min.value,test.min, 0.000001);
    CU_ASSERT_EQUAL(test.max_present, mem->max.annotation != NULL);
    if (mem->max.annotation)
      validate_limit(mem->max.value,test.max, 0.000001);
  } else if (idl_is_union(pstate->root)) {
    const idl_case_t *cs = ((const idl_union_t*)pstate->root)->cases;
    CU_ASSERT_EQUAL(test.min_present, cs->min.annotation != NULL);
    if (cs->min.annotation)
      validate_limit(cs->min.value,test.min, 0.000001);
    CU_ASSERT_EQUAL(test.max_present, cs->max.annotation != NULL);
    if (cs->max.annotation)
      validate_limit(cs->max.value,test.max, 0.000001);
  } else {
    CU_FAIL("Invalid data type");
  }

  idl_delete_pstate(pstate);
}

CU_Test(idl_annotation, limits)
{
  minmax_test_t tests[] = {
    //unsupported annotation
    {"@min(0) module m { struct s { char c; }; };", IDL_RETCODE_SEMANTIC_ERROR},
    {"@max(10) module m { struct s { char c; }; };", IDL_RETCODE_SEMANTIC_ERROR},
    {"@range(min = 0, max = 10) module m { struct s { char c; }; };", IDL_RETCODE_SEMANTIC_ERROR},
    {U_L("@min"), IDL_RETCODE_SEMANTIC_ERROR},
    {U_L("@max"), IDL_RETCODE_SEMANTIC_ERROR},
    {U_L("@range"), IDL_RETCODE_SEMANTIC_ERROR},
    {U_L("@min(3"), IDL_RETCODE_SYNTAX_ERROR},
    {U_L("@min(\"Some String\")"), IDL_RETCODE_SEMANTIC_ERROR},
    {U_L("@max(\"Some String\")"), IDL_RETCODE_SEMANTIC_ERROR},
    {U_L("@range(min = \"Some String\", max = \"Some other string\")"), IDL_RETCODE_SEMANTIC_ERROR},
    //nothing
    {U_L(""), IDL_RETCODE_OK, false, false},
    {S_L(""), IDL_RETCODE_OK, false, false},
    //int parameter on int field
    {U_L("@min(5)"), IDL_RETCODE_OK, true, false, 5},
    {U_L("@max(10)"), IDL_RETCODE_OK, false, true, 0, 10},
    {U_L("@range(min = 5, max = 10)"), IDL_RETCODE_OK, true, true, 5, 10},
    {S_L("@min(5)"), IDL_RETCODE_OK, true, false, 5},
    {S_L("@max(10)"), IDL_RETCODE_OK, false, true, 0, 10},
    {S_L("@range(min = 5, max = 10)"), IDL_RETCODE_OK, true, true, 5, 10},
    //double parameter on int field
    {U_L("@min(2.71828)"), IDL_RETCODE_SEMANTIC_ERROR},
    {U_L("@max(3.1415)"), IDL_RETCODE_SEMANTIC_ERROR},
    {U_L("@range(min = 2.71828, max = 3.1415)"), IDL_RETCODE_SEMANTIC_ERROR},
    {S_L("@min(2.71828)"), IDL_RETCODE_SEMANTIC_ERROR},
    {S_L("@max(3.1415)"), IDL_RETCODE_SEMANTIC_ERROR},
    {S_L("@range(min = 2.71828, max = 3.1415)"), IDL_RETCODE_SEMANTIC_ERROR},
    //double parameter on double field
    {U_D("@min(2.71828)"), IDL_RETCODE_OK, true, false, 2.71828},
    {U_D("@max(3.1415)"), IDL_RETCODE_OK, false, true, 0, 3.1415},
    {U_D("@range(min = 2.71828, max = 3.1415)"), IDL_RETCODE_OK, true, true, 2.71828, 3.1415},
    {S_D("@min(2.71828)"), IDL_RETCODE_OK, true, false, 2.71828},
    {S_D("@max(3.1415)"), IDL_RETCODE_OK, false, true, 0, 3.1415},
    {S_D("@range(min = 2.71828, max = 3.1415)"), IDL_RETCODE_OK, true, true, 2.71828, 3.1415},
    //int parameter on double field
    {U_D("@min(5)"), IDL_RETCODE_OK, true, false, 5},
    {U_D("@max(10)"), IDL_RETCODE_OK, false, true, 0, 10},
    {U_D("@range(min = 5, max = 10)"), IDL_RETCODE_OK, true, true, 5, 10},
    {S_D("@min(5)"), IDL_RETCODE_OK, true, false, 5},
    {S_D("@max(10)"), IDL_RETCODE_OK, false, true, 0, 10},
    {S_D("@range(min = 5, max = 10)"), IDL_RETCODE_OK, true, true, 5, 10},
    //unsupported field type
    {U("@min(0)", "string"), IDL_RETCODE_SEMANTIC_ERROR},
    {U("@max(0)", "string"), IDL_RETCODE_SEMANTIC_ERROR},
    {U("@range(min = 5, max = 10)", "string"), IDL_RETCODE_SEMANTIC_ERROR},
    {S("@min(0)", "string", ""), IDL_RETCODE_SEMANTIC_ERROR},
    {S("@max(0)", "string", ""), IDL_RETCODE_SEMANTIC_ERROR},
    {S("@range(min = 5, max = 10)", "string", ""), IDL_RETCODE_SEMANTIC_ERROR},
    //attempting to double set
    {U_L("@min(3) @range(min = 4, max = 5)"), IDL_RETCODE_SEMANTIC_ERROR},
    {U_L("@range(min = 4, max = 5) @min(3)"), IDL_RETCODE_SEMANTIC_ERROR},
    {S_L("@min(3) @range(min = 4, max = 5)"), IDL_RETCODE_SEMANTIC_ERROR},
    {S_L("@range(min = 4, max = 5) @min(3)"), IDL_RETCODE_SEMANTIC_ERROR},
    //inaccurate comparisons
    {"struct s { @range(min = 9007199254740993, max = 9007199254740993.5) double mem; };", IDL_RETCODE_SEMANTIC_ERROR},
  };

  for (size_t i = 0; i < sizeof(tests)/sizeof(tests[0]); i++) {
    test_min_max(tests[i]);
  }
}

typedef struct unit_test {
  const char *s;
  idl_retcode_t ret;
  const char *unitstr;
} unit_test_t;

static void test_unit(unit_test_t test)
{
  idl_pstate_t *pstate = NULL;
  idl_retcode_t ret = parse_string(IDL_FLAG_ANNOTATIONS, test.s, &pstate);
  CU_ASSERT_EQUAL(ret, test.ret);

  if (ret)
    return;

  if (idl_is_struct(pstate->root)) {
    const idl_member_t *mem = ((const idl_struct_t*)pstate->root)->members;
    CU_ASSERT_EQUAL(test.unitstr != NULL, mem->unit.value != NULL);
    if (test.unitstr&& mem->unit.value) {
        CU_ASSERT_STRING_EQUAL(test.unitstr, mem->unit.value);
    }
  } else if (idl_is_union(pstate->root)) {
    const idl_case_t *cs = ((const idl_union_t*)pstate->root)->cases;
    CU_ASSERT_EQUAL(test.unitstr != NULL, cs->unit.value != NULL);
    if (test.unitstr&& cs->unit.value) {
        CU_ASSERT_STRING_EQUAL(test.unitstr, cs->unit.value);
    }
  } else {
    CU_FAIL("Invalid data type");
  }

  idl_delete_pstate(pstate);
}

CU_Test(idl_annotation, units)
{
  unit_test_t tests[] = {
    //unsupported annotations
    {"@unit(\"Watt\") module m { struct s { char c; }; };", IDL_RETCODE_SEMANTIC_ERROR},
    {U_L("@unit"), IDL_RETCODE_SEMANTIC_ERROR},
    {U_L("@unit(0.1234)"), IDL_RETCODE_ILLEGAL_EXPRESSION},
    //allowed annotations
    {U_L("@unit(\"Watt\")"), IDL_RETCODE_OK, "Watt"},
    {S_L("@unit(\"Watt\")"), IDL_RETCODE_OK, "Watt"},
  };

  for (size_t i = 0; i < sizeof(tests)/sizeof(tests[0]); i++) {
    test_unit(tests[i]);
  }
}

#undef U
#undef U_L
#undef U_D
#undef S
#undef S_L
#undef S_D

typedef struct rep_test {
  const char *s;
  idl_retcode_t ret;
  allowable_data_representations_t reps[4];
  size_t i;
} rep_test_t;

static idl_retcode_t
test_rep(
  const idl_pstate_t* pstate,
  const bool revisit,
  const idl_path_t* path,
  const void* node,
  void* user_data)
{
  (void) pstate;
  (void) revisit;
  (void) path;

  rep_test_t *test = (rep_test_t*)user_data;

  allowable_data_representations_t
    allowed = idl_allowable_data_representations(node),
    expected = test->reps[test->i++];

  CU_ASSERT_EQUAL(expected, allowed);

  return expected == allowed ? IDL_RETCODE_OK : IDL_RETCODE_SEMANTIC_ERROR;
}

static void test_representation(rep_test_t test)
{
  idl_pstate_t *pstate = NULL;
  idl_retcode_t ret = parse_string(IDL_FLAG_ANNOTATIONS, test.s, &pstate);
  CU_ASSERT_EQUAL(ret, test.ret);

  if (ret)
    return;

  idl_visitor_t visitor;
  memset(&visitor, 0, sizeof(visitor));
  visitor.visit = IDL_STRUCT | IDL_MODULE | IDL_UNION;
  visitor.accept[IDL_ACCEPT_STRUCT] = &test_rep;
  visitor.accept[IDL_ACCEPT_MODULE] = &test_rep;
  visitor.accept[IDL_ACCEPT_UNION] = &test_rep;
  (void) idl_visit(pstate, pstate->root, &visitor, &test);

  idl_delete_pstate(pstate);
}

#define U(name, val)\
"@data_representation(" val ") union " name " switch(char) {\n case 'a': long l;\n};\n"
#define S(name, val)\
"@data_representation(" val ") struct " name "{\nlong l;\n};\n"
#define M(name, val, etc)\
"@data_representation(" val ") module " name " {\n" etc "};\n"
#define XCDR1 IDL_DATAREPRESENTATION_FLAG_XCDR1
#define XCDR2 IDL_DATAREPRESENTATION_FLAG_XCDR2
#define XML IDL_DATAREPRESENTATION_FLAG_XML
#define DEFAULT IDL_ALLOWABLE_DATAREPRESENTATION_DEFAULT

CU_Test(idl_annotation, data_representation)
{
  rep_test_t tests[] = {
    //unsupported annotations
    {"@data_representation module m { struct s { char c; }; };", IDL_RETCODE_SEMANTIC_ERROR},
    {"@data_representation(1) enum e { e_0, e_1, e_2 };", IDL_RETCODE_SEMANTIC_ERROR},
    //on modules, should also propagate down
    {M("m","1","struct s {char c;};"), IDL_RETCODE_OK, {1, 1} },
    {M("m","XCDR1","struct s {char c;};"), IDL_RETCODE_OK, {XCDR1, XCDR1} },
    {M("m","6", S("s","2") U("u","4")), IDL_RETCODE_OK, {6, 2, 4} },
    {M("m","XML|XCDR2", S("s","XML") U("u","XCDR2")), IDL_RETCODE_OK, {XML | XCDR2, XML, XCDR2} },
    //on structs
    {S("s","2"), IDL_RETCODE_OK, {2} },
    {S("s","XCDR1|XCDR2"), IDL_RETCODE_OK, {XCDR1 | XCDR2} },
    //on unions
    {U("u","4"), IDL_RETCODE_OK, {4} },
    {U("u","XML"), IDL_RETCODE_OK, {XML} },
    //clashes with extensibility
    {"@data_representation(XCDR1) @mutable struct a { long f1; };", IDL_RETCODE_SEMANTIC_ERROR },
    {"@data_representation(XCDR1) @mutable union u switch (long) { case 1: long f1; };", IDL_RETCODE_SEMANTIC_ERROR },
    //default
    {"struct a { long f1; };", IDL_RETCODE_OK, {DEFAULT} },
    {"struct b { long b1; }; struct a : b { long f1; };", IDL_RETCODE_OK, {DEFAULT, DEFAULT} },
    {"union u switch(long) { case 1: long f1; };", IDL_RETCODE_OK, {DEFAULT} },
    {"module m { struct a { long f1; }; };", IDL_RETCODE_OK, {DEFAULT, DEFAULT} }
  };

  for (size_t i = 0; i < sizeof(tests)/sizeof(tests[0]); i++) {
    test_representation(tests[i]);
  }
}

#undef U
#undef S
#undef M
#undef XCDR1
#undef XCDR2
#undef XML
#undef DEFAULT

CU_Test(idl_annotation, idl_is_string_fix)
{
  idl_pstate_t *pstate = NULL;
  idl_retcode_t ret = parse_string(IDL_FLAG_ANNOTATIONS, "struct s { @default(\"abcdef\") string str;};", &pstate);
  CU_ASSERT_EQUAL(ret, IDL_RETCODE_OK);

  if (ret)
    return;

  const idl_struct_t *_struct = (const idl_struct_t*)pstate->root;
  CU_ASSERT_FATAL(idl_is_struct(pstate->root));

  const idl_member_t *_member = _struct->members;
  CU_ASSERT_FATAL(idl_is_member(_member));

  CU_ASSERT_FATAL(_member->value.annotation != NULL);
  CU_ASSERT(idl_is_string(_member->value.value));

  idl_delete_pstate(pstate);
}
