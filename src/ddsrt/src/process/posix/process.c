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
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <sys/types.h>
#include "dds/ddsrt/process.h"
#include "dds/ddsrt/string.h"
#include "dds/ddsrt/heap.h"


ddsrt_pid_t
ddsrt_getpid(void)
{
  /* Mapped to taskIdSelf() in VxWorks kernel mode. */
  return getpid();
}


#ifdef PROCESS_MANAGEMENT_ENABLED


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
    argv_out = ddsrt_calloc(2, sizeof(char*));
    if (argv_out) {
      argv_out[0] = (char*)prefix;
      argv_out[1] = NULL;
    }
  } else {
    size_t argi;
    size_t argc = 0;
    while (argv_in[argc] != NULL) {
      argc++;
    }
    argv_out = ddsrt_calloc((argc + 2), sizeof(char*));
    if (argv_out) {
      argv_out[0] = (char*)prefix;
      for (argi = 0; argi < argc; argi++) {
        argv_out[argi + 1] = (char*)argv_in[argi];
      }
      argv_out[argc + 1] = NULL;
    }
  }

  return argv_out;
}



dds_retcode_t
ddsrt_process_create(
  const char *executable,
  char *const argv[],
  ddsrt_pid_t *pid)
{
  dds_retcode_t rv;
  char **exec_argv;
  int exec_fds[2];
  int exec_err;
  pid_t spawn;
  ssize_t nr;

  assert(executable != NULL);
  assert(pid != NULL);

  /* Prefix the argv with the executable, which is the convention. */
  exec_argv = prefix_argv(executable, argv);
  if (exec_argv == NULL) {
    return DDS_RETCODE_OUT_OF_RESOURCES;
  }

  /* Prepare pipe to know the result of the exec. */
  if (pipe(exec_fds) == -1) {
    rv = DDS_RETCODE_OUT_OF_RESOURCES;
    goto fail_pipe;
  }
  if ((fcntl(exec_fds[0], F_SETFD, fcntl(exec_fds[0], F_GETFD) | FD_CLOEXEC) == -1) ||
      (fcntl(exec_fds[1], F_SETFD, fcntl(exec_fds[1], F_GETFD) | FD_CLOEXEC) == -1) ){
    rv = DDS_RETCODE_ERROR;
    goto fail_fctl;
  }

  /* Create a new process. */
  spawn = fork();
  if (spawn == -1)
  {
    rv = DDS_RETCODE_ERROR;
    goto fail_fork;
  }
  else if (spawn == 0)
  {
    /* Child process */

    /* Run the executable, replacing current process. */
    execv(executable, exec_argv);

    /* If executing this, something has gone wrong */
    exec_err = errno;
    write(exec_fds[1], &exec_err, sizeof(int));
    close(exec_fds[1]);
    close(exec_fds[0]);
    ddsrt_free(exec_argv);
    exit(1);
  }
  else
  {
    /* Parent process */

    /* Get execv result. */
    rv = DDS_RETCODE_ERROR;
    close(exec_fds[1]);
    nr = read(exec_fds[0], &exec_err, sizeof(int));
    if (nr == 0) {
      /* Pipe closed by successful execv. */
      rv = DDS_RETCODE_OK;
    } else if (nr == sizeof(int)) {
      /* Translate execv error. */
      if ((exec_err == ENOENT ) ||
          (exec_err == ENOEXEC) ){
        rv = DDS_RETCODE_BAD_PARAMETER;
      } else if (exec_err == EACCES) {
        rv = DDS_RETCODE_NOT_ALLOWED;
      }
    }
    close(exec_fds[0]);

    /* Remember child pid. */
    *pid = spawn;
  }

  ddsrt_free(exec_argv);
  return rv;

fail_fork:
fail_fctl:
  close(exec_fds[0]);
  close(exec_fds[1]);
fail_pipe:
  ddsrt_free(exec_argv);
  return rv;
}



DDS_EXPORT dds_retcode_t
ddsrt_process_get_exit_code(
  ddsrt_pid_t pid,
  int32_t *code)
{
  dds_retcode_t rv;
  int ret;
  int s;

  ret = waitpid(pid, &s, WNOHANG);
  if (ret == pid) {
    if (code) {
      if (WIFEXITED(s)) {
        *code = WEXITSTATUS(s);
      } else {
        *code = 1;
      }
    }
    rv = DDS_RETCODE_OK;
  } else if (ret == 0) {
    /* Process is still alive. */
    rv = DDS_RETCODE_PRECONDITION_NOT_MET;
  } else if ((ret == -1) && (errno == ECHILD)) {
    /* Unknown pid. */
    rv = DDS_RETCODE_BAD_PARAMETER;
  } else {
    /* Unknown error. */
    rv = DDS_RETCODE_ERROR;
  }

  return rv;
}



dds_retcode_t
ddsrt_process_terminate(
  ddsrt_pid_t pid,
  bool force)
{
  dds_retcode_t rv = DDS_RETCODE_ERROR;
  int ret = -1;

  if (force) {
    /* Forcefully kill. */
    ret = kill(pid, SIGKILL);
  } else {
    /* Try a graceful kill. */
    ret = kill(pid, SIGTERM);
  }

  if (ret == 0) {
    rv = DDS_RETCODE_OK;
  } else if (ret == -1) {
    if (errno == EPERM) {
      rv = DDS_RETCODE_ILLEGAL_OPERATION;
    } else if (errno == ESRCH) {
      rv = DDS_RETCODE_BAD_PARAMETER;
    }
  }

  return rv;
}

#endif /* PROCESS_MANAGEMENT_ENABLED */
