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
#include <stdlib.h>
#include <string.h>

#include "idl/processor.h"

#include "CUnit/Test.h"

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

typedef struct inherit_spec_test {
  const char *str;
  idl_retcode_t parse_ret;
  idl_extensibility_t base_ext;
  idl_extensibility_t inh_ext;
} inherit_spec_test_t;

static void
test_inheritance(inherit_spec_test_t test) {
  idl_pstate_t *pstate = NULL;
  idl_retcode_t ret = parse_string(IDL_FLAG_ANNOTATIONS, test.str, &pstate);
  CU_ASSERT_EQUAL(test.parse_ret, ret);
  if (test.parse_ret != IDL_RETCODE_OK) {
    CU_ASSERT_PTR_NULL(pstate);
  } else if (ret == IDL_RETCODE_OK) {
    CU_ASSERT_PTR_NOT_NULL_FATAL(pstate);

    idl_struct_t *base = (idl_struct_t*)pstate->root;
    idl_struct_t *derived = (idl_struct_t*)idl_next(base);

    CU_ASSERT_FATAL(idl_is_struct(base) && idl_is_struct(derived));

    CU_ASSERT_STRING_EQUAL(idl_identifier(base), "base");
    CU_ASSERT_EQUAL(base->extensibility.value, test.base_ext);

    CU_ASSERT_STRING_EQUAL(idl_identifier(derived), "derived");
    CU_ASSERT_EQUAL(derived->extensibility.value, test.inh_ext);

    CU_ASSERT(derived->inherit_spec && derived->inherit_spec->base == base);

    idl_delete_pstate(pstate);
  }
}

CU_Test(idl_inheritance, extensibility)
{
  static const inherit_spec_test_t tests[] = {
    {"struct base { char c; };\n"
     "struct derived : base { char d; };",                             IDL_RETCODE_OK,             IDL_FINAL, IDL_FINAL}, //base sanity check
    {"@extensibility(MUTABLE) struct base { char c; };\n"
     "struct derived : base { char d; };",                             IDL_RETCODE_OK,             IDL_MUTABLE, IDL_MUTABLE}, //implicit extensibility sanity check
    {"@extensibility(MUTABLE) struct base { char c; };\n"
     "@extensibility(MUTABLE) struct derived : base { char d; };",     IDL_RETCODE_OK,             IDL_MUTABLE, IDL_MUTABLE}, //explicit extensibility sanity check
    {"struct base { char c; };\n"
     "@extensibility(APPENDABLE) struct derived : base { char d; };",  IDL_RETCODE_SEMANTIC_ERROR, IDL_FINAL, IDL_FINAL}, //extensibility clash (implicit)
    {"@extensibility(FINAL) struct base { char c; };\n"
     "@extensibility(MUTABLE) struct derived : base { char d; };",     IDL_RETCODE_SEMANTIC_ERROR, IDL_FINAL, IDL_FINAL} //extensibility clash (explicit)
  };

  for (size_t i = 0; i < sizeof(tests)/sizeof(tests[0]); i++) {
    test_inheritance(tests[i]);
  }
}

CU_Test(idl_inheritance, members)
{
  static const inherit_spec_test_t tests[] = {
    {"struct base { @key char c; char d; };\n"
     "struct derived : base { char e, f; };",       IDL_RETCODE_OK,             IDL_FINAL, IDL_FINAL}, //base sanity check
    {"struct base { char c, d; };\n"
     "struct derived : base { string d, e; };",     IDL_RETCODE_SEMANTIC_ERROR, IDL_FINAL, IDL_FINAL}, //member name clash
    {"struct base { @key char c, d; };\n"
     "struct derived : base { @key char e, f; };",  IDL_RETCODE_SEMANTIC_ERROR, IDL_FINAL, IDL_FINAL} //extending key values is not allowed
  };

  for (size_t i = 0; i < sizeof(tests)/sizeof(tests[0]); i++) {
    test_inheritance(tests[i]);
  }
}

CU_Test(idl_inheritance, empty_structs)
{
  static struct {
    const char *str;
    uint32_t ids;
  } tests[] = {
    { "struct s1 { }; struct s2 : s1 { char c1; }; struct sx : s2 { char c2; };", 2 },
    { "struct s1 { char c1; }; struct s2 : s1 { }; struct s3 : s2 { }; struct sx : s3 { char c2; };", 2 },
    { "struct s1 { }; struct s2 : s1 { char c1; }; struct s3 : s2 { }; struct sx : s3 { char c2; };", 2 },
    { "struct s1 { char c1; }; struct s2 : s1 { char c2; }; struct sx : s2 { };", 2 },
    { "struct sx { };", 0 },
    { "struct s1 { }; struct s2 : s1 { }; struct sx : s2 { };", 0 }
  };

  for (size_t i=0, n=sizeof(tests)/sizeof(tests[0]); i < n; i++) {
    uint32_t fields = 0, ids = tests[i].ids;
    idl_retcode_t ret;
    idl_pstate_t *pstate = NULL;
    ret = idl_create_pstate(0, NULL, &pstate);
    CU_ASSERT_EQUAL(ret, IDL_RETCODE_OK);
    CU_ASSERT_PTR_NOT_NULL_FATAL(pstate);
    assert(pstate);
    ret = idl_parse_string(pstate, tests[i].str);
    CU_ASSERT_EQUAL(ret, IDL_RETCODE_OK);
    CU_ASSERT_PTR_NOT_NULL_FATAL(pstate->root);
    assert(pstate->root);
    const idl_struct_t *s = (idl_struct_t *)pstate->root;
    // search for struct by name
    while (s && !(idl_is_struct(s) && strcmp(idl_identifier(s), "sx") == 0))
      s = idl_next(s);
    // recurse down
    CU_ASSERT_PTR_NOT_NULL(s);
    while (s) {
      for (const idl_member_t *m = s->members; m; m = idl_next(m)) {
        for (const idl_declarator_t *d = m->declarators; d; d = idl_next(d)) {
          if (ids) {
            ids--;
            CU_ASSERT_EQUAL(d->id.value, ids);
          }
          fields++;
        }
      }
      s = s->inherit_spec ? s->inherit_spec->base : NULL;
    }
    CU_ASSERT_EQUAL(ids, 0);
    CU_ASSERT_EQUAL(fields, tests[i].ids);
    idl_delete_pstate(pstate);
  }
}
