// Copyright(c) 2026 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include "dds/dds.h"
#include "dds/ddsrt/string.h"
#include "test_common.h"

DDSRT_WARNING_DEPRECATED_OFF

static unsigned fail_count;
static char fail_msg[1000];
static const char *fail_where;

static void test_fail_fn (const char *msg, const char *where)
{
  fail_count++;
  ddsrt_strlcpy (fail_msg, msg, sizeof (fail_msg));
  fail_where = where;
}

CU_Test(ddsc_err_check, deprecated_compatibility)
{
  dds_fail_fn const old_fail_fn = dds_fail_get ();

  fail_count = 0;
  fail_msg[0] = 0;
  fail_where = NULL;

  CU_ASSERT_NEQ (old_fail_fn, NULL);
  CU_ASSERT_STREQ (dds_err_str (DDS_RETCODE_OK), "Success");
  CU_ASSERT_STREQ (dds_err_str (DDS_RETCODE_BAD_PARAMETER), "Bad Parameter");
  CU_ASSERT (dds_err_check (DDS_RETCODE_OK, DDS_CHECK_REPORT | DDS_CHECK_FAIL, "ok"));
  CU_ASSERT (DDS_ERR_CHECK (DDS_RETCODE_OK, DDS_CHECK_REPORT));
  CU_ASSERT (!dds_err_check (DDS_RETCODE_BAD_PARAMETER, 0, "unchecked"));

  dds_fail_set (test_fail_fn);
  CU_ASSERT (dds_fail_get () == test_fail_fn);
  dds_fail ("direct failure", "direct");
  CU_ASSERT_EQ (fail_count, 1);
  CU_ASSERT_STREQ (fail_msg, "direct failure");
  CU_ASSERT_STREQ (fail_where, "direct");

  CU_ASSERT (!dds_err_check (DDS_RETCODE_BAD_PARAMETER, DDS_CHECK_REPORT | DDS_CHECK_FAIL, "checked"));
  CU_ASSERT_EQ (fail_count, 2);
  CU_ASSERT_STREQ (fail_msg, "Error Bad Parameter");
  CU_ASSERT_STREQ (fail_where, "checked");

  dds_fail_set (NULL);
  CU_ASSERT_EQ (dds_fail_get (), NULL);
  dds_fail ("ignored", "nowhere");
  CU_ASSERT_EQ (fail_count, 2);

  dds_fail_set (old_fail_fn);
}
