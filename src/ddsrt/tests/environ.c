// Copyright(c) 2006 to 2020 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <stdlib.h>
#include <assert.h>

#include "CUnit/Theory.h"
#include "dds/ddsrt/environ.h"
#include "dds/ddsrt/misc.h"
#include "dds/ddsrt/heap.h"

CU_TheoryDataPoints(ddsrt_environ, bad_name) = {
  CU_DataPoints(const char *, "", "foo=")
};

CU_Theory((const char *name), ddsrt_environ, bad_name)
{
  dds_return_t rc;
  static const char value[] = "bar";
  static char dummy[] = "foobar";
  const char *ptr;

  rc = ddsrt_setenv(name, value);
  CU_ASSERT_EQUAL(rc, DDS_RETCODE_BAD_PARAMETER);
  rc = ddsrt_unsetenv(name);
  CU_ASSERT_EQUAL(rc, DDS_RETCODE_BAD_PARAMETER);
  ptr = dummy;
  rc = ddsrt_getenv(name, &ptr);
  CU_ASSERT_EQUAL(rc, DDS_RETCODE_BAD_PARAMETER);
  CU_ASSERT_PTR_EQUAL(ptr, dummy);
}

DDSRT_WARNING_MSVC_OFF(4996)
CU_Test(ddsrt_environ, setenv)
{
  dds_return_t rc;
  static const char name[] = "foo";
  static char value[] = "bar";
  char *ptr;

  rc = ddsrt_setenv(name, value);
  CU_ASSERT_EQUAL(rc, DDS_RETCODE_OK);
  ptr = getenv(name);
  CU_ASSERT_PTR_NOT_NULL(ptr);
  assert (ptr != NULL); /* for the benefit of clang's static analyzer */
  CU_ASSERT_STRING_EQUAL(ptr, "bar");
  /* Ensure value is copied into the environment. */
  value[2] = 'z';
  ptr = getenv(name);
  CU_ASSERT_PTR_NOT_NULL(ptr);
  assert (ptr != NULL); /* for the benefit of clang's static analyzer */
  CU_ASSERT_STRING_EQUAL(ptr, "bar");
  rc = ddsrt_setenv(name, "");
  CU_ASSERT_EQUAL(rc, DDS_RETCODE_OK);
  ptr = getenv(name);
  CU_ASSERT_PTR_NULL(ptr);
}
DDSRT_WARNING_MSVC_ON(4996)

CU_Test(ddsrt_environ, getenv)
{
  dds_return_t rc;
  static const char name[] = "foo";
  static const char value[] = "bar";
  static char dummy[] = "foobar";
  const char *ptr;

  /* Ensure "not found" is returned. */
  rc = ddsrt_unsetenv(name);
  CU_ASSERT_EQUAL_FATAL(rc, DDS_RETCODE_OK);

  ptr = dummy;
  rc = ddsrt_getenv(name, &ptr);
  CU_ASSERT_EQUAL(rc, DDS_RETCODE_NOT_FOUND);
  CU_ASSERT_PTR_EQUAL(ptr, dummy);

  /* Ensure "ok" is returned and value is what it should be. */
  rc = ddsrt_setenv(name, value);
  CU_ASSERT_EQUAL_FATAL(rc, DDS_RETCODE_OK);

  ptr = dummy;
  rc = ddsrt_getenv(name, &ptr);
  CU_ASSERT_EQUAL(rc, DDS_RETCODE_OK);
  CU_ASSERT_PTR_NOT_EQUAL(ptr, dummy);
  CU_ASSERT_PTR_NOT_EQUAL(ptr, NULL);
  if (ptr != NULL) {
    CU_ASSERT_STRING_EQUAL(ptr, "bar");
  }

  /* Ensure environment is as it was. */
  rc = ddsrt_unsetenv(name);
  CU_ASSERT_EQUAL(rc, DDS_RETCODE_OK);
}

