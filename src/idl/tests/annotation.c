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
  ret = idl_parse_string(pstate, str);
  if (ret != IDL_RETCODE_OK)
    idl_delete_pstate(pstate);
  else
    *pstatep = pstate;
  return ret;
}

CU_Test(idl_annotation, id_member)
{
  idl_retcode_t ret;
  idl_pstate_t *pstate = NULL;
  idl_struct_t *s;
  idl_member_t *c;
  const char str[] = "struct s { @id(1) @optional char c; };";

  ret = parse_string(IDL_FLAG_ANNOTATIONS, str, &pstate);
  CU_ASSERT_EQUAL_FATAL(ret, IDL_RETCODE_OK);
  CU_ASSERT_PTR_NOT_NULL_FATAL(pstate);
  assert(pstate);
  s = (idl_struct_t *)pstate->root;
  CU_ASSERT_PTR_NOT_NULL_FATAL(s);
  CU_ASSERT_FATAL(idl_is_struct(s));
  assert(s);
  c = (idl_member_t *)s->members;
  CU_ASSERT_PTR_NOT_NULL(c);
  assert(c);
  CU_ASSERT_FATAL(idl_is_member(c));
  CU_ASSERT_EQUAL(c->id.annotation, IDL_ID);
  CU_ASSERT_EQUAL(c->id.value, 1);
  idl_delete_pstate(pstate);
}

CU_Test(idl_annotation, key)
{
  idl_retcode_t ret;
  idl_pstate_t *pstate = NULL;
  idl_struct_t *s;
  idl_member_t *m;
  const char str[] = "struct s {\n"
                     "  @key char a;\n"
                     "  @key(TRUE) char b;\n"
                     "  @key(FALSE) char c;\n"
                     "  char d;\n"
                     "};";

  ret = parse_string(IDL_FLAG_ANNOTATIONS, str, &pstate);
  CU_ASSERT_EQUAL_FATAL(ret, IDL_RETCODE_OK);
  CU_ASSERT_PTR_NOT_NULL_FATAL(pstate);
  assert(pstate);
  s = (idl_struct_t *)pstate->root;
  CU_ASSERT_FATAL(idl_is_struct(s));
  assert(s);
  m = (idl_member_t *)s->members;
  CU_ASSERT_FATAL(idl_is_member(m));
  CU_ASSERT_EQUAL(m->key, IDL_TRUE);
  assert(m);
  m = idl_next(m);
  CU_ASSERT_FATAL(idl_is_member(m));
  CU_ASSERT_EQUAL(m->key, IDL_TRUE);
  m = idl_next(m);
  CU_ASSERT_FATAL(idl_is_member(m));
  CU_ASSERT_EQUAL(m->key, IDL_FALSE);
  m = idl_next(m);
  CU_ASSERT_FATAL(idl_is_member(m));
  CU_ASSERT_EQUAL(m->key, IDL_DEFAULT);
  idl_delete_pstate(pstate);
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
  CU_ASSERT_EQUAL(s->nested.annotation, IDL_DEFAULT_NESTED);
  CU_ASSERT(s->nested.value == false);
  s = idl_next(s);
  CU_ASSERT_FATAL(idl_is_struct(s));
  CU_ASSERT_EQUAL(s->nested.annotation, IDL_NESTED);
  CU_ASSERT(s->nested.value == true);
  s = idl_next(s);
  CU_ASSERT_FATAL(idl_is_struct(s));
  CU_ASSERT_EQUAL(s->nested.annotation, IDL_NESTED);
  CU_ASSERT(s->nested.value == true);
  s = idl_next(s);
  CU_ASSERT_FATAL(idl_is_struct(s));
  CU_ASSERT_EQUAL(s->nested.annotation, IDL_NESTED);
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
    idl_nested_t n;
  } tests[] = {
    {                   S("s1"), {0,0} },
    {               N() S("s1"), {1,1} },
    { T()               S("s1"), {2,0} },
    { T()           N() S("s1"), {2,0} },
    { T(P("!DDS"))      S("s1"), {0,0} },
    { T(P("DDS"))       S("s1"), {2,0} },
    { T(P("!DDS"))  N() S("s1"), {1,1} }
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
      s = (idl_struct_t *)pstate->root;
      CU_ASSERT_FATAL(idl_is_struct(s));
      CU_ASSERT(memcmp(&s->nested, &tests[i].n, sizeof(s->nested)) == 0);
    }
    idl_delete_pstate(pstate);
  }
}

