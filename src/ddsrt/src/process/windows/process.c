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
#include <assert.h>
#include <process.h>
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/time.h"
#include "dds/ddsrt/string.h"
#include "dds/ddsrt/atomics.h"
#include "dds/ddsrt/process.h"
#include "dds/ddsrt/timeconv.h"


ddsrt_pid_t
ddsrt_getpid(void)
{
  return GetCurrentProcessId();
}



static HANDLE        pid_to_phdl          (ddsrt_pid_t pid);
static dds_retcode_t process_get_exit_code(HANDLE phdl, int32_t *code);
static dds_retcode_t process_kill         (HANDLE phdl);
static char*         commandline          (const char *exe, char *const argv_in[]);
static BOOL          child_add            (HANDLE phdl);
static void          child_remove         (HANDLE phdl);
static DWORD         child_list           (HANDLE *list, DWORD max);
static HANDLE        child_handle         (ddsrt_pid_t pid);



dds_retcode_t
ddsrt_proc_create(
  const char *executable,
  char *const argv[],
  ddsrt_pid_t *pid)
{
  dds_retcode_t rv = DDS_RETCODE_ERROR;
  PROCESS_INFORMATION process_info;
  STARTUPINFO si;
  char *cmd;
  LPTCH environment;

  assert(executable != NULL);
  assert(pid != NULL);

  cmd = commandline(executable, argv);
  if (cmd == NULL) {
    return DDS_RETCODE_OUT_OF_RESOURCES;
  }

  memset(&si, 0, sizeof(STARTUPINFO));
  si.cb = sizeof(STARTUPINFO);

  /* The new process will inherit the input/output handles. */
  /* TODO: Redirect is not working yet. */
  si.dwFlags = STARTF_USESTDHANDLES;
  si.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
  si.hStdInput  = GetStdHandle(STD_INPUT_HANDLE);
  si.hStdError  = GetStdHandle(STD_ERROR_HANDLE);

  /* Get the environment variables to pass along. */
  environment = GetEnvironmentStrings();
  if(environment){
    BOOL created;
    created = CreateProcess(executable,               // ApplicationName
                            cmd,                      // CommandLine
                            NULL,                     // ProcessAttributes
                            NULL,                     // ThreadAttributes
                            TRUE,                     // InheritHandles
                            CREATE_NO_WINDOW,         // dwCreationFlags
                            (LPVOID)environment,      // Environment
                            NULL,                     // CurrentDirectory
                            &si,                      // StartupInfo
                            &process_info);           // ProcessInformation
    if (created) {
      if (child_add(process_info.hProcess)) {
        *pid = process_info.dwProcessId;
        rv = DDS_RETCODE_OK;
      } else {
        process_kill(process_info.hProcess);
        rv = DDS_RETCODE_OUT_OF_RESOURCES;
      }
    } else {
      DWORD error = GetLastError();
      if ((ERROR_FILE_NOT_FOUND == error) ||
          (ERROR_PATH_NOT_FOUND == error)) {
        rv = DDS_RETCODE_BAD_PARAMETER;
      } else if (ERROR_ACCESS_DENIED  == error) {
        rv = DDS_RETCODE_NOT_ALLOWED;
      }
    }
    FreeEnvironmentStrings(environment);
  }

  ddsrt_free(cmd);

  return rv;
}



dds_retcode_t
ddsrt_proc_waitpid(
  ddsrt_pid_t pid,
  dds_duration_t timeout,
  int32_t *code)
{
  dds_retcode_t rv = DDS_RETCODE_OK;
  HANDLE phdl;
  DWORD ret;

  if (timeout < 0) {
    return DDS_RETCODE_BAD_PARAMETER;
  }

  phdl = child_handle(pid);
  if (phdl == 0) {
    return DDS_RETCODE_NOT_FOUND;
  }

  if (timeout > 0) {
    ret = WaitForSingleObject(phdl, ddsrt_duration_to_msecs_ceil(timeout));
    if (ret != WAIT_OBJECT_0) {
      if (ret == WAIT_TIMEOUT) {
        rv = DDS_RETCODE_TIMEOUT;
      } else {
        rv = DDS_RETCODE_ERROR;
      }
    }
  }

  if (rv == DDS_RETCODE_OK) {
    rv = process_get_exit_code(phdl, code);
  }

  if (rv == DDS_RETCODE_OK) {
    child_remove(phdl);
  }


  return rv;
}



