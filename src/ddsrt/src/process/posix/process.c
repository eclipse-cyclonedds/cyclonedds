/*
 * Copyright(c) 2006 to 2018 ADLINK Technology Limited and others
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
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <stdlib.h>
#include <sys/types.h>
#include "dds/ddsrt/process.h"


ddsrt_pid_t
ddsrt_getpid(void)
{
  /* Mapped to taskIdSelf() in VxWorks kernel mode. */
  return getpid();
}


#ifdef DDSRT_USE_PROCESSCREATION

#ifndef PIKEOS_POSIX
#include <sys/wait.h>
#endif

#if defined(__APPLE__)
#include <crt_externs.h>
#else
/* environ is a variable declared in unistd.h, and it keeps track
 * of the environment variables during this running process. */
extern char **environ;
#endif

#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/string.h"



/*
 * This'll take a argv and prefixes it with the given prefix.
 * If argv is NULL, the new argv array will only contain the prefix and a NULL.
 * The result array is always terminated with NULL.
 */
static char**
prefix_argv(const char *prefix, char *const argv_in[])
{
  char **argv_out;

  assert(prefix);

  if (argv_in == NULL) {
    argv_out = ddsrt_malloc(2 * sizeof(char*));
    argv_out[0] = (char*)prefix;
    argv_out[1] = NULL;
  } else {
    size_t argi;
    size_t argc = 0;
    while (argv_in[argc] != NULL) {
      argc++;
    }
    argv_out = ddsrt_malloc((argc + 2) * sizeof(char*));
    argv_out[0] = (char*)prefix;
    for (argi = 0; argi < argc; argi++) {
      argv_out[argi + 1] = (char*)argv_in[argi];
    }
    argv_out[argc + 1] = NULL;
  }

  return argv_out;
}



dds_retcode_t
ddsrt_process_create(
  const char *executable,
  char *const argv[],
  ddsrt_pid_t *pid)
{
  dds_retcode_t rv = DDS_RETCODE_OK;
  pid_t spawn;

  assert(executable != NULL);
  assert(pid != NULL);

  /* Does it exist and is it actually an executable? */
  if (access(executable, X_OK) != 0) {
    rv = DDS_RETCODE_BAD_PARAMETER;
  } else {
    /* Create a new process. */
    spawn = fork();
    if (spawn == -1) {
      rv = DDS_RETCODE_ERROR;
    } else if (spawn == 0) {
      /* Child process */
      char **argvexec;
#if defined(__APPLE__)
      char **environ = *_NSGetEnviron ();
#endif

      /* First prefix the argv with the executable, which is the convention. */
      argvexec = prefix_argv(executable, argv);

      /* Then execute the executable, replacing current process. */
      execve(executable, argvexec, environ);

      /* If executing this, something has gone wrong */
      ddsrt_free(argvexec);
      exit(1);
    } else {
      /* Parent process */
      *pid = spawn;
    }
  }

  return rv;
}



dds_retcode_t
ddsrt_process_wait_exit(
  ddsrt_pid_t pid,
  dds_duration_t timeout,
  int32_t *status)
{
  static const dds_duration_t poll = DDS_MSECS(50);
  dds_retcode_t rv = DDS_RETCODE_TIMEOUT;
  int ret;
  int s;

  /*
   * Polling.
   * It would be preferable to replace this with with a better
   * exit detection. But it'll do for now.
   */
  while (rv == DDS_RETCODE_TIMEOUT) {
    ret = waitpid(pid, &s, WNOHANG);
    if (ret == pid) {
      if (status) {
        if (WIFEXITED(s)) {
          *status = WEXITSTATUS(s);
        } else {
          *status = 1;
        }
      }
      rv = DDS_RETCODE_OK;
    } else if (ret == 0) {
      /* Process is still alive. */
      timeout -= poll;
      if (timeout < 0) {
        break;
      }
      dds_sleepfor(poll);
    } else if ((ret == -1) && (errno == ECHILD)) {
      rv = DDS_RETCODE_BAD_PARAMETER;
    } else {
      rv = DDS_RETCODE_ERROR;
    }
  }

  return rv;
}



dds_retcode_t
ddsrt_process_terminate(
  ddsrt_pid_t pid,
  dds_duration_t timeout)
{
  dds_retcode_t rv = DDS_RETCODE_ERROR;

  /* Try a graceful kill. */
  if (kill(pid, SIGTERM) == 0) {
    /* wait for process to be gone */
    rv = ddsrt_process_wait_exit(pid, timeout, NULL);
  }

  /* Escalate when not killed yet. */
  if (rv == DDS_RETCODE_TIMEOUT) {
    /* Forcefully kill. */
    kill(pid, SIGKILL);
  }

  return rv;
}

#endif /* DDSRT_USE_PROCESSCREATION */
