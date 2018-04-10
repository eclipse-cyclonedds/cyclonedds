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
/** \file os/win32/code/os_process.c
 *  \brief WIN32 process management
 *
 * Implements process management for WIN32
 */
#include "os/os.h"

#include <process.h>
#include <assert.h>
#include <stdlib.h>

/* #642 fix : define mapping between scheduling abstraction and windows
 * Windows provides 6 scheduling classes for the process
 * IDLE_PRIORITY_CLASS
 * BELOW_NORMAL_PRIORITY_CLASS
 * NORMAL_PRIORITY_CLASS
 * ABOVE_NORMAL_PRIORITY_CLASS
 * HIGH_PRIORITY_CLASS
 * REALTIME_PRIORITY_CLASS */

/* These defaults should be modifiable through configuration */
static const os_schedClass TIMESHARE_DEFAULT_SCHED_CLASS = NORMAL_PRIORITY_CLASS;
static const os_schedClass REALTIME_DEFAULT_SCHED_CLASS  = REALTIME_PRIORITY_CLASS;

static os_atomic_voidp_t os_procname = OS_ATOMIC_VOIDP_INIT(0);

/* Protected functions */
void
os_processModuleInit(void)
{
    return;
}

void
os_processModuleExit(void)
{
    void *pname;
    do {
        pname = os_atomic_ldvoidp(&os_procname);
    } while (!os_atomic_casvoidp(&os_procname, pname, NULL));
    os_free(pname);
}

/** \brief Return the process ID of the calling process
 *
 * Possible Results:
 * - returns the process ID of the calling process
 */
os_procId
os_procIdSelf(void)
{
   /* returns a pseudo HANDLE to process, no need to close it */
   return GetProcessId (GetCurrentProcess());
}

/** \brief Figure out the identity of the current process
 *
 * Possible Results:
 * - returns the actual length of procIdentity
 *
 * Postcondition:
 * - \b procIdentity is ""
 *     the process identity could not be determined
 * - \b procIdentity is "<decimal number>"
 *     only the process numeric identity could be determined
 * - \b procIdentity is "name <pid>"
 *     the process name and numeric identity could be determined
 *
 * \b procIdentity will not be filled beyond the specified \b procIdentitySize
 */
#define _OS_PROC_PROCES_NAME_LEN (512)
int
os_procNamePid(
    _Out_writes_z_(procIdentitySize) char *procIdentity,
    _In_ size_t procIdentitySize)
{
    int size;
    char process_name[_OS_PROC_PROCES_NAME_LEN];

    size = os_procName(process_name, sizeof(process_name));

    if (size > 0) {
        size = snprintf(procIdentity, procIdentitySize, "%s <%"PRIprocId">", process_name, os_procIdSelf());
    } else {
        /* No processname could be determined, so default to PID */
        size = snprintf(procIdentity, procIdentitySize, "<%"PRIprocId">", os_procIdSelf());
    }

    return size;
}

int
os_procName(
    _Out_writes_z_(procNameSize) char *procName,
    _In_ size_t procNameSize)
{
    char *process_name;

    if ((process_name = os_atomic_ldvoidp(&os_procname)) == NULL) {
        char *exec, *pname;
        DWORD nSize, allocated = 0;

        do {
            /* While procNameSize could be used (since the caller cannot
             * store more data anyway, it is not used. This way the amount that
             * needs to be allocated to get the full-name can be determined. */
            allocated++;
            process_name = os_realloc(process_name, allocated * _OS_PROC_PROCES_NAME_LEN);
            /* First parameter NULL retrieves module-name of executable */
            nSize = GetModuleFileNameA(NULL, process_name, allocated * _OS_PROC_PROCES_NAME_LEN);

            /* process_name will only be guaranteed to be NULL-terminated if nSize <
             * (allocated * _OS_PROC_PROCES_NAME_LEN), so continue until that's true */
        } while (nSize >= (allocated * _OS_PROC_PROCES_NAME_LEN));

        exec = strrchr(process_name, '\\');
        if (exec) {
            /* skip all before the last '\' */
            exec++;
            memmove(process_name, exec, strlen(exec) + 1);
            /* Could potentially realloc; can't be bothered */
        }
        do {
            pname = os_atomic_ldvoidp(&os_procname);
        } while (pname == NULL && !os_atomic_casvoidp(&os_procname, NULL, process_name));
        if(pname) {
            os_free(process_name);
            process_name = pname;
        }
    }
    return snprintf(procName, procNameSize, "%s", process_name);
}
#undef _OS_PROC_PROCES_NAME_LEN
