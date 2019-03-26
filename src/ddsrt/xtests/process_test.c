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
#include "dds/ddsrt/time.h"
#include "dds/ddsrt/strtol.h"
#include "dds/ddsrt/environ.h"
#include "dds/ddsrt/process.h"


/*
 * Few helpful test functions/macros/magicvalues.
 */
int test_assert_(bool cond, const char *err, const char *func, int line)
{
  int result = 0;
  assert(err);
  if (!cond) {
    printf("  [%s:%d] %s\n", func, line, err);
    result = 1;
  }
  return result;
}

#define TEST_ASSERT(cond, err) test_assert_((cond), err, __FUNCTION__, __LINE__);

#define TEST_RUN(test, run)             \
{                                       \
  if (run) {                            \
    printf("Test: ddsrt_%s\n", #test);  \
    if (test(argv[0]) == 0) {           \
      printf("  [ OK ]\n");             \
    } else {                            \
      printf("  [FAIL]\n");             \
      result = 1;                       \
    }                                   \
    printf("\n");                       \
  }                                     \
}

#define PID_TO_RET(pid) ((int)(int32_t)(pid % 127))

#define PROCESS_DONE_NOTHING_EXIT_CODE (42)

#define ENVIRONMENT_TEST_VAR_NAME  "TEST_ENV_VAR_NAME"
#define ENVIRONMENT_TEST_VAR_VALUE "TEST_ENV_VAR_VALUE"
#define ENVIRONMENT_TEST_OK        (12)
#define ENVIRONMENT_TEST_FAIL      (21)


/*
 * The spawned process.
 */
int process(int argc, char **argv)
{
  ddsrt_pid_t pid = 0;
  int ret = 0;
  int argi = 1;
  char *envptr = NULL;
  long long sleep = -1;

  while (argi < argc) {
    if (strcmp(argv[argi], "--sleep") == 0) {
      argi++;
      if (argi < argc) {
        ddsrt_strtoll(argv[argi], NULL, 0, &sleep);
      } else {
        printf("No --sleep value\n");
        return 123;
      }
    } else if (strcmp(argv[argi], "--retpid") == 0) {
      pid = ddsrt_getpid();
    } else if (strcmp(argv[argi], "--checkenv") == 0) {
      if (ddsrt_getenv(ENVIRONMENT_TEST_VAR_NAME, &envptr) != DDS_RETCODE_OK) {
        envptr = NULL;
        ret = ENVIRONMENT_TEST_FAIL;
      }
    }
    argi++;
  }

  if (argc == 1) {
    printf(" Process: no arguments is used for the create test. Try with the '-a' (all) argument.\n");
    ret = PROCESS_DONE_NOTHING_EXIT_CODE;
  }

  if (sleep > 0) {
    printf(" Process: sleep %d seconds\n", (int)sleep);
    dds_sleepfor(DDS_SECS((int64_t)sleep));
  }

  if (pid != 0) {
    ret = PID_TO_RET(pid);
    printf(" Process: pid %d reduced to %d exit code\n", (int)pid, ret);
  }

  if (envptr != NULL) {
    printf(" Process: env %s=%s\n", ENVIRONMENT_TEST_VAR_NAME, envptr);
    if (strcmp(envptr, ENVIRONMENT_TEST_VAR_VALUE) == 0) {
      ret = ENVIRONMENT_TEST_OK;
    } else {
      ret = ENVIRONMENT_TEST_FAIL;
    }
  }

  return ret;
}


/*
 * Try to create a process without arguments.
 * The exit status of the process should be PROCESS_DONE_NOTHING_EXIT_CODE.
 */
int process_create(const char *exe)
{
  int result = 0;
  dds_retcode_t ret;
  ddsrt_pid_t pid;
  int32_t status;

  ret = ddsrt_process_create(exe, NULL, &pid);
  result = TEST_ASSERT((ret == DDS_RETCODE_OK), "Could not create process");

  if (result == 0) {
    ret = ddsrt_process_wait_exit(pid, DDS_SECS(10), &status);
    result = TEST_ASSERT((ret == DDS_RETCODE_OK), "Failed to wait for process exit");
  }

  if (result == 0) {
    result = TEST_ASSERT((status == PROCESS_DONE_NOTHING_EXIT_CODE), "Unexpected status");
  }

  return result;
}


/*
 * Create a process that'll sleep for a while.
 * Try to destroy that process.
 */
int process_destroy(const char *exe)
{
  int result = 0;
  dds_retcode_t ret;
  ddsrt_pid_t pid;
  int32_t status;
  char *argv[] = { "--sleep", "20", NULL };

  ret = ddsrt_process_create(exe, argv, &pid);
  result = TEST_ASSERT((ret == DDS_RETCODE_OK), "Could not create process");

  if (result == 0) {
    /* Check if process is running. */
    ret = ddsrt_process_wait_exit(pid, 0, &status);
    result = TEST_ASSERT((ret == DDS_RETCODE_TIMEOUT), "Process not running");
  }

  if (result == 0) {
    /* Destroy it. */
    ret = ddsrt_process_terminate(pid, DDS_SECS(10));
    result = TEST_ASSERT((ret == DDS_RETCODE_OK), "Could not destroy process");
  }

  if (result == 0) {
    /* Check if process is actually gone. */
    ret = ddsrt_process_wait_exit(pid, 0, &status);
    result = TEST_ASSERT((ret != DDS_RETCODE_TIMEOUT), "Process not destroyed");
  }

  return result;
}


/*
 * Create a process that'll return it's own pid value (reduced
 * to fit the exit code range). It should match the pid that was
 * returned by the process create (also reduced to be able to
 * match the returned semi-pid value).
 */
int process_pid(const char *exe)
{
  int result = 0;
  dds_retcode_t ret;
  ddsrt_pid_t pid;
  int32_t status;
  char *argv[] = { "--retpid", NULL };

  ret = ddsrt_process_create(exe, argv, &pid);
  result = TEST_ASSERT((ret == DDS_RETCODE_OK), "Could not create process");

  if (result == 0) {
    ret = ddsrt_process_wait_exit(pid, DDS_SECS(10), &status);
    result = TEST_ASSERT((ret == DDS_RETCODE_OK), "Failed to wait for process exit");
  }

  if (result == 0) {
    result = TEST_ASSERT(((int)status == PID_TO_RET(pid)), "Unexpected pid");
  }

  return result;
}


/*
 * Set a environment variable in the parent process.
 * Create a process that should have access to that env var.
 */
int process_env(const char *exe)
{
  int result = 0;
  dds_retcode_t ret;
  ddsrt_pid_t pid;
  int32_t status;
  char *argv[] = { "--checkenv", NULL };

  ret = ddsrt_setenv(ENVIRONMENT_TEST_VAR_NAME, ENVIRONMENT_TEST_VAR_VALUE);
  result = TEST_ASSERT((ret == DDS_RETCODE_OK), "Could not set environment variable");

  if (result == 0) {
    ret = ddsrt_process_create(exe, argv, &pid);
    result = TEST_ASSERT((ret == DDS_RETCODE_OK), "Could not create process");
  }

  if (result == 0) {
    ret = ddsrt_process_wait_exit(pid, DDS_SECS(10), &status);
    result = TEST_ASSERT((ret == DDS_RETCODE_OK), "Failed to wait for process exit");
  }

  if (result == 0) {
    result = TEST_ASSERT((status == ENVIRONMENT_TEST_OK), "Process could not access env var");
  }

  return result;
}


/*
 * Try to create a process with an non-existing executable file.
 * It should fail.
 */
int process_invalid(const char *exe)
{
  int result = 0;
  dds_retcode_t ret;
  ddsrt_pid_t pid;

  /* Don't use 'self executable', but an invalid one. */
  exe = "ProbablyNotAnValidExecutable";
  ret = ddsrt_process_create(exe, NULL, &pid);
  result = TEST_ASSERT((ret == DDS_RETCODE_BAD_PARAMETER), "Process creation did not fail as expected.");

  return result;
}



int main (int argc, char **argv)
{
  int result = 1;
  int test_pid = 0;
  int test_env = 0;
  int test_create = 0;
  int test_destroy = 0;
  int test_invalid = 0;

  if ((argc == 2) && (strcmp(argv[1], "-a") == 0)) {
    /* Run all tests. */
    result = 0;
    test_pid = 1;
    test_env = 1;
    test_create = 1;
    test_destroy = 1;
    test_invalid = 1;
  } else if ((argc == 3) && (strcmp(argv[1], "-t") == 0)) {
    /* Run one specific test. */
    if (strcmp(argv[2],     "pid") == 0) { test_pid     = 1; result = 0; }
    if (strcmp(argv[2],     "env") == 0) { test_env     = 1; result = 0; }
    if (strcmp(argv[2],  "create") == 0) { test_create  = 1; result = 0; }
    if (strcmp(argv[2], "destroy") == 0) { test_destroy = 1; result = 0; }
    if (strcmp(argv[2], "invalid") == 0) { test_invalid = 1; result = 0; }
  } else {
    /* Run the spawned process only. */
    return process(argc, argv);
  }

  /* Run the test(s). */
  TEST_RUN(process_create,  test_create );
  TEST_RUN(process_destroy, test_destroy);
  TEST_RUN(process_pid,     test_pid    );
  TEST_RUN(process_env,     test_env    );
  TEST_RUN(process_invalid, test_invalid);

  return result;
}
