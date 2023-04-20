// Copyright(c) 2006 to 2019 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include "dds/ddsrt/retcode.h"
#include "dds/ddsrt/static_assert.h"

static const char *retcodes[] = {
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
  "Unknown return code",
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

const char *dds_strretcode (dds_return_t rc)
{
  const dds_return_t nretcodes = (dds_return_t) (sizeof (retcodes) / sizeof (retcodes[0]));
  const dds_return_t nxretcodes = (dds_return_t) (sizeof (xretcodes) / sizeof (xretcodes[0]));
  DDSRT_STATIC_ASSERT (DDS_XRETCODE_BASE < 0);
  /* Retcodes used to be positive, but return values from the API would be a negative
     and so there are/were/may be places outside the core library where dds_strretcode
     is called with a -N for N a API return value, so ... play it safe and use the
     magnitude.  Specially handle INT32_MIN to avoid undefined behaviour on integer
     overflow. */
  if (rc == INT32_MIN)
    return xretcodes[0];

  if (rc < 0)
    rc = -rc;
  if (rc >= 0 && rc < nretcodes)
    return retcodes[rc];
  else if (rc >= (-DDS_XRETCODE_BASE) && rc < (-DDS_XRETCODE_BASE) + nxretcodes)
    return xretcodes[rc - (-DDS_XRETCODE_BASE)];
  else
    return xretcodes[0];
}