CU_Test(idl_annotation, default_nested)
{
  static const struct {
    const char *str;
    idl_boolean_t dn[3];
    idl_nested_t n[2];
  } tests[] = {
    {         M("m1",         M("m2",        S("s1")) M("m3", S("s2"))), {0,0,0}, {{0,0},{0,0}} },
    { DN()    M("m1",         M("m2",        S("s1")) M("m3", S("s2"))), {2,0,0}, {{0,1},{0,1}} },
    { DN()    M("m1", DN(NO)  M("m2",        S("s1")) M("m3", S("s2"))), {2,1,0}, {{0,0},{0,1}} },
    { DN(NO)  M("m1", DN(YES) M("m2",        S("s1")) M("m3", S("s2"))), {1,2,0}, {{0,1},{0,0}} },
    { DN(YES) M("m1",         M("m2", N(NO)  S("s1")) M("m3", S("s2"))), {2,0,0}, {{1,0},{0,1}} }
  };

  static const size_t n = sizeof(tests)/sizeof(tests[0]);

  idl_retcode_t ret;
  idl_pstate_t *pstate;
  idl_module_t *m;
  idl_struct_t *s;

  for (size_t i=0; i < n; i++) {
    pstate = NULL;
    ret = parse_string(IDL_FLAG_ANNOTATIONS, tests[i].str, &pstate);
    CU_ASSERT_EQUAL(ret, IDL_RETCODE_OK);
    if (ret == IDL_RETCODE_OK) {
      m = (idl_module_t *)pstate->root;
      CU_ASSERT_FATAL(idl_is_module(m));
      CU_ASSERT_EQUAL(m->default_nested, tests[i].dn[0]);
      m = m->definitions;
      CU_ASSERT_FATAL(idl_is_module(m));
      CU_ASSERT_EQUAL(m->default_nested, tests[i].dn[1]);
      s = m->definitions;
      CU_ASSERT_FATAL(idl_is_struct(s));
      CU_ASSERT(memcmp(&s->nested, &tests[i].n[0], sizeof(s->nested)) == 0);
      m = idl_next(m);
      CU_ASSERT_FATAL(idl_is_module(m));
      CU_ASSERT_EQUAL(m->default_nested, tests[i].dn[2]);
      s = m->definitions;
      CU_ASSERT_FATAL(idl_is_struct(s));
      CU_ASSERT(memcmp(&s->nested, &tests[i].n[1], sizeof(s->nested)) == 0);
    }
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

// x. do not allow annotation_appl in annotation

#if 0
CU _ Test(idl_annotation, id_non_member)
{
  idl_retcode_t ret;
  idl_tree_t *tree = NULL;
  const char str[] = "@id(1) struct s { char c; };";

  ret = idl_parse_string(str, IDL_FLAG_ANNOTATIONS, &tree);
  CU_ASSERT_EQUAL(ret, IDL_RETCODE_SEMANTIC_ERROR);
  idl_delete_tree(tree);
}

CU _ Test(idl_annotation, hashid_member)
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

CU _ Test(idl_annotation, autoid_struct)
{
  idl_retcode_t ret;
  idl_tree_t *tree = NULL;
  idl_struct_t *s1;
  const char str[] = "@autoid struct s { char c; };";

  ret = idl_parse_string(str, IDL_FLAG_ANNOTATIONS, &tree);
  CU_ASSERT_EQUAL_FATAL(ret, IDL_RETCODE_OK);
  CU_ASSERT_PTR_NOT_NULL(tree);
  s1 = (idl_struct_t *)tree->root;
  CU_ASSERT_PTR_NOT_NULL_FATAL(s1);
  CU_ASSERT_FATAL(idl_is_struct(s1));
  CU_ASSERT_EQUAL(s1->autoid, IDL_AUTOID_SEQUENTIAL);
  idl_delete_tree(tree);
}

// x. autoid twice
// x. autoid (HASH)
// x. autoid (SEQUENTIAL)

#endif

#define A(ann) ann " struct s { char c; };"
CU_Test(idl_annotation, struct_extensibility)
{
  static const struct {
    const char *str;
    enum idl_extensibility ext;
  } tests[] = {
    { A("@final"), IDL_EXTENSIBILITY_FINAL },
    { A("@appendable"), IDL_EXTENSIBILITY_APPENDABLE },
    { A("@mutable"), IDL_EXTENSIBILITY_MUTABLE},
    { A("@extensibility(FINAL)"), IDL_EXTENSIBILITY_FINAL },
    { A("@extensibility(APPENDABLE)"), IDL_EXTENSIBILITY_APPENDABLE },
    { A("@extensibility(MUTABLE)"), IDL_EXTENSIBILITY_MUTABLE},
  };
  static const size_t n = sizeof(tests)/sizeof(tests[0]);

  idl_retcode_t ret;
  idl_pstate_t *pstate = NULL;
  idl_struct_t *s;

  for (size_t i = 0; i < n; i++) {
    pstate = NULL;
    ret = parse_string(IDL_FLAG_ANNOTATIONS, tests[i].str, &pstate);
    CU_ASSERT_EQUAL_FATAL(ret, IDL_RETCODE_OK);
    CU_ASSERT_PTR_NOT_NULL_FATAL(pstate);
    assert(pstate);
    s = (idl_struct_t *)pstate->root;
    CU_ASSERT_PTR_NOT_NULL_FATAL(s);
    assert(s);
    CU_ASSERT_FATAL(idl_is_struct(s));
    CU_ASSERT_EQUAL(s->extensibility, tests[i].ext);
    idl_delete_pstate(pstate);
  }
}
#undef A

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
    /* invalid */
    { BM("0"), false, 0 },
    { BM("65"), false, 0 },
    { E("0"), false, 0 },
    { E("33"), false, 0 },
    { "@bit_bound(1) bitmask MyBitMask { flag0, flag1 };", false, 0 },
    { "@bit_bound(1) enum MyEnum { ENUM1, ENUM2 };", false, 0 },
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
      CU_ASSERT_EQUAL_FATAL(b->bit_bound, tests[i].value);
    } else if (idl_is_enum(pstate->root)) {
      idl_enum_t *e = (idl_enum_t *)pstate->root;
      CU_ASSERT_EQUAL_FATAL(e->bit_bound, tests[i].value);
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
      CU_ASSERT_EQUAL(bv->position, tests[i].p[j]);
    }
    idl_delete_pstate(pstate);
  }
}

#undef BM
