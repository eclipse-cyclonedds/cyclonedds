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
#include <stdlib.h>
#include <string.h>

#include "idl/string.h"
#include "file.h"

#include "CUnit/Theory.h"

#define ok (IDL_RETCODE_OK)
#define bad_param (IDL_RETCODE_BAD_PARAMETER)

#if _WIN32
# define ROOT "C:\\"
# define SEP "\\"
#else
# define ROOT "/"
# define SEP "/"
#endif

static char *prefix = NULL;

CU_Init(idl_file)
{
  idl_current_path(&prefix);
  return 0;
}

CU_Clean(idl_file)
{
  if (prefix)
    free(prefix);
  return 0;
}

struct scrub_test {
  const char *input;
  const char *output;
  const int8_t length;
};

const struct scrub_test tests[] = {
  { ROOT "..",                        "",                     -1 },
  { ROOT "/..",                       "",                     -1 },
  { ROOT "//..",                      "",                     -1 },
  { ROOT "a/../..",                   "",                     -1 },
  { ROOT "/a//..//..",                "",                     -1 },
  { ROOT "//a///..///..",             "",                     -1 },
  { "",                               "",                      0 },
  { ".",                              "",                      0 },
  { "./",                             "",                      0 },
  { ".//",                            "",                      0 },
  { ".///",                           "",                      0 },
  { "././",                           "",                      0 },
  { ".//.//",                         "",                      0 },
  { ".///.///",                       "",                      0 },
  { "..",                             "..",                    2 },
  { "../",                            "..",                    2 },
  { "..//",                           "..",                    2 },
  { "..///",                          "..",                    2 },
  { "./..",                           "..",                    2 },
  { ".//..",                          "..",                    2 },
  { ".///..",                         "..",                    2 },
  { ".///..///",                      "..",                    2 },
  { "../..",                          ".." SEP "..",           5 },
  { "..//..",                         ".." SEP "..",           5 },
  { "..///..",                        ".." SEP "..",           5 },
  { "../../../",                      ".." SEP ".." SEP "..",  8 },
  { ROOT ,                            ROOT,                    1 },
  { ROOT "/",                         ROOT,                    1 },
  { ROOT "//",                        ROOT,                    1 },
  { ROOT ".",                         ROOT,                    1 },
  { ROOT "/.",                        ROOT,                    1 },
  { ROOT "//.",                       ROOT,                    1 },
  { ROOT "./",                        ROOT,                    1 },
  { ROOT "/.//",                      ROOT,                    1 },
  { ROOT "//.///",                    ROOT,                    1 },
  { "a/..",                           "",                      0 },
  { "a//..",                          "",                      0 },
  { "a///..",                         "",                      0 },
  { "a/b/../..",                      "",                      0 },
  { "a//b//..//..",                   "",                      0 },
  { "a///b///..///..",                "",                      0 },
  { "a/../b/..",                      "",                      0 },
  { "a//..//b//..",                   "",                      0 },
  { "a///..///b///..",                "",                      0 },
  { "a/./.././b",                     "b",                     1 },
  { "a//.//..//.//b",                 "b",                     1 },
  { "a///.///..///.///b",             "b",                     1 },
  { "ab/./.././cd",                   "cd",                    2 },
  { "ab//.//..//.//cd",               "cd",                    2 },
  { "ab///.///..///.///cd",           "cd",                    2 },
  { "abc/./.././def",                 "def",                   3 },
  { "abc//.//..//.//def",             "def",                   3 },
  { "abc///.///..///.///def",         "def",                   3 },
  { ROOT "a/..",                      ROOT,                    1 },
  { ROOT "/a//..",                    ROOT,                    1 },
  { ROOT "//a///..",                  ROOT,                    1 },
  { ROOT "a/b/../..",                 ROOT,                    1 },
  { ROOT "/a//b//..//..",             ROOT,                    1 },
  { ROOT "//a///b///..///..",         ROOT,                    1 },
  { ROOT "a/../b/..",                 ROOT,                    1 },
  { ROOT "/a//..//b//..",             ROOT,                    1 },
  { ROOT "//a///..///b///..",         ROOT,                    1 },
  { ROOT "ab/./.././cd",              ROOT "cd",               3 },
  { ROOT "/ab//.//..//.//cd",         ROOT "cd",               3 },
  { ROOT "//ab///.///..///.///cd",    ROOT "cd",               3 },
  { ROOT "abc/./.././def",            ROOT "def",              4 },
  { ROOT "/abc//.//..//.//def",       ROOT "def",              4 },
  { ROOT "//abc///.///..///.///def",  ROOT "def",              4 }
};

