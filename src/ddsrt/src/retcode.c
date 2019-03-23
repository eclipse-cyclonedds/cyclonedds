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
#include "dds/ddsrt/retcode.h"

static const char *retcodes[] =
{
  "Success",
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
  "Illegal Operation",
  "Not Allowed By Security"
};

static const char *xretcodes[] = {
  "Operation in progress",
  "Try again",
  "Interrupted",
  "Not allowed",
  "Host not found",
  "Network not available",
  "Connection not available",
  "No space left",
  "Result too large",
  "Not found"
};

const char *
dds_strretcode (dds_retcode_t rc)
{
  if (rc >= 0 &&
      rc < (dds_retcode_t)(sizeof(retcodes) / sizeof(retcodes[0])))
  {
    return retcodes[rc];
  } else if (rc >= (DDS_XRETCODE_BASE) &&
             rc <  (dds_retcode_t)(DDS_XRETCODE_BASE + (sizeof(xretcodes) / sizeof(xretcodes[0]))))
  {
    return xretcodes[rc - DDS_XRETCODE_BASE];
  }

  return "Unknown return code";
}