dds_retcode_t
ddsrt_proc_waitpids(
  dds_duration_t timeout,
  ddsrt_pid_t *pid,
  int32_t *code)
{
  dds_retcode_t rv = DDS_RETCODE_OK;
  HANDLE hdls[MAXIMUM_WAIT_OBJECTS];
  HANDLE phdl;
  DWORD cnt;
  DWORD ret;

  if (timeout < 0) {
    return DDS_RETCODE_BAD_PARAMETER;
  }

  cnt = child_list(hdls, MAXIMUM_WAIT_OBJECTS);
  if (cnt == 0) {
    return DDS_RETCODE_NOT_FOUND;
  }

  ret = WaitForMultipleObjects(cnt, hdls, FALSE, ddsrt_duration_to_msecs_ceil(timeout));
  if ((ret < WAIT_OBJECT_0) || (ret >= (WAIT_OBJECT_0 + cnt))) {
    if (ret == WAIT_TIMEOUT) {
      if (timeout == 0) {
        rv = DDS_RETCODE_PRECONDITION_NOT_MET;
      } else {
        rv = DDS_RETCODE_TIMEOUT;
      }
    } else {
      rv = DDS_RETCODE_ERROR;
    }
  } else {
    /* Get the handle of the specific child that was triggered. */
    phdl = hdls[ret - WAIT_OBJECT_0];
  }

  if (rv == DDS_RETCODE_OK) {
    rv = process_get_exit_code(phdl, code);
  }

  if (rv == DDS_RETCODE_OK) {
    if (pid) {
      *pid = GetProcessId(phdl);
    }
    child_remove(phdl);
  }

  return rv;
}



dds_retcode_t
ddsrt_proc_exists(
  ddsrt_pid_t pid)
{
  dds_retcode_t rv = DDS_RETCODE_NOT_FOUND;
  HANDLE phdl;

  phdl = pid_to_phdl(pid);
  if (phdl != 0) {
    rv = process_get_exit_code(phdl, NULL);
    if (rv == DDS_RETCODE_PRECONDITION_NOT_MET) {
      /* Process still exists. */
      rv = DDS_RETCODE_OK;
    } else if (rv == DDS_RETCODE_OK) {
      /* The process has gone. */
      rv = DDS_RETCODE_NOT_FOUND;
    } else {
      rv = DDS_RETCODE_ERROR;
    }
    CloseHandle(phdl);
  }

  return rv;
}



dds_retcode_t
ddsrt_proc_kill(
  ddsrt_pid_t pid)
{
  dds_retcode_t rv = DDS_RETCODE_BAD_PARAMETER;
  HANDLE phdl;

  phdl = pid_to_phdl(pid);
  if (phdl != 0) {
    /* Forcefully kill. */
    rv = process_kill(phdl);
    CloseHandle(phdl);
  }

  return rv;
}



static HANDLE
pid_to_phdl(ddsrt_pid_t pid)
{
  return OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | SYNCHRONIZE | PROCESS_TERMINATE, FALSE, pid);
}



static dds_retcode_t
process_get_exit_code(
  HANDLE phdl,
  int32_t *code)
{
  dds_retcode_t rv = DDS_RETCODE_ERROR;
  DWORD tr;

  assert(phdl != 0);

  if (GetExitCodeProcess(phdl, &tr)) {
    if (tr == STILL_ACTIVE) {
      rv = DDS_RETCODE_PRECONDITION_NOT_MET;
    } else {
      if (code) {
        *code = (int32_t)tr;
      }
      rv = DDS_RETCODE_OK;
    }
  }

  return rv;
}



