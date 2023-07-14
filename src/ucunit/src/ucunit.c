// Copyright(c) 2023 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include <stdint.h>
#include <inttypes.h>

#include "ucunit/ucunit.h"

static const char *output_filename_root;
static CU_ErrorCode last_error;
static struct CU_Suite *suites;

static struct CU_Suite *cur_suite;
static struct CU_Test *cur_test;
static jmp_buf fatal_jmpbuf;
static uint32_t failure_count;

void CU_assertImplementation (bool value, int line, const char *expr, const char *file, const char *something, bool isfatal)
{
  (void)something;
  assert (cur_suite && cur_test);
  cur_test->nasserts++;
  if (value)
    return;

  fprintf (stderr, "suite %s test %s: assertion failure: %s:%d: %s\n", cur_suite->name, cur_test->name, file, line, expr);
  failure_count++;
  cur_test->nfailures++;
  struct CU_FailureRecord *fr = malloc (sizeof (*fr));
  fr->file = file;
  fr->line = line;
  fr->expr = expr;
  fr->next = NULL;
  if (cur_test->latest_failure)
    cur_test->latest_failure->next = fr;
  else
    cur_test->failures = fr;
  cur_test->latest_failure = fr;

  if (isfatal)
    longjmp (fatal_jmpbuf, 1);
}

CU_ErrorCode CU_initialize_registry (void)
{
  output_filename_root = NULL;
  failure_count = 0;
  last_error = CUE_SUCCESS;
  suites = NULL;
  cur_suite = NULL;
  cur_test = NULL;
  return CUE_SUCCESS;
}

const char *CU_get_error_msg (void)
{
  switch (last_error)
  {
    case CUE_SUCCESS: return "success";
    case CUE_ERROR: return "error";
  }
  return "?";
}

static char *ucunit_strdup (const char *s)
{
  size_t l = strlen (s) + 1;
  char *n = malloc (l);
  memcpy (n, s, l);
  return n;
}

CU_pSuite CU_add_suite (const char *strName, CU_InitializeFunc pInit, CU_CleanupFunc pClean)
{
  struct CU_Suite *cur, *last;
  for (cur = suites, last = NULL; cur && strcmp (cur->name, strName) != 0; last = cur, cur = cur->next)
    ;
  if (cur != NULL)
    return cur;
  cur = malloc (sizeof (*cur));
  cur->name = ucunit_strdup (strName);
  cur->init = pInit;
  cur->cleanup = pClean;
  cur->next = NULL;
  cur->active = true;
  cur->tests = NULL;
  cur->ntests = 0;
  cur->ntests_pass = 0;
  cur->ntests_failed = 0;
  cur->initfailed = false;
  if (last)
    last->next = cur;
  else
    suites = cur;
  return cur;
}

CU_pSuite CU_get_suite (const char *strName)
{
  struct CU_Suite *cur;
  for (cur = suites; cur; cur = cur->next)
    if (strcmp (cur->name, strName) == 0)
      return cur;
  return NULL;
}

CU_ErrorCode CU_set_suite_active (CU_pSuite pSuite, bool fNewActive)
{
  pSuite->active = fNewActive;
  return CUE_SUCCESS;
}

CU_pTest CU_add_test (CU_pSuite pSuite, const char *strName, CU_TestFunc pTestFunc)
{
  struct CU_Test *cur, *last;
  for (cur = pSuite->tests, last = NULL; cur && strcmp (cur->name, strName) != 0; last = cur, cur = cur->next)
    ;
  if (cur != NULL)
    return cur;
  cur = malloc (sizeof (*cur));
  cur->name = ucunit_strdup (strName);
  cur->testfunc = pTestFunc;
  cur->next = NULL;
  cur->active = true;
  cur->nasserts = 0;
  cur->nfailures = 0;
  cur->failures = NULL;
  cur->latest_failure = NULL;
  if (last)
    last->next = cur;
  else
    pSuite->tests = cur;
  pSuite->ntests++;
  return cur;
}

