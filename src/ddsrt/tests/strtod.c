// Copyright(c) 2026 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <errno.h>
#include <math.h>
#include <string.h>

#include "CUnit/Test.h"
#include "dds/ddsrt/strtod.h"

CU_Test(ddsrt_strtod, parse)
{
  char *end = NULL;
  double value = 0.0;
  errno = EDOM;

  CU_ASSERT_EQ (ddsrt_strtod ("  -12.5tail", &end, &value), DDS_RETCODE_OK);
  CU_ASSERT (value == -12.5);
  CU_ASSERT_NEQ (end, NULL);
  CU_ASSERT_STREQ (end, "tail");
  CU_ASSERT_EQ (errno, EDOM);

  value = 0.0;
  end = NULL;
  CU_ASSERT_EQ (ddsrt_strtod ("0x1.8p+1!", &end, &value), DDS_RETCODE_OK);
  CU_ASSERT (value == 3.0);
  CU_ASSERT_NEQ (end, NULL);
  CU_ASSERT_STREQ (end, "!");

  value = 1.0;
  CU_ASSERT_EQ (ddsrt_strtod ("42", NULL, &value), DDS_RETCODE_OK);
  CU_ASSERT (value == 42.0);
}

CU_Test(ddsrt_strtod, reject)
{
  char *end = NULL;
  double value = 1.0;

  CU_ASSERT_EQ (ddsrt_strtod ("junk", &end, &value), DDS_RETCODE_OUT_OF_RANGE);
  CU_ASSERT_NEQ (end, NULL);
  CU_ASSERT_STREQ (end, "junk");

  CU_ASSERT_EQ (ddsrt_strtod ("nan", &end, &value), DDS_RETCODE_OUT_OF_RANGE);
  CU_ASSERT (isnan (value));

  CU_ASSERT_EQ (ddsrt_strtod ("inf", &end, &value), DDS_RETCODE_OUT_OF_RANGE);
  CU_ASSERT (isinf (value));

  CU_ASSERT_EQ (ddsrt_strtod ("1e9999", &end, &value), DDS_RETCODE_OUT_OF_RANGE);
  CU_ASSERT (isinf (value));
}

CU_Test(ddsrt_strtod, print)
{
  char buf[32];
  int n;

  n = ddsrt_dtostr (1.25, buf, sizeof (buf));
  CU_ASSERT (n > 0);
  CU_ASSERT_STREQ (buf, "1.25");

  n = ddsrt_ftostr (1.25f, buf, sizeof (buf));
  CU_ASSERT (n > 0);
  CU_ASSERT_STREQ (buf, "1.25");
}
