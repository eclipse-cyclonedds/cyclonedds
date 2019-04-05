/*
 * Copyright(c) 2019 ADLINK Technology Limited and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "process_test.h"
#include "dds/ddsrt/strtol.h"
#include "dds/ddsrt/environ.h"
#include "dds/ddsrt/process.h"


static int test_create(void)
{
  printf(" Process: created without args.\n");
  return TEST_CREATE_EXIT;
}

static int test_sleep(int argi, int argc, char **argv)
{
  argi++;
  if (argi < argc) {
    long long dorment;
    ddsrt_strtoll(argv[argi], NULL, 0, &dorment);
    printf(" Process: sleep %d seconds.\n", (int)dorment);
    dds_sleepfor(DDS_SECS((int64_t)dorment));
  } else {
    printf(" Process: no --sleep value.\n");
    return TEST_EXIT_WRONG_ARGS;
  }
  /* Expected to be destroyed before reaching this. */
  return TEST_EXIT_FAILURE;
}

static int test_pid(void)
{
  int ret;
  ddsrt_pid_t pid;
  pid = ddsrt_getpid();
  ret = TEST_PID_EXIT(pid);
  printf(" Process: pid %d reduced to %d exit code.\n", (int)pid, ret);
  return ret;
}

static int test_env(void)
{
  int ret = TEST_EXIT_FAILURE;
  char *envptr = NULL;
  if (ddsrt_getenv(TEST_ENV_VAR_NAME, &envptr) == DDS_RETCODE_OK) {
    printf(" Process: env %s=%s.\n", TEST_ENV_VAR_NAME, envptr);
    if (strcmp(envptr, TEST_ENV_VAR_VALUE) == 0) {
      ret = TEST_ENV_EXIT;
    }
  } else {
    printf(" Process: failed to get environment variable.\n");
  }
  return ret;
}

static int test_bslash(void)
{
  printf(" Process: backslash argument.\n");
  return TEST_BSLASH_EXIT;
}

static int test_dquote(void)
{
  printf(" Process: double-quote argument.\n");
  return TEST_DQUOTE_EXIT;
}


/*
 * The spawned application used in the process tests.
 */
int main(int argc, char **argv)
{
  int ret;

  if (argc == 1) {
    ret = test_create();
  } else {
    if (strcmp(argv[1], TEST_SLEEP_ARG) == 0) {
      ret = test_sleep(1, argc, argv);
    } else if (strcmp(argv[1], TEST_PID_ARG) == 0) {
      ret = test_pid();
    } else if (strcmp(argv[1], TEST_ENV_ARG) == 0) {
      ret = test_env();
    } else if (strcmp(argv[1], TEST_BSLASH_ARG) == 0) {
      ret = test_bslash();
    } else if (strcmp(argv[1], TEST_DQUOTE_ARG) == 0) {
      ret = test_dquote();
    } else {
      printf(" Process: unknown argument.\n");
      ret = TEST_EXIT_WRONG_ARGS;
    }
  }

  return ret;
}
