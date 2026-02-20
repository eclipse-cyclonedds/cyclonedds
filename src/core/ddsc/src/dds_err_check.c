// Copyright(c) 2026 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include "dds/dds.h"

DDSRT_WARNING_DEPRECATED_OFF

static void dds_fail_default (const char * msg, const char * where)
{
  fprintf (stderr, "Aborting Failure: %s %s\n", where, msg);
  abort ();
}

static dds_fail_fn dds_fail_func = dds_fail_default;

void dds_fail_set (dds_fail_fn fn)
{
  dds_fail_func = fn;
}

dds_fail_fn dds_fail_get (void)
{
  return dds_fail_func;
}

void dds_fail (const char * msg, const char * where)
{
  if (dds_fail_func)
  {
    (dds_fail_func) (msg, where);
  }
}

const char * dds_err_str (dds_return_t err)
{
  return dds_strretcode (err);
}

bool dds_err_check (dds_return_t err, unsigned flags, const char * where)
{
  if (err < 0)
  {
    if (flags & (DDS_CHECK_REPORT | DDS_CHECK_FAIL))
    {
      char msg[256];
      (void) snprintf (msg, sizeof (msg), "Error %s", dds_strretcode (err));
      if (flags & DDS_CHECK_REPORT)
      {
        printf ("%s: %s\n", where, msg);
      }
      if (flags & DDS_CHECK_FAIL)
      {
        dds_fail (msg, where);
      }
    }
    if (flags & DDS_CHECK_EXIT)
    {
      exit (-1);
    }
  }
  return (err >= 0);
}
