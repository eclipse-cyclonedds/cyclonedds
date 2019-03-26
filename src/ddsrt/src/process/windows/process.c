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


ddsrt_pid_t
ddsrt_getpid(void)
{
  return GetCurrentProcessId();
}


#ifdef DDSRT_USE_PROCESSCREATION

static HANDLE        pid_to_phdl         (ddsrt_pid_t pid);
static dds_retcode_t process_wait_exit   (HANDLE phdl, dds_duration_t timeout, int32_t *status);
static dds_retcode_t process_terminate   (HANDLE phdl, dds_duration_t timeout);
static dds_retcode_t process_kill        (HANDLE phdl, dds_duration_t timeout);
static char*         commandline         (const char *exe, char *const argv_in[]);

dds_retcode_t
ddsrt_process_create(
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
          (ERROR_PATH_NOT_FOUND == error) ||
          (ERROR_ACCESS_DENIED  == error)) {
        rv = DDS_RETCODE_BAD_PARAMETER;
      }
    }
    FreeEnvironmentStrings(environment);
  }

  ddsrt_free(cmd);

  return rv;
}



dds_retcode_t
ddsrt_process_wait_exit(
  ddsrt_pid_t pid,
  dds_duration_t timeout,
  int32_t *status)
{
  dds_retcode_t rv = DDS_RETCODE_BAD_PARAMETER;
  HANDLE phdl;

  phdl = pid_to_phdl(pid);
  if (phdl != 0) {
    rv = process_wait_exit(phdl, timeout, status);
    CloseHandle (phdl);
  }

  return rv;
}



dds_retcode_t
ddsrt_process_terminate(
  ddsrt_pid_t pid,
  dds_duration_t timeout)
{
  dds_retcode_t rv = DDS_RETCODE_BAD_PARAMETER;
  HANDLE phdl;

  phdl = pid_to_phdl(pid);
  if (phdl != 0) {
    /* Try a graceful kill. */
    rv = process_terminate(phdl, timeout);

    /* Escalate when not killed yet. */
    if (rv != DDS_RETCODE_OK) {
      /* Forcefully kill. */
      rv = process_kill(phdl, timeout);
      if (rv == DDS_RETCODE_OK) {
        rv = DDS_RETCODE_TIMEOUT;
      }
    }
    CloseHandle(phdl);
  }

  return rv;
}



#if (_WIN32_WINNT <= 0x0502)
static DWORD
processQueryFlag(void)
{
  DWORD flag;

  /* For Windows Server 2003 and Windows XP:  PROCESS_QUERY_LIMITED_INFORMATION is not supported.
   * PROCESS_QUERY_INFORMATION need higher access permission which are not always granted when using OpenSplice
   * as a windows service due to UAC so use PROCESS_QUERY_LIMITED_INFORMATION instead */
  if(LOBYTE(LOWORD(GetVersion())) < 6) {
    flag = PROCESS_QUERY_INFORMATION;
  } else {
    flag = 0x1000; /* PROCESS_QUERY_LIMITED_INFORMATION */
  }
  return flag;
}
#else
# define processQueryFlag() PROCESS_QUERY_LIMITED_INFORMATION
#endif

static HANDLE
pid_to_phdl(ddsrt_pid_t pid)
{
  return OpenProcess(processQueryFlag() | SYNCHRONIZE | PROCESS_TERMINATE, FALSE, pid);
}



static dds_retcode_t
process_wait_exit(
  HANDLE phdl,
  dds_duration_t timeout,
  int32_t *status)
{
  dds_retcode_t rv = DDS_RETCODE_TIMEOUT;
  dds_duration_t poll = DDS_MSECS(10);
  BOOL ret;
  DWORD tr;

  assert(phdl != 0);

  /*
   * Polling.
   * It would be preferable to replace this with with a better
   * exit detection. But it'll do for now.
   */
  while (rv == DDS_RETCODE_TIMEOUT) {
    ret = GetExitCodeProcess(phdl, &tr);
    if (!ret) {
      if (errno == ERROR_INVALID_HANDLE) {
        rv = DDS_RETCODE_BAD_PARAMETER;
      } else {
        rv = DDS_RETCODE_ERROR;
      }
    } else if (tr == STILL_ACTIVE) {
      /* Process is still alive. */
      timeout -= poll;
      if (timeout < 0) {
        break;
      }
      dds_sleepfor(poll);
    } else {
      if (status) {
        *status = (int32_t)tr;
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
process_terminate(
  HANDLE phdl,
  dds_duration_t timeout)
{
  dds_retcode_t rv = DDS_RETCODE_ERROR;
  HINSTANCE kernel32;
  HANDLE remoteHandle;
  HANDLE dupHandle;
  BOOL duplicated;
  DWORD threadId;
  DWORD waitres;
  DWORD waitmsec;

  assert(phdl != 0);

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

    rv = process_wait_exit(phdl, 0, NULL);
    if (rv == DDS_RETCODE_TIMEOUT) {
      /* Target process is still alive. Try to terminate it by
       * injecting a thread that will execute a ExitProcess(). */
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
        if (remoteHandle == 0) {
          rv = DDS_RETCODE_ERROR;
        } else {
          /* Wait for the process to exit. */
          waitmsec = INFINITE;
          if (timeout != DDS_INFINITY) {
            waitmsec = (DWORD)(timeout / DDS_NSECS_IN_MSEC);
          }
          waitres = WaitForSingleObject(phdl, waitmsec);
          if (waitres == 0) {
            rv = DDS_RETCODE_OK;
          } else if (waitres == WAIT_TIMEOUT) {
            rv = DDS_RETCODE_TIMEOUT;
          } else {
            rv = DDS_RETCODE_ERROR;
          }
          CloseHandle(remoteHandle);
        }
      } else {
        rv = DDS_RETCODE_ERROR;
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
process_kill(
  HANDLE phdl,
  dds_duration_t timeout)
{
  assert(phdl != 0);
  if (TerminateProcess(phdl, 1 /* exit code */) == 0) {
    return DDS_RETCODE_OK;
  }
  return DDS_RETCODE_ERROR;
}



/* Add quotes to a given string. */
static size_t
stringify_len(const char *str)
{
  return strlen(str) + 4; /* '\"' + str + '\"' + ' ' + '\0' */
}
static char*
stringify(char *buf, const char *str)
{
  size_t len = strlen(str);
  size_t idx = 0;
  buf[idx++] = '\"';
  if (len > 0) {
    memcpy(&(buf[idx]), str, len);
    idx += len;
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
    stringify(cmd, exe);
  } else {
    char *ptr;
    size_t argi;
    for (argi = 0; argv_in[argi] != NULL; argi++) {
      cmdlen += stringify_len(argv_in[argi]);
    }
    cmd = ddsrt_malloc(cmdlen);
    ptr = stringify(cmd, exe);
    for (argi = 0; argv_in[argi] != NULL; argi++) {
      ptr = stringify(ptr, argv_in[argi]);
    }
  }

  return cmd;
}

#endif /* DDSRT_USE_PROCESSCREATION */