CU_TheoryDataPoints(ddsrt_environ, expand) = {
  CU_DataPoints(const char *,
         "${X}",        "$X",          "X",       "${Y}",     "${Q}",      "${X",
    "${X:-ALT}", "${Q:-ALT}", "${X:-${Y}}", "${Q:-${Y}}", "${X:-$Y}", "${Q:-$Y}", "${X:-}", "${Q:-}",
    "${X:+SET}", "${Q:+SET}", "${X:+${Y}}", "${Q:+${Y}}", "${X:+$Y}", "${Q:+$Y}", "${X:+}", "${Q:+}",
    "${X:?SET}", "${Q:?SET}", "${X:?${Y}}", "${Q:?${Y}}", "${X:?$Y}", "${Q:?$Y}", "${X:?}", "${Q:?}"),
  CU_DataPoints(const char *,
         "TEST",        "$X",          "X",        "FOO",         "",       NULL,
         "TEST",       "ALT",       "TEST",        "FOO",     "TEST",       "$Y",   "TEST",       "",
          "SET",          "",        "FOO",           "",       "$Y",         "",       "",       "",
         "TEST",        NULL,       "TEST",         NULL,     "TEST",       NULL,   "TEST",     NULL)
};
CU_Theory((const char *var, const char *expect), ddsrt_environ, expand)
{
  dds_return_t rc;
  static const char x_name[]  = "X";
  static const char x_value[] = "TEST";
  static const char y_name[]  = "Y";
  static const char y_value[] = "FOO";
  char *ptr;

  /* Ensure that the vars are not used yet. */
  rc = ddsrt_unsetenv(x_name);
  CU_ASSERT_EQUAL_FATAL(rc, DDS_RETCODE_OK);
  rc = ddsrt_unsetenv(y_name);
  CU_ASSERT_EQUAL_FATAL(rc, DDS_RETCODE_OK);

  /* Set the env vars to check expansion. */
  rc = ddsrt_setenv(x_name, x_value);
  CU_ASSERT_EQUAL_FATAL(rc, DDS_RETCODE_OK);
  rc = ddsrt_setenv(y_name, y_value);
  CU_ASSERT_EQUAL_FATAL(rc, DDS_RETCODE_OK);

  /* Expand a string with available environment variables. */
  ptr = ddsrt_expand_envvars(var,UINT32_MAX);
  if (ptr) {
    /* printf("==== %10s: expand(%s), expect(%s))\n", var, ptr, expect); */
    CU_ASSERT_STRING_EQUAL(ptr, expect);
    ddsrt_free(ptr);
  } else {
    /* printf("==== %10s: expand(<null>), expect(<null>))\n", var ? var : "<null>"); */
    CU_ASSERT_PTR_NULL(expect);
  }

  /* Ensure to reset the environment is as it was. */
  rc = ddsrt_unsetenv(y_name);
  CU_ASSERT_EQUAL(rc, DDS_RETCODE_OK);
  rc = ddsrt_unsetenv(x_name);
  CU_ASSERT_EQUAL(rc, DDS_RETCODE_OK);
}


CU_TheoryDataPoints(ddsrt_environ, expand_sh) = {
  CU_DataPoints(const char *,
         "${X}",        "$X",          "X",       "${Y}",     "${Q}",      "${X",
    "${X:-ALT}", "${Q:-ALT}", "${X:-${Y}}", "${Q:-${Y}}", "${X:-$Y}", "${Q:-$Y}", "${X:-}", "${Q:-}",
    "${X:+SET}", "${Q:+SET}", "${X:+${Y}}", "${Q:+${Y}}", "${X:+$Y}", "${Q:+$Y}", "${X:+}", "${Q:+}",
    "${X:?SET}", "${Q:?SET}", "${X:?${Y}}", "${Q:?${Y}}", "${X:?$Y}", "${Q:?$Y}", "${X:?}", "${Q:?}"),
  CU_DataPoints(const char *,
         "TEST",      "TEST",          "X",        "FOO",         "",       NULL,
         "TEST",       "ALT",       "TEST",        "FOO",     "TEST",      "FOO",   "TEST",       "",
          "SET",          "",        "FOO",           "",      "FOO",         "",       "",       "",
         "TEST",        NULL,       "TEST",         NULL,     "TEST",       NULL,   "TEST",     NULL)
};
CU_Theory((const char *var, const char *expect), ddsrt_environ, expand_sh)
{
  dds_return_t rc;
  static const char x_name[]  = "X";
  static const char x_value[] = "TEST";
  static const char y_name[]  = "Y";
  static const char y_value[] = "FOO";
  char *ptr;

  /* Ensure that the vars are not used yet. */
  rc = ddsrt_unsetenv(x_name);
  CU_ASSERT_EQUAL_FATAL(rc, DDS_RETCODE_OK);
  rc = ddsrt_unsetenv(y_name);
  CU_ASSERT_EQUAL_FATAL(rc, DDS_RETCODE_OK);

  /* Set the env vars to check expansion. */
  rc = ddsrt_setenv(x_name, x_value);
  CU_ASSERT_EQUAL_FATAL(rc, DDS_RETCODE_OK);
  rc = ddsrt_setenv(y_name, y_value);
  CU_ASSERT_EQUAL_FATAL(rc, DDS_RETCODE_OK);

  /* Expand a string with available environment variables. */
  ptr = ddsrt_expand_envvars_sh(var,UINT32_MAX);
  if (ptr) {
    /* printf("==== %10s: expand(%s), expect(%s))\n", var, ptr, expect); */
    CU_ASSERT_STRING_EQUAL(ptr, expect);
    ddsrt_free(ptr);
  } else {
    /* printf("==== %10s: expand(<null>), expect(<null>))\n", var ? var : "<null>"); */
    CU_ASSERT_PTR_NULL(expect);
  }

  /* Ensure to reset the environment is as it was. */
  rc = ddsrt_unsetenv(y_name);
  CU_ASSERT_EQUAL(rc, DDS_RETCODE_OK);
  rc = ddsrt_unsetenv(x_name);
  CU_ASSERT_EQUAL(rc, DDS_RETCODE_OK);
}
