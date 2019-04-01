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
#include <errno.h>
#include <assert.h>
#include <process.h>
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/time.h"
#include "dds/ddsrt/string.h"
#include "dds/ddsrt/process.h"
#include "dds/ddsrt/timeconv.h"


ddsrt_pid_t
ddsrt_getpid(void)
{
  return GetCurrentProcessId();
}



static HANDLE        pid_to_phdl          (ddsrt_pid_t pid);
static dds_retcode_t process_get_exit_code(HANDLE phdl, int32_t *code);
static dds_retcode_t process_terminate    (HANDLE phdl);
static dds_retcode_t process_kill         (HANDLE phdl);
static char*         commandline          (const char *exe, char *const argv_in[]);



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
      *pid = process_info.dwProcessId;
      rv = DDS_RETCODE_OK;
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
ddsrt_proc_get_exit_code(
  ddsrt_pid_t pid,
  int32_t *code)
{
  dds_retcode_t rv = DDS_RETCODE_BAD_PARAMETER;
  HANDLE phdl;

  phdl = pid_to_phdl(pid);
  if (phdl != 0) {
    rv = process_get_exit_code(phdl, code);
    CloseHandle (phdl);
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
ddsrt_proc_term(
  ddsrt_pid_t pid)
{
  dds_retcode_t rv = DDS_RETCODE_BAD_PARAMETER;
  HANDLE phdl;

  phdl = pid_to_phdl(pid);
  if (phdl != 0) {
    /* Try a graceful kill. */
    rv = process_terminate(phdl);
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



/*
 * This function will try to inject a thread into the given process.
 * That thread will execute the ExitProcess() call, which should cause
 * the process to terminate somewhat gracefully.
 */
static dds_retcode_t
process_terminate(HANDLE phdl)
{
  dds_retcode_t rv;
  HINSTANCE kernel32;
  HANDLE remoteHandle;
  HANDLE dupHandle;
  BOOL duplicated;
  DWORD threadId;
  DWORD waitres;

  assert(phdl != 0);

  /* Check if target process is actually still alive. */
  rv = process_get_exit_code(phdl, NULL);
  if (rv == DDS_RETCODE_PRECONDITION_NOT_MET) {
    rv = DDS_RETCODE_ERROR;
    kernel32 = GetModuleHandle("Kernel32");
    if (kernel32) {
      /* Ensure that we will have rights to create a thread
       * in the remote process. */
      duplicated = DuplicateHandle(GetCurrentProcess(),
                                   phdl,
                                   GetCurrentProcess(),
                                   &dupHandle,
                                   PROCESS_ALL_ACCESS,
                                   FALSE,
                                   0);
      if (duplicated) {
        phdl = dupHandle;
      }

      /* Try terminate the process by injecting a thread
       * that will execute a ExitProcess(). */
      FARPROC funcExitProcess;
      funcExitProcess = GetProcAddress(kernel32, "ExitProcess");
      if (funcExitProcess) {
        remoteHandle = CreateRemoteThread(phdl,
                                          NULL,
                                          0,
                                          (LPTHREAD_START_ROUTINE)funcExitProcess,
                                          (LPVOID)1,  /* Exit code */
                                          0,
                                          &threadId);
        if (remoteHandle != 0) {
          /* Wait for the process to exit before closing the handle. */
          waitres = WaitForSingleObject(phdl, 10000);
          if (waitres == 0) {
            rv = DDS_RETCODE_OK;
          }
          CloseHandle(remoteHandle);
        }
      }
    }

    if (duplicated) {
      CloseHandle(dupHandle);
    }
  }

  return rv;
}



/* Forcefully kill the given process. */
static dds_retcode_t
process_kill(HANDLE phdl)
{
  assert(phdl != 0);
  if (TerminateProcess(phdl, 1 /* exit code */) == 0) {
    return DDS_RETCODE_OK;
  }
  return DDS_RETCODE_ERROR;
}



/* Add quotes to a given string and escape. */
static size_t
stringify_len(const char *str)
{
  const char *esc;
  size_t escapes = 0;
  for (esc = strchr(str, '\"'); esc != NULL; esc = strchr(esc, '\"')) {
    escapes++;
    esc++;
  }
  return strlen(str) + escapes + 5; /* '\"' +
                                       str +
                                       nr_of_quotes +
                                       possible_trailing_bslash +
                                       '\"' + ' ' + '\0' */
}
static char*
stringify(char *buf, const char *str)
{
  const char *ptr = str;
  const char *esc;
  size_t len = strlen(str);
  size_t idx = 0;
  char last = '\0';

  if (len > 0) {
    last = str[len - 1];
  }

  buf[idx++] = '\"';
  while (len > 0) {
    esc = strchr(ptr, '\"');
    if (esc) {
      size_t n = (size_t)(esc - ptr);
      assert(esc >= ptr);
      if (n != 0) {
        /* Copy part before escape. */
        memcpy(&(buf[idx]), ptr, n);
        idx += n;
      }
      /* Escape the double quote. */
      buf[idx++] = '\\';
      buf[idx++] = '\"';
      /* Continue after the offending char. */
      ptr = esc + 1;
      len -= (n + 1);
    } else {
      /* No (more) chars to escape. */
      memcpy(&(buf[idx]), ptr, len);
      idx += len;
      len = 0;
    }
  }
  /* For some reason, the last backslash will be stripped. */
  if (last == '\\') {
    buf[idx++] = '\\';
  }
  buf[idx++] = '\"';
  buf[idx++] = ' ';
  buf[idx  ] = '\0';
  return &(buf[idx]);
}



/* Create command line with executable and arguments. */
static char*
commandline(const char *exe, char *const argv_in[])
{
  char *cmd;
  size_t cmdlen;

  assert(exe);
  cmdlen = stringify_len(exe);

  if (argv_in == NULL) {
    cmd = ddsrt_malloc(cmdlen);
    if (cmd) {
      stringify(cmd, exe);
    }
  } else {
    char *ptr;
    size_t argi;
    for (argi = 0; argv_in[argi] != NULL; argi++) {
      cmdlen += stringify_len(argv_in[argi]);
    }
    cmd = ddsrt_malloc(cmdlen);
    if (cmd) {
      ptr = stringify(cmd, exe);
      for (argi = 0; argv_in[argi] != NULL; argi++) {
        ptr = stringify(ptr, argv_in[argi]);
      }
    }
  }

  return cmd;
}

