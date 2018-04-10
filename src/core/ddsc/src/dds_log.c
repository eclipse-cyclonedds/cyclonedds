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
#include <assert.h>
#include <string.h>
#include <stdarg.h>
#include "ddsc/dds.h"
#include "ddsi/q_log.h"

#define DDS_FMT_MAX 128

void dds_log_info (const char * fmt, ...)
{
  va_list args;

  va_start (args, fmt);
  nn_vlog (LC_INFO, fmt, args);
  va_end (args);
}

void dds_log_warn (const char * fmt, ...)
{
  va_list args;
  char fmt2 [DDS_FMT_MAX];

  strcpy (fmt2, "<Warning> ");
  strncat (fmt2, fmt, DDS_FMT_MAX - 11);
  fmt2[DDS_FMT_MAX-1] = 0;
  fmt = fmt2;

  va_start (args, fmt);
  nn_vlog (LC_WARNING, fmt, args);
  va_end (args);
}

void dds_log_error (const char * fmt, ...)
{
  va_list args;
  char fmt2 [DDS_FMT_MAX];

  strcpy (fmt2, "<Error> ");
  strncat (fmt2, fmt, DDS_FMT_MAX - 9);
  fmt2[DDS_FMT_MAX-1] = 0;
  fmt = fmt2;

  va_start (args, fmt);
  nn_vlog (LC_ERROR, fmt, args);
  va_end (args);
}

void dds_log_fatal (const char * fmt, ...)
{
  va_list args;
  char fmt2 [DDS_FMT_MAX];

  strcpy (fmt2, "<Fatal> ");
  strncat (fmt2, fmt, DDS_FMT_MAX - 9);
  fmt2[DDS_FMT_MAX-1] = 0;
  fmt = fmt2;

  va_start (args, fmt);
  nn_vlog (LC_FATAL, fmt, args);
  va_end (args);
  DDS_FAIL (fmt);
}
