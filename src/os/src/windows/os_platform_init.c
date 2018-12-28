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
/****************************************************************
 * Initialization / Deinitialization                            *
 ****************************************************************/

/** \file os/darwin/code/os_init.c
 *  \brief Initialization / Deinitialization
 */

#include <assert.h>

#include "os/os.h"

void os_osPlatformInit (void)
{
  os_threadModuleInit();
  os_timeModuleInit();
  os_socketModuleInit();
}

void os_osPlatformExit (void)
{
  os_socketModuleExit();
  os_timeModuleExit();
  os_threadModuleExit();
}

/* We need this on windows to make sure the main thread of MFC applications
 * calls os_osInit().
 */
BOOL WINAPI DllMain(
    HINSTANCE hinstDLL,  /* handle to DLL module */
    DWORD fdwReason,     /* reason for calling function */
    LPVOID lpReserved )  /* reserved */
{
    /* Perform actions based on the reason for calling.*/
    switch( fdwReason ) {
    case DLL_PROCESS_ATTACH:
        /* Initialize once for each new process.
         * Return FALSE to fail DLL load.
         */
        os_osInit();
    break;
    case DLL_THREAD_ATTACH:
         /* Do thread-specific initialization.
          */
    break;
    case DLL_THREAD_DETACH:
         /* Do thread-specific cleanup.
          */
    break;
    case DLL_PROCESS_DETACH:
        /* Perform any necessary cleanup.
         */
        os_osExit();
    break;
    }
    return TRUE;  /* Successful DLL_PROCESS_ATTACH.*/
}
