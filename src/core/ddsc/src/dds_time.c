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
#include "ddsc/dds.h"
#include "os/os.h"

dds_time_t dds_time (void)
{
  os_time time;
  
  /* Get the current time */
  time = os_timeGet ();

  /* convert os_time to dds_time_t */
  dds_time_t dds_time = time.tv_nsec + (time.tv_sec * DDS_NSECS_IN_SEC);

  return dds_time;
}
 
void dds_sleepfor (dds_duration_t n)
{
  os_time interval = { (os_timeSec) (n / DDS_NSECS_IN_SEC), (int32_t) (n % DDS_NSECS_IN_SEC) };
  os_nanoSleep (interval);
}

void dds_sleepuntil (dds_time_t n)
{
  dds_time_t interval = n - dds_time ();
  if (interval > 0)
  {
    dds_sleepfor (interval);
  }
}