void CU_set_test_active (CU_pTest pTest, bool fNewActive)
{
  pTest->active = fNewActive;
}

CU_ErrorCode CU_get_error (void)
{
  return last_error;
}

static bool runtest (struct CU_Test *test)
{
  volatile int v = setjmp (fatal_jmpbuf);
  if (v == 0)
    test->testfunc ();
  return (v == 0 && test->nfailures == 0 && test->nasserts > 0);
}

CU_ErrorCode CU_basic_run_tests (void)
{
  uint32_t ntests_defined = 0, ntests_failed = 0, ntests_pass = 0;
  uint32_t nasserts_encountered = 0, nasserts_failed = 0;
  uint32_t nsuites_failed = 0;

  for (struct CU_Suite *suite = suites; suite; suite = suite->next)
  {
    ntests_defined += suite->ntests;
    if (!suite->active)
      continue;
    cur_suite = suite;
    if (suite->init)
    {
      if (suite->init () != 0)
      {
        fprintf (stderr, "failed to initialize suite %s\n", suite->name);
        suite->initfailed = true;
        nsuites_failed++;
        continue;
      }
    }
    for (struct CU_Test *test = suite->tests; test; test = test->next)
    {
      if (!test->active)
        continue;
      cur_test = test;
      if (runtest (test))
        suite->ntests_pass++;
      else
      {
        suite->ntests_failed++;
        if (test->nasserts == 0)
        {
          fprintf (stderr, "no asserts in suite %s test %s, considering it a failed test\n", suite->name, test->name);
          failure_count++;
        }
        nasserts_failed += test->nfailures;
      }
      nasserts_encountered += test->nasserts;
    }
    if (suite->cleanup)
      (void) suite->cleanup ();

    ntests_failed += suite->ntests_failed;
    ntests_pass += suite->ntests_pass;
  }

  fprintf (stderr, "tests: pass %"PRIu32" fail %"PRIu32" (total defined %"PRIu32"); asserts: pass %"PRIu32" fail %"PRIu32"\n",
           ntests_pass, ntests_failed, ntests_defined, nasserts_encountered - nasserts_failed, nasserts_failed);
  if (ntests_failed)
  {
    fprintf (stderr, "failed tests\n");
    for (const struct CU_Suite *suite = suites; suite; suite = suite->next)
    {
      for (const struct CU_Test *test = suite->tests; test; test = test->next)
      {
        if (test->nfailures)
        {
          fprintf (stderr, "- %s %s\n", suite->name, test->name);
          for (const struct CU_FailureRecord *fr = test->failures; fr; fr = fr->next)
            fprintf (stderr, "  assertion failure: %s:%d: %s\n", fr->file, fr->line, fr->expr);
        }
      }
    }
  }
  if (nsuites_failed)
  {
    fprintf (stderr, "suite initialization failures: %"PRIu32":\n", nsuites_failed);
    for (const struct CU_Suite *suite = suites; suite; suite = suite->next)
      if (suite->initfailed)
        fprintf (stderr, "- %s\n", suite->name);
    failure_count++;
  }
  // no tests at all: failure
  if (ntests_pass + ntests_failed == 0)
    failure_count++;
  return CUE_SUCCESS;
}

uint32_t CU_get_number_of_failures (void)
{
  return failure_count;
}

void CU_cleanup_registry (void)
{
  struct CU_Suite *suite;
  while ((suite = suites) != NULL)
  {
    struct CU_Test *test;
    suites = suite->next;
    while ((test = suite->tests) != NULL)
    {
      suite->tests = test->next;
      struct CU_FailureRecord *fr;
      while ((fr = test->failures) != NULL)
      {
        test->failures = fr->next;
        free (fr);
      }
      free (test->name);
      free (test);
    }
    free (suite->name);
    free (suite);
  }
}
