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
#include "CUnit/Test.h"
#include "process_test.h"
#include "dds/ddsrt/time.h"
#include "dds/ddsrt/strtol.h"
#include "dds/ddsrt/environ.h"
#include "dds/ddsrt/process.h"


/*
 * Create a process that is expected to exit quickly.
 * Compare the exit code with the expected exit code.
 */
void create_and_test_exit(const char *arg, int code)
{
  dds_retcode_t ret;
  ddsrt_pid_t pid;
  int32_t status;
  char *argv[] = { NULL, NULL };

  argv[0] = (char*)arg;
  ret = ddsrt_process_create(TEST_APPLICATION, argv, &pid);
  CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);

  ret = ddsrt_process_wait_exit(pid, DDS_SECS(10), &status);
  CU_ASSERT_EQUAL(ret, DDS_RETCODE_OK);

  /* Check result. */
  CU_ASSERT_EQUAL(status, code);

  /* Garbage collection when needed. */
  if (ret != DDS_RETCODE_OK) {
    ddsrt_process_terminate(pid, DDS_SECS(10));
  }
}


/*
 * Try to create a process without arguments.
 * The exit status of the process should be PROCESS_DONE_NOTHING_EXIT_CODE.
 */
CU_Test(ddsrt_process, create)
{
  create_and_test_exit(NULL, TEST_CREATE_EXIT);
}


/*
 * Create a process that'll sleep for a while.
 * Try to destroy that process.
 */
CU_Test(ddsrt_process, destroy)
{
  dds_retcode_t ret;
  ddsrt_pid_t pid;
  int32_t status;
  char *argv[] = { TEST_DESTROY_ARG0, TEST_DESTROY_ARG1, NULL };

  ret = ddsrt_process_create(TEST_APPLICATION, argv, &pid);
  CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
  CU_ASSERT_NOT_EQUAL_FATAL(pid, 0);

  /* Check if process is running. */
  ret = ddsrt_process_wait_exit(pid, 0, &status);
  CU_ASSERT_EQUAL(ret, DDS_RETCODE_TIMEOUT);

  /* Destroy it. */
  ret = ddsrt_process_terminate(pid, DDS_SECS(10));
  CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);

  /* Check if process is actually gone. */
  ret = ddsrt_process_wait_exit(pid, 0, &status);
  CU_ASSERT_NOT_EQUAL(ret, DDS_RETCODE_TIMEOUT);
}


/*
 * Create a process that'll return it's own pid value (reduced
 * to fit the exit code range). It should match the pid that was
 * returned by the process create (also reduced to be able to
 * match the returned semi-pid value).
 */
CU_Test(ddsrt_process, pid)
{
  dds_retcode_t ret;
  ddsrt_pid_t pid;
  int32_t status;
  char *argv[] = { TEST_PID_ARG, NULL };

  ret = ddsrt_process_create(TEST_APPLICATION, argv, &pid);
  CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
  CU_ASSERT_NOT_EQUAL_FATAL(pid, 0);

  ret = ddsrt_process_wait_exit(pid, DDS_SECS(10), &status);
  CU_ASSERT_EQUAL(ret, DDS_RETCODE_OK);

  /* Compare the pid values. */
  CU_ASSERT_EQUAL(status, TEST_PID_EXIT(pid));

  /* Garbage collection when needed. */
  if (ret != DDS_RETCODE_OK) {
    ddsrt_process_terminate(pid, DDS_SECS(10));
  }
}


/*
 * Set a environment variable in the parent process.
 * Create a process that should have access to that env var.
 */
CU_Test(ddsrt_process, env)
{
  dds_retcode_t ret;

  ret = ddsrt_setenv(TEST_ENV_VAR_NAME, TEST_ENV_VAR_VALUE);
  CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);

  create_and_test_exit(TEST_ENV_ARG, TEST_ENV_EXIT);
}


/*
 * Try to create a process with an non-existing executable file.
 * It should fail.
 */
CU_Test(ddsrt_process, invalid)
{
  dds_retcode_t ret;
  ddsrt_pid_t pid;

  ret = ddsrt_process_create("ProbablyNotAnValidExecutable", NULL, &pid);
  CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_BAD_PARAMETER);

  /* Garbage collection when needed. */
  if (ret == DDS_RETCODE_OK) {
    ddsrt_process_terminate(pid, DDS_SECS(10));
  }
}


/*
 * Create a process with a backslash in the argument
 */
CU_Test(ddsrt_process, arg_bslash)
{
  create_and_test_exit(TEST_BSLASH_ARG, TEST_BSLASH_EXIT);
}


/*
 * Create a process with a double-quote in the argument
 */
CU_Test(ddsrt_process, arg_dquote)
{
  create_and_test_exit(TEST_DQUOTE_ARG, TEST_DQUOTE_EXIT);
}