/* Forcefully kill the given process. */
static dds_retcode_t
process_kill(HANDLE phdl)
{
  assert(phdl != 0);
  if (TerminateProcess(phdl, 1 /* exit code */) != 0) {
    return DDS_RETCODE_OK;
  }
  return DDS_RETCODE_ERROR;
}



/* Add quotes to a given string, escape it and add it to a buffer. */
static char*
insert_char(char *buf, size_t *len, size_t *max, char c)
{
  static const size_t buf_inc = 64;
  if (*len == *max) {
    *max += buf_inc;
    buf = ddsrt_realloc(buf, *max);
  }
  if (buf) {
    buf[(*len)++] = c;
  }
  return buf;
}
static char*
stringify_cat(char *buf, size_t *len, size_t *max, const char *str)
{
  char last = '\0';

  /* Start stringification with an opening double-quote. */
  buf = insert_char(buf, len, max, '\"');
  if (!buf) goto end;

  /* Copy and escape the string. */
  while ((*str) != '\0') {
    if (*str == '\"') {
      buf = insert_char(buf, len, max, '\\');
      if (!buf) goto end;
    }
    buf = insert_char(buf, len, max, *str);
    if (!buf) goto end;
    last = *str;
    str++;
  }

  /* For some reason, only the last backslash will be stripped.
   * No need to escape the other backslashes. */
  if (last == '\\') {
    buf = insert_char(buf, len, max, '\\');
    if (!buf) goto end;
  }

  /* End stringification. */
  buf = insert_char(buf, len, max, '\"');
  if (!buf) goto end;
  buf = insert_char(buf, len, max, ' ');

end:
  return buf;
}



/* Create command line with executable and arguments. */
static char*
commandline(const char *exe, char *const argv_in[])
{
  char *cmd = NULL;
  size_t len = 0;
  size_t max = 0;
  size_t argi;

  assert(exe);

  /* Add quoted and escaped executable. */
  cmd = stringify_cat(cmd, &len, &max, exe);
  if (!cmd) goto end;

  /* Add quoted and escaped arguments. */
  if (argv_in != NULL) {
    for (argi = 0; argv_in[argi] != NULL; argi++) {
      cmd = stringify_cat(cmd, &len, &max, argv_in[argi]);
      if (!cmd) goto end;
    }
  }

  /* Terminate command line string. */
  cmd = insert_char(cmd, &len, &max, '\0');

end:
  return cmd;
}


/* Maintain a list of children to be able to wait for all them. */
static ddsrt_atomic_voidp_t g_children[MAXIMUM_WAIT_OBJECTS] = {0};

static BOOL
child_update(HANDLE old, HANDLE new)
{
  BOOL updated = FALSE;
  for (int i = 0; (i < MAXIMUM_WAIT_OBJECTS) && (!updated); i++)
  {
    updated = ddsrt_atomic_casvoidp(&(g_children[i]), old, new);
  }
  return updated;
}

static BOOL
child_add(HANDLE phdl)
{
  return child_update(0, phdl);
}

static void
child_remove(HANDLE phdl)
{
  (void)child_update(phdl, 0);
}

static DWORD
child_list(HANDLE *list, DWORD max)
{
  HANDLE hdl;
  int cnt = 0;
  assert(list);
  assert(max <= MAXIMUM_WAIT_OBJECTS);
  for (int i = 0; (i < MAXIMUM_WAIT_OBJECTS); i++)
  {
    hdl = ddsrt_atomic_ldvoidp(&(g_children[i]));
    if (hdl != 0) {
      list[cnt++] = hdl;
    }
  }
  return cnt;
}

static HANDLE
child_handle(ddsrt_pid_t pid)
{
  HANDLE phdl = 0;

  for (int i = 0; (i < MAXIMUM_WAIT_OBJECTS) && (phdl == 0); i++)
  {
    phdl = ddsrt_atomic_ldvoidp(&(g_children[i]));
    if (phdl != 0) {
      if (GetProcessId(phdl) != pid) {
        phdl = 0;
      }
    }
  }

  return phdl;
}