/* FIXME: add Windows-specific tests */

CU_Test(idl_file, untaint)
{
  const size_t n = sizeof(tests) / sizeof(tests[0]);

  for (size_t i=0; i < n; i++) {
    char *str;
    ssize_t len;

    str = idl_strdup(tests[i].input);
    CU_ASSERT_PTR_NOT_NULL_FATAL(str);
    assert(str);
    fprintf(stderr, "input: '%s'\n", str);
    len = idl_untaint_path(str);
    if (tests[i].length == -1) {
      CU_ASSERT_EQUAL(len, -1);
    } else {
      CU_ASSERT_EQUAL(len, (ssize_t)strlen(tests[i].output));
    }
    if (len >= 0) {
      fprintf(stderr, "output: '%s'\n", str);
      CU_ASSERT_STRING_EQUAL(str, tests[i].output);
    }
    free(str);
  }
}

CU_Test(idl_file, normalize_empty)
{
  idl_retcode_t ret;
  char *norm = NULL;

  ret = idl_normalize_path("", &norm);
  CU_ASSERT_FATAL(ret >= 0);
  CU_ASSERT_PTR_NOT_NULL_FATAL(norm);
  assert(prefix);
  fprintf(stderr, "path: %s\nexpect: %s\nnormalized: %s\n", prefix, prefix, norm);
  CU_ASSERT_STRING_EQUAL(norm, prefix);
  free(norm);
}

CU_Test(idl_file, normalize_revert)
{
  idl_retcode_t ret;
  char *norm = NULL, *path = NULL;

  (void) idl_asprintf(&path, "%s/..", prefix);
  CU_ASSERT_PTR_NOT_NULL_FATAL(path);
  assert(path);
  ret = idl_normalize_path(path, &norm);
  CU_ASSERT_FATAL(ret >= 0);
  CU_ASSERT_PTR_NOT_NULL_FATAL(norm);
  assert(norm);
  fprintf(stderr, "path: %s\n", path);
  { size_t sep = 0;
    for (size_t i=0,n=strlen(prefix); i < n; i++) {
      if (idl_isseparator(path[i]))
        sep = i;
    }
    CU_ASSERT_NOT_EQUAL_FATAL(sep, 0);
    path[sep] = '\0';
  }
  fprintf(stderr, "expect: %s\nnormalized: %s\n", path, norm);
  CU_ASSERT_STRING_EQUAL(path, norm);
  free(path);
  free(norm);
}

CU_Test(idl_file, normalize_revert_too_many)
{
  idl_retcode_t ret;
  size_t size, step, steps = 1; /* one too many */
  char *revert = NULL, *path = NULL, *norm = NULL;

  fprintf(stderr, "prefix: %s\n", prefix);

  for (size_t i=0; prefix[i]; i++)
    steps += idl_isseparator(prefix[i]);

  step = sizeof("/..") - 1;
  size = steps * step;
  revert = malloc(size + 1);
  CU_ASSERT_PTR_NOT_NULL_FATAL(revert);
  assert(revert);
  for (size_t i=0; i < steps; i++)
    memcpy(revert + (i*step), "/..", step);
  revert[size] = '\0';

  fprintf(stderr, "revert: %s\n", revert);

  path = NULL;
  (void) idl_asprintf(&path, "%s%s", prefix, revert);
  CU_ASSERT_PTR_NOT_NULL_FATAL(path);

  fprintf(stderr, "path: %s\n", path);

  norm = NULL;
  ret = idl_normalize_path(path, &norm);
  CU_ASSERT_EQUAL(ret, IDL_RETCODE_BAD_PARAMETER);
  CU_ASSERT_PTR_NULL(norm);
  free(revert);
  free(path);
  if (norm)
    free(norm);
}

