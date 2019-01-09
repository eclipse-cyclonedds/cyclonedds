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

/** \file os/code/os_init.c
 *  \brief Initialization / Deinitialization
 *
 * Initialization / Deinitialization provides routines for
 * initializing the OS layer claiming required resources
 * and routines to deinitialize the OS layer, releasing
 * all resources still claimed.
 */

#include "os/os.h"

#define OSINIT_STATUS_OK 0x80000000u
static os_atomic_uint32_t osinit_status = OS_ATOMIC_UINT32_INIT(0);
static os_mutex init_mutex;

void os_osInit (void)
{
  uint32_t v;
  v = os_atomic_inc32_nv(&osinit_status);
retry:
  if (v > OSINIT_STATUS_OK)
    return;
  else if (v == 1) {
    os_osPlatformInit();
    os_mutexInit(&init_mutex);
    os_atomic_or32(&osinit_status, OSINIT_STATUS_OK);
  } else {
    while (v > 1 && !(v & OSINIT_STATUS_OK)) {
      os_nanoSleep((os_time){0, 10000000});
      v = os_atomic_ld32(&osinit_status);
    }
    goto retry;
  }
}

void os_osExit (void)
{
  uint32_t v, nv;
  do {
    v = os_atomic_ld32(&osinit_status);
    if (v == (OSINIT_STATUS_OK | 1)) {
      nv = 1;
    } else {
      nv = v - 1;
    }
  } while (!os_atomic_cas32(&osinit_status, v, nv));
  if (nv == 1)
  {
    os_mutexDestroy(&init_mutex);
    os_osPlatformExit();
    os_atomic_dec32(&osinit_status);
  }
}

os_mutex *os_getSingletonMutex(void)
{
  return &init_mutex;
}


