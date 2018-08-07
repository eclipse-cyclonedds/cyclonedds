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
#include <stdlib.h>
#include "os/os.h"
#include "dds__types.h"
#include "dds__err.h"

#define DDS_ERR_CODE_NUM 12
#define DDS_ERR_MSG_MAX 128

#define DDS_ERR_NR_INDEX(e) (((-e) & DDS_ERR_NR_MASK) -1)

static const char * dds_err_code_array[DDS_ERR_CODE_NUM] =
{
  "Error",
  "Unsupported",
  "Bad Parameter",
  "Precondition Not Met",
  "Out Of Resources",
  "Not Enabled",
  "Immutable Policy",
  "Inconsistent Policy",
  "Already Deleted",
  "Timeout",
  "No Data",
  "Illegal Operation"
};

const char * dds_err_str (dds_return_t err)
{
  unsigned index = (unsigned)DDS_ERR_NR_INDEX (err);
  if (err >= 0)
  {
    return "Success";
  }
  if (index >= DDS_ERR_CODE_NUM)
  {
    return "Unknown";
  }
  return dds_err_code_array[index];
}

bool dds_err_check (dds_return_t err, unsigned flags, const char * where)
{
  if (err < 0)
  {
    if (flags & (DDS_CHECK_REPORT | DDS_CHECK_FAIL))
    {
      char msg[DDS_ERR_MSG_MAX];
      (void) snprintf (msg, DDS_ERR_MSG_MAX, "Error %d:M%d:%s", dds_err_file_id(err), dds_err_line(err), dds_err_str(err));
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
