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
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <sys/types.h>
#include "dds/ddsrt/process.h"
#include "dds/ddsrt/string.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/log.h"


ddsrt_pid_t
ddsrt_getpid(void)
{
  /* Mapped to taskIdSelf() in VxWorks kernel mode. */
  return getpid();
}


/*
 * This'll take a argv and prefixes it with the given prefix.
 * If argv is NULL, the new argv array will only contain the prefix and a NULL.
 * The result array is always terminated with NULL.
 */
static char**
prefix_argv(const char *prefix, char *const argv_in[])
{
  char **argv_out;
  size_t argc = 0;

  assert(prefix);

  if (argv_in != NULL) {
    while (argv_in[argc] != NULL) {
      argc++;
    }
  }

  argv_out = ddsrt_calloc((argc + 2), sizeof(char*));
  if (argv_out) {
    size_t argi;
    argv_out[0] = (char*)prefix;
    for (argi = 0; argi < argc; argi++) {
      argv_out[argi + 1] = (char*)argv_in[argi];
    }
    argv_out[argc + 1] = NULL;
  }

  return argv_out;
}


static void no_op(int sig)
{
  (void)sig;
}


static dds_retcode_t
waitpids(
  ddsrt_pid_t request_pid,
  dds_duration_t timeout,
  ddsrt_pid_t *child_pid,
  int32_t *code)
{
  struct sigaction sigactold;
  struct sigaction sigact;
  dds_retcode_t rv;
  int options = 0;
  int ret;
  int s;

  if (timeout < 0) {
    return DDS_RETCODE_BAD_PARAMETER;
  }

  if (timeout == 0) {
    options = WNOHANG;
  } else if (timeout != DDS_INFINITY) {
    /* Round-up timemout to alarm seconds. */
    unsigned secs;
    secs = (unsigned)(timeout / DDS_NSECS_IN_SEC);
    if ((timeout % DDS_NSECS_IN_SEC) != 0) {
      secs++;
    }
    /* Be sure that the SIGALRM only wakes up waitpid. */
    sigemptyset (&sigact.sa_mask);
    sigact.sa_handler = no_op;
    sigact.sa_flags = 0;
    sigaction (SIGALRM, &sigact, &sigactold);
    /* Now, set the alarm. */
    alarm(secs);
  }

  ret = waitpid(request_pid, &s, options);
  if (ret > 0) {
    if (code) {
      if (WIFEXITED(s)) {
        *code = WEXITSTATUS(s);
      } else {
        *code = 1;
      }
    }
    if (child_pid) {
      *child_pid = ret;
    }
    rv = DDS_RETCODE_OK;
  } else if (ret == 0) {
    /* Process is still alive. */
    rv = DDS_RETCODE_PRECONDITION_NOT_MET;
  } else if ((ret == -1) && (errno == EINTR)) {
    /* Interrupted,
     * so process(es) likely didn't change state and are/is alive. */
    rv = DDS_RETCODE_TIMEOUT;
  } else if ((ret == -1) && (errno == ECHILD)) {
    /* Unknown pid. */
    rv = DDS_RETCODE_NOT_FOUND;
  } else {
    /* Unknown error. */
    rv = DDS_RETCODE_ERROR;
  }

  if ((timeout != 0) && (timeout != DDS_INFINITY)) {
    /* Clean the alarm. */
    alarm(0);
    /* Reset SIGALRM actions. */
    sigaction(SIGALRM, &sigactold, NULL);
  }

  return rv;
}



dds_retcode_t
ddsrt_proc_create(
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
    if (write(exec_fds[1], &exec_err, sizeof(int)) < (ssize_t)sizeof(int)) {
      DDS_ERROR("Could not write proc error pipe.\n");
    }
    close(exec_fds[1]);
    close(exec_fds[0]);
    ddsrt_free(exec_argv);
    _exit(1);
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

    if (rv == DDS_RETCODE_OK) {
      /* Remember child pid. */
      *pid = spawn;
    } else {
      /* Remove the failed fork pid from the system list. */
      waitpid(spawn, NULL, 0);
    }
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



dds_retcode_t
ddsrt_proc_waitpid(
  ddsrt_pid_t pid,
  dds_duration_t timeout,
  int32_t *code)
{
  if (pid > 0) {
    return waitpids(pid, timeout, NULL, code);
  }
  return DDS_RETCODE_BAD_PARAMETER;
}



dds_retcode_t
ddsrt_proc_waitpids(
  dds_duration_t timeout,
  ddsrt_pid_t *pid,
  int32_t *code)
{
  return waitpids(0, timeout, pid, code);
}



dds_retcode_t
ddsrt_proc_exists(
  ddsrt_pid_t pid)
{
  if (kill(pid, 0) == 0)
    return DDS_RETCODE_OK;
  else if (errno == EPERM)
    return DDS_RETCODE_OK;
  else if (errno == ESRCH)
    return DDS_RETCODE_NOT_FOUND;
  else
    return DDS_RETCODE_ERROR;
}



dds_retcode_t
ddsrt_proc_kill(
  ddsrt_pid_t pid)
{
  if (kill(pid, SIGKILL) == 0)
    return DDS_RETCODE_OK;
  else if (errno == EPERM)
    return DDS_RETCODE_ILLEGAL_OPERATION;
  else if (errno == ESRCH)
    return DDS_RETCODE_BAD_PARAMETER;
  else
    return DDS_RETCODE_ERROR;
}