struct relative_test {
  const char *base;
  const char *path;
  const char *relpath;
  idl_retcode_t retcode;
};

static struct relative_test relative_bad_params_tests[] = {
  /* test absolute, fully resolved, paths are required */
  {      "",            ROOT "foo",  NULL,  bad_param },
  {      ".",           ROOT "foo",  NULL,  bad_param },
  {      "./",          ROOT "foo",  NULL,  bad_param },
  { ROOT ".",           ROOT "foo",  NULL,  bad_param },
  { ROOT "./",          ROOT "foo",  NULL,  bad_param },
  {      "./foo/bar",   ROOT "foo",  NULL,  bad_param },
  { ROOT "foo/./bar",   ROOT "foo",  NULL,  bad_param },
  { ROOT "foo/bar/.",   ROOT "foo",  NULL,  bad_param },
  {      "..",          ROOT "foo",  NULL,  bad_param },
  {      "../",         ROOT "foo",  NULL,  bad_param },
  { ROOT "..",          ROOT "foo",  NULL,  bad_param },
  {      "../foo/bar",  ROOT "foo",  NULL,  bad_param },
  { ROOT "foo/../bar",  ROOT "foo",  NULL,  bad_param },
  { ROOT "foo/bar/..",  ROOT "foo",  NULL,  bad_param }
};

CU_Test(idl_file, relative_bad_params)
{
  idl_retcode_t ret;
  char *rel;

  struct relative_test *t = relative_bad_params_tests;
  size_t n = sizeof(relative_bad_params_tests)/sizeof(relative_bad_params_tests[0]);

  for (size_t i=0; i < n; i++) {
    const char *base, *path;
    for (size_t j=0; j < 2; j++) {
      rel = NULL;
      base = j ? t[i].path : t[i].base;
      path = j ? t[i].base : t[i].path;
      fprintf(stderr, "base: %s\n", base);
      fprintf(stderr, "path: %s\n", path);
      ret = idl_relative_path(base, path, &rel);
      fprintf(stderr, "relative: %s\n", rel ? rel : "-");
      if (rel)
        free(rel);
      CU_ASSERT_EQUAL_FATAL(ret, bad_param);
      // coverity[use_after_free:FALSE]
      CU_ASSERT_PTR_NULL(rel);
    }
  }
}

static struct relative_test relative_tests[] = {
  { ROOT "",              ROOT "",              "",                                              ok },
  { ROOT "f/o/o",         ROOT "",              ".." SEP ".." SEP "..",                          ok },
  { ROOT "",              ROOT "f/o/o",         "f" SEP "o" SEP "o",                             ok },
  { ROOT "f/o/o",         ROOT "b/a/r",         ".." SEP ".." SEP ".." SEP "b" SEP "a" SEP "r",  ok },
  { ROOT "f/o/o",         ROOT "f/o",           "..",                                            ok },
  { ROOT "f/oo",          ROOT "f/o",           ".." SEP "o",                                    ok },
  { ROOT "f/o",           ROOT "f/o",           "",                                              ok },
  { ROOT "f/o",           ROOT "f/oo",          ".." SEP "oo",                                   ok },
  { ROOT "f/o",           ROOT "f/o/o",         "o",                                             ok },
  { ROOT "foo/bar",       ROOT "foo/baz",       ".." SEP "baz",                                  ok },
  { ROOT "f//o///o////",  ROOT "f////o///o//",  "",                                              ok },
  { ROOT "f/o//o///",     ROOT "f///o//o/",     "",                                              ok },
  { ROOT "b/a//r///",     ROOT "b/a/r",         "",                                              ok },
  { ROOT "b//a//r",       ROOT "b/a/r",         "",                                              ok },
  { ROOT "b/a//r///",     ROOT "b/a/z",         ".." SEP "z",                                    ok }
};

