// Copyright(c) 2006 to 2019 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <string.h>

#include "CUnit/Theory.h"
#include "dds/ddsrt/retcode.h"

CU_TheoryDataPoints(ddsrt_retcode, unknown) = {
  CU_DataPoints(dds_return_t,
                DDS_RETCODE_NOT_ALLOWED_BY_SECURITY-1,
                -(DDS_RETCODE_NOT_ALLOWED_BY_SECURITY-1),
                DDS_XRETCODE_BASE,
                -DDS_XRETCODE_BASE,
                DDS_RETCODE_NOT_FOUND-1,
                -(DDS_RETCODE_NOT_FOUND-1),
                INT32_MAX,
                -INT32_MAX,
                INT32_MIN)
};

CU_Theory((dds_return_t ret), ddsrt_retcode, unknown)
{
  CU_ASSERT_STRING_EQUAL(dds_strretcode(ret), "Unknown return code");
}

CU_TheoryDataPoints(ddsrt_retcode, spotchecks) = {
  CU_DataPoints(dds_return_t,
                DDS_RETCODE_OK,
                -DDS_RETCODE_OK,
                DDS_RETCODE_NOT_ALLOWED_BY_SECURITY,
                -DDS_RETCODE_NOT_ALLOWED_BY_SECURITY,
                DDS_RETCODE_IN_PROGRESS,
                -DDS_RETCODE_IN_PROGRESS,
                DDS_RETCODE_NOT_FOUND,
                -DDS_RETCODE_NOT_FOUND),
  CU_DataPoints(const char *,
                "Success",
                "Success",
                "Not Allowed By Security",
                "Not Allowed By Security",
                "Operation in progress",
                "Operation in progress",
                "Not found",
                "Not found")
};

CU_Theory((dds_return_t ret, const char *exp), ddsrt_retcode, spotchecks)
{
  CU_ASSERT_STRING_EQUAL(dds_strretcode(ret), exp);
}
