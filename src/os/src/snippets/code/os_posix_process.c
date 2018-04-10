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
/** \file os/posix/code/os_process.c
 *  \brief Posix process management
 *
 * Implements process management for POSIX
 */

#include "os/os.h"

#include <sys/types.h>
#ifndef OSPL_NO_VMEM
#include <sys/mman.h>
#endif
#ifndef PIKEOS_POSIX
#include <sys/wait.h>
#endif
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <sched.h>
#ifndef INTEGRITY
#include <signal.h>
#endif
#include <stdio.h>
#include <pthread.h>

static os_atomic_voidp_t os_procname = OS_ATOMIC_VOIDP_INIT(0);

/** \brief pointer to environment variables */
#ifdef __APPLE__
//not available on iOS, but also not currently used
//#include <crt_externs.h>
#else
extern char **environ;
#endif

/* protected functions */
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
/* public functions */

/** \brief Return the process ID of the calling process
 *
 * Possible Results:
 * - returns the process ID of the calling process
 */
os_procId
os_procIdSelf(void)
{
    return getpid();
}

/* _OS_PROCESS_DEFAULT_CMDLINE_LEN_ is defined as
 * strlen("/proc/<max_pid>/cmdline" + 1, max_pid = 32768 on Linux, so a reason-
 * able default is 20 */
#define _OS_PROCESS_DEFAULT_CMDLINE_LEN_ (20)
#define _OS_PROCESS_PROCFS_PATH_FMT_     "/proc/%d/cmdline"
#define _OS_PROCESS_DEFAULT_NAME_LEN_    (512)

/** \brief Figure out the identity of the current process
 *
 * os_procNamePid determines the numeric, and if possible named
 * identity of a process. It will first check if the environment variable
 * SPLICE_PROCNAME is set (which is always the case if the process is started
 * via os_procCreate). If so, that value will be returned. Otherwise it will be
 * attempted to determine the commandline which started the process through the
 * procfs. If that fails, the PID will be returned.
 *
 * \param procIdentity  Pointer to a char-buffer to which the result can be
 *                      written. If a name could be resolved, the result will
 *                      have the format "name <PID>". Otherwise it will just
 *                      be "<PID>".
 * \param procIdentitySize Size of the buffer pointed to by procIdentitySize
 * \return same as snprintf returns
 */
int
os_procNamePid(
    char *procIdentity,
    size_t procIdentitySize)
{
    int size;
    char process_name[_OS_PROCESS_DEFAULT_NAME_LEN_];

    size = os_procName(process_name, _OS_PROCESS_DEFAULT_NAME_LEN_);
    if (size > 0) {
        size = snprintf(procIdentity, procIdentitySize, "%s <%"PRIprocId">", (char *)os_atomic_ldvoidp(&os_procname), os_procIdSelf());
    } else {
        /* No processname could be determined, so default to PID */
        size = snprintf(procIdentity, procIdentitySize, "<%"PRIprocId">", os_procIdSelf());
    }
    return size;
}

#undef _OS_PROCESS_DEFAULT_CMDLINE_LEN_
#undef _OS_PROCESS_PROCFS_PATH_FMT_
#undef _OS_PROCESS_DEFAULT_NAME_LEN_
