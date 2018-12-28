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

/** \brief OS layer initialization
 *
 * \b os_osInit calls:
 * - \b os_sharedMemoryInit
 * - \b os_threadInit
 */
void os_osPlatformInit (void)
{
  os_syncModuleInit();
  os_threadModuleInit();
}

/** \brief OS layer deinitialization
 */
void os_osPlatformExit (void)
{
  os_threadModuleExit();
  os_syncModuleExit();
}

/* This constructor is invoked when the library is loaded into a process. */
void __attribute__ ((constructor))
os__osInit(
        void)
{
    os_osInit();
}

/* This destructor is invoked when the library is unloaded from a process. */
void __attribute__ ((destructor))
os__osExit(
        void)
{
    os_osExit();
}