CU_Test(idl_file, relative)
{
  idl_retcode_t ret;
  char *rel;

  struct relative_test *t = relative_tests;
  size_t n = sizeof(relative_tests)/sizeof(relative_tests[0]);

  for (size_t i=0; i < n; i++) {
    rel = NULL;
    fprintf(stderr, "base: '%s'\n", t[i].base);
    fprintf(stderr, "path: '%s'\n", t[i].path);
    ret = idl_relative_path(t[i].base, t[i].path, &rel);
    CU_ASSERT_EQUAL(ret, t[i].retcode);
    if (rel)
      fprintf(stderr, "relative: '%s'\n", rel);
    if (t[i].relpath) {
      CU_ASSERT_PTR_NOT_NULL_FATAL(rel);
      CU_ASSERT(rel && strcmp(t[i].relpath, rel) == 0);
    } else {
      CU_ASSERT_PTR_NULL(rel);
    }
    if (rel)
      free(rel);
  }
}


typedef struct out_file_test{
  const char *path;
  const char *output_dir;
  const char *base_dir;
  const char *out_ext;
  idl_retcode_t ret;
  const char *expected_out;
} out_file_test_t;

static void test_out_file(const out_file_test_t test)
{
  char *out = NULL;
  idl_retcode_t ret = idl_generate_out_file(test.path, test.output_dir, test.base_dir, test.out_ext, &out, true);

  CU_ASSERT_EQUAL(ret, test.ret);

  if(ret != IDL_RETCODE_BAD_PARAMETER){
    CU_ASSERT_STRING_EQUAL(out, test.expected_out);
  }

  if (out)
    free(out);
}

CU_Test(idl_file, out_file_generation)
{
  const out_file_test_t out_tests[] = {
      {"a/b.idl",            NULL,    NULL,         NULL, IDL_RETCODE_OK,            "a/b"},
      {"a/b.idl",            NULL,    NULL,         "c",  IDL_RETCODE_OK,            "a/b.c"},
      {"a/b.idl",            "c",     NULL,         NULL, IDL_RETCODE_OK,            "c/a/b"},
      {ROOT"a/b.idl",        NULL,    ROOT"a",      "c",  IDL_RETCODE_OK,            "b.c"},
      {ROOT"a/b.idl",        "f",     ROOT"a",      "c",  IDL_RETCODE_OK,            "f/b.c"},
      {ROOT"a/b.idl",        NULL,    NULL,         NULL, IDL_RETCODE_OK,            "b"},
      {ROOT"a/b.idl",        NULL,    NULL,         "c",  IDL_RETCODE_OK,            "b.c"},
      {"a/b.idl",            "c",     NULL,         NULL, IDL_RETCODE_OK,            "c/a/b"},
      {ROOT"a/b.idl",        "c",     NULL,         NULL, IDL_RETCODE_OK,            "c/b"},
      {"a/b.idl",            ROOT"c", NULL,         NULL, IDL_RETCODE_OK,            ROOT"c/a/b"},
      {"a/b/c.idl",          "..",    NULL,         NULL, IDL_RETCODE_OK,            "../a/b/c"},
      {"a/b/c.idl",          "../d",  NULL,         NULL, IDL_RETCODE_OK,            "../d/a/b/c"},
      {"a/b/c.idl",          ".",     NULL,         NULL, IDL_RETCODE_OK,            "./a/b/c"},
      {ROOT"a/b/c/file.idl", ".",     ROOT"a/b/",   NULL, IDL_RETCODE_OK,            "./c/file"},
      {ROOT"a/b/c/file.idl", "..",    ROOT"a/b/",   "h",  IDL_RETCODE_OK,            "../c/file.h"},
      {ROOT"a/b/c/file.idl", ".",     ROOT"a/b/",   NULL, IDL_RETCODE_OK,            "./c/file"},
      {ROOT"a/b/c/file.idl", NULL,    ROOT"a/b/",   NULL, IDL_RETCODE_OK,            "c/file"},
      {ROOT"a/b/c/file.idl", ".",     ROOT"a/b/d/", NULL, IDL_RETCODE_BAD_PARAMETER, "./c/file"},
      {ROOT"a/b/c/file.idl", NULL,    ROOT"a/b/d/", NULL, IDL_RETCODE_BAD_PARAMETER, "c/file"}
  };

  for (size_t i = 0; i < sizeof(out_tests)/sizeof(out_tests[0]); i++)
    test_out_file(out_tests[i]);
}
